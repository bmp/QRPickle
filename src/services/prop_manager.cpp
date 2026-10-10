#include "prop_manager.h"
#include "net_lock.h"
#include "safe_client.h"
#include "wifi_manager.h"
#include "../config/config.h"
#include "../core/crashlog.h"
#include "../core/metadata.h"
#include "../hw/led_rgb.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <atomic>
#include <cstring>
#include <time.h>

namespace services {

    PropagationTelemetry PropagationManager::data;

    static constexpr const char* DEFAULT_URL = "https://www.hamqsl.com/solarxml.php";
    static constexpr size_t MAX_BODY = 4096;  // the feed is ~1.7 KB
    static constexpr uint32_t MIN_VALID_UTC = 1700000000;  // before this the clock isn't set (NTP)

    // Fetch task -> main loop hand-over.
    static portMUX_TYPE result_mux = portMUX_INITIALIZER_UNLOCKED;
    static std::atomic<bool> fetching{false};
    static bool result_ready = false;
    static bool result_ok = false;
    static solar::SolarData result_data;

    static solar::Schedule schedule = {0, 0, false};

    void PropagationManager::init() {
        memset(&data, 0, sizeof(data));
        solar::parse_xml(nullptr, data.solar);  // all fields "none"
        data.dirty = true;
    }

    static void log_result(bool ok, const solar::SolarData& d, int http_code, bool own_source) {
        // The own source URL is never logged (it may carry a token).
        const char* src = own_source ? "own source" : "hamqsl.com";
        if (!ok) {
            Serial.printf("[SOLAR] Fetch from %s failed (HTTP %d), largest block %u B\n", src, http_code,
                          (unsigned)ESP.getMaxAllocHeap());
            return;
        }
        Serial.printf("[SOLAR] %s: SFI %d SSN %d A %d K %d X-ray %s wind %.0f Bz %.1f geomag %s%s\n", src, d.sfi,
                      d.sunspots, d.a_index, d.k_index, d.xray[0] ? d.xray : "-", d.solar_wind, d.bz,
                      d.geomag[0] ? d.geomag : "-", solar::is_storm(d) ? " (storm mode)" : "");
    }

    void PropagationManager::fetch_task(void*) {
        crashlog::mark(crashlog::SLOT_SOLAR, 1);
        const char* own = config::get().solar_url;
        char url[sizeof(config::Config::solar_url)];
        strlcpy(url, own[0] ? own : DEFAULT_URL, sizeof(url));
        const bool own_source = own[0] != '\0';

        bool ok = false;
        int code = 0;
        solar::SolarData parsed;
        {
            NetLock lock;
            if (lock.held()) {
                crashlog::mark(crashlog::SLOT_SOLAR, 2);
                SafeClient plain;  // thread-safe DNS (safe_client.h)
                SafeTlsClient tls;
                tls.setInsecure();  // public data; the parser rejects anything that isn't solarxml
                const bool https = strncmp(url, "https://", 8) == 0;
                HTTPClient http;
                http.useHTTP10(true);
                if (https ? http.begin(tls, url) : http.begin(plain, url)) {
                    char ua[40];
                    snprintf(ua, sizeof(ua), "%s/%s", meta::FW_NAME, meta::FW_VERSION);
                    http.setUserAgent(ua);
                    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
                    http.setRedirectLimit(3);
                    http.setTimeout(10000);
                    crashlog::mark(crashlog::SLOT_SOLAR, 3);
                    code = http.GET();
                    if (code == HTTP_CODE_OK) {
                        char* body = static_cast<char*>(malloc(MAX_BODY));
                        if (body) {
                            crashlog::mark(crashlog::SLOT_SOLAR, 4);
                            WiFiClient* s = http.getStreamPtr();
                            size_t n = 0;
                            const uint32_t start = millis();
                            while (n < MAX_BODY - 1 && millis() - start < 10000) {
                                if (s->available()) {
                                    int c = s->readBytes(body + n, MAX_BODY - 1 - n);
                                    if (c > 0) n += (size_t)c;
                                } else if (!s->connected()) {
                                    break;
                                } else {
                                    vTaskDelay(pdMS_TO_TICKS(10));
                                }
                            }
                            body[n] = '\0';
                            ok = solar::parse_xml(body, parsed);
                            free(body);
                        }
                    }
                    http.end();
                }
            }
        }
        crashlog::mark(crashlog::SLOT_SOLAR, 5);
        log_result(ok, parsed, code, own_source);
        if (ok) hw::led_rgb::trigger_traffic_pulse();  // data ingress (docs/LEDColours.md)

        portENTER_CRITICAL(&result_mux);
        result_ok = ok;
        if (ok) result_data = parsed;
        result_ready = true;
        portEXIT_CRITICAL(&result_mux);
        fetching = false;
        crashlog::mark(crashlog::SLOT_SOLAR, 6);
        vTaskDelete(NULL);
    }

    bool PropagationManager::start_fetch(uint32_t now_utc) {
        bool expected = false;
        if (!fetching.compare_exchange_strong(expected, true)) return false;
        schedule.last_attempt_utc = now_utc;
        // 8 KB like the POTA fetch (insecure TLS, no certificate verification).
        if (xTaskCreatePinnedToCore(fetch_task, "solar_fetch", 8192, NULL, 1, NULL, 0) != pdPASS) {
            Serial.println("[SOLAR] Task creation failed (heap).");
            fetching = false;
            return false;
        }
        return true;
    }

    void PropagationManager::update() {
        const uint32_t now = (uint32_t)time(nullptr);

        bool ready = false, ok = false;
        portENTER_CRITICAL(&result_mux);
        if (result_ready) {
            ready = true;
            ok = result_ok;
            if (ok) data.solar = result_data;
            result_ready = false;
        }
        portEXIT_CRITICAL(&result_mux);
        if (ready && ok) {
            const auto& d = data.solar;
            data.has_data = true;
            data.fetched_utc = now;
            data.sfi = (uint16_t)d.sfi;
            data.k_index = (uint8_t)d.k_index;
            data.a_index = d.a_index == solar::NONE ? 0 : (uint8_t)d.a_index;
            strlcpy(data.forecast, d.geomag, sizeof(data.forecast));
            data.dirty = true;
            schedule.last_ok_utc = now;
            schedule.storm = solar::is_storm(d);
        }

        if (fetching || now < MIN_VALID_UTC || !wifi_manager_is_connected()) return;
        if (solar::fetch_due(schedule, now)) start_fetch(now);
    }

    bool PropagationManager::refresh_now() {
        const uint32_t now = (uint32_t)time(nullptr);
        if (fetching || now < MIN_VALID_UTC || !wifi_manager_is_connected()) return false;
        if (!solar::manual_allowed(schedule, now)) return false;
        return start_fetch(now);
    }

    ConditionRating PropagationManager::get_band_rating(uint8_t band_group_idx, bool look_at_nighttime) {
        // K-Index >= 5 indicates a geomagnetic storm, wiping out most HF propagation
        if (data.k_index >= 5) return RATING_POOR;

        ConditionRating rating = RATING_POOR;

        switch (band_group_idx) {
            case 0: // 80m-40m (Lower HF: Heavily dependent on K-Index)
                if (look_at_nighttime) {
                    if (data.k_index <= 2) rating = RATING_GOOD;
                    else if (data.k_index <= 4) rating = RATING_FAIR;
                } else {
                    // Daytime absorption is high, but FAIR is possible with low noise
                    if (data.k_index <= 3) rating = RATING_FAIR;
                }
                break;

            case 1: // 30m-20m (Mid HF: The reliable workhorses)
                if (!look_at_nighttime) {
                    if (data.sfi >= 90) rating = RATING_GOOD;
                    else if (data.sfi >= 70) rating = RATING_FAIR;
                } else {
                    if (data.sfi >= 100 && data.k_index <= 2) rating = RATING_GOOD;
                    else if (data.sfi >= 80 && data.k_index <= 3) rating = RATING_FAIR;
                }
                break;

            case 2: // 17m-15m (Upper HF: Needs solid SFI)
                if (!look_at_nighttime) {
                    if (data.sfi >= 95) rating = RATING_GOOD;
                    else if (data.sfi >= 80) rating = RATING_FAIR;
                } else {
                    if (data.sfi >= 120) rating = RATING_GOOD;
                    else if (data.sfi >= 95) rating = RATING_FAIR;
                }
                break;

            case 3: // 12m-10m (Highest HF: Needs excellent SFI)
                if (!look_at_nighttime) {
                    if (data.sfi >= 110) rating = RATING_GOOD;
                    else if (data.sfi >= 90) rating = RATING_FAIR;
                } else {
                    if (data.sfi >= 150) rating = RATING_GOOD;
                    else if (data.sfi >= 120) rating = RATING_FAIR;
                }
                break;
        }

        // Apply a general penalty: K-Index of 4 (Minor disturbances) degrades marginal bands
        if (data.k_index == 4 && rating > RATING_POOR) {
            rating = (ConditionRating)(rating - 1);
        }

        return rating;
    }

    const PropagationTelemetry& PropagationManager::get_telemetry() { return data; }
    void PropagationManager::clear_dirty() { data.dirty = false; }

} // namespace services
