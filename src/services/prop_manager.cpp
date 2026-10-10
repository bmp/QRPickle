#include "prop_manager.h"
#include "net_lock.h"
#include "safe_client.h"
#include "wifi_manager.h"
#include "aprs_manager.h"
#include "hamalert_manager.h"
#include "../config/config.h"
#include "../core/crashlog.h"
#include "../core/metadata.h"
#include "../hw/led_rgb.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <atomic>
#include <cstring>
#include <esp_attr.h>
#include <time.h>

namespace services {

    PropagationTelemetry PropagationManager::data;
    BandView PropagationManager::view;

    static constexpr const char* DEFAULT_URL = "https://www.hamqsl.com/solarxml.php";
    static constexpr size_t MAX_BODY = 4096;  // the feed is ~1.7 KB
    static constexpr uint32_t MIN_VALID_UTC = 1700000000;  // before this the clock isn't set (NTP)
    static constexpr uint32_t CACHE_MAX_AGE_S = 6 * 3600;  // older cached data isn't shown after a reset

    // Fetch task -> main loop hand-over.
    static portMUX_TYPE result_mux = portMUX_INITIALIZER_UNLOCKED;
    static std::atomic<bool> fetching{false};
    static bool result_ready = false;
    static bool result_ok = false;
    static bool result_own = false;
    static solar::SolarData result_data;

    static solar::Schedule schedule = {0, 0, false};

    // The last result survives software and watchdog resets (not power-off), so a reboot within the
    // hour neither refetches (hamqsl.com asks for at most hourly) nor shows "--".
    struct SolarCache {
        uint32_t magic;
        uint32_t fetched_utc;
        bool own_source;
        solar::SolarData data;
        uint32_t check;
    };
    RTC_NOINIT_ATTR static SolarCache rtc_cache;
    static constexpr uint32_t CACHE_MAGIC = 0x50524F50;  // "PROP"

    static uint32_t cache_check(const SolarCache& c) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&c);
        uint32_t h = 2166136261u;  // FNV-1a over everything before `check`
        for (size_t i = 0; i < offsetof(SolarCache, check); i++) h = (h ^ p[i]) * 16777619u;
        return h;
    }

    // View inputs that change it besides the data: location and the minute.
    static float last_lat = 1000, last_lon = 1000;
    static uint32_t last_view_minute = 0;
    static bool cache_checked = false;

    void PropagationManager::init() {
        memset(&data, 0, sizeof(data));
        solar::parse_xml(nullptr, data.solar);  // all fields "none"
        memset(&view, 0, sizeof(view));
        rebuild_view(0);
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
        const bool https = strncmp(url, "https://", 8) == 0;

        // Quiet window (as the Cloud OTA check): the TLS handshake needs ~40 KB of contiguous heap,
        // which HamAlert/APRS and open screens can leave short ("start_ssl_client: -1"). Pause them
        // for the few seconds of the fetch, then resume the ones that were running.
        const bool ham_was = https && !HamAlertManager::is_stopped();
        const bool aprs_was = https && !AprsManager::is_stopped();
        if (https) {
            HamAlertManager::stop();
            AprsManager::stop();
            for (int i = 0; i < 80 && !(HamAlertManager::is_stopped() && AprsManager::is_stopped()); i++) {
                vTaskDelay(pdMS_TO_TICKS(100));  // stop() only requests the exit
            }
        }

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
        if (ham_was) HamAlertManager::start();
        if (aprs_was) AprsManager::start();
        log_result(ok, parsed, code, own_source);
        if (ok) hw::led_rgb::trigger_traffic_pulse();  // data ingress (docs/LEDColours.md)

        portENTER_CRITICAL(&result_mux);
        result_ok = ok;
        result_own = own_source;
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

    static void apply(PropagationTelemetry& data, solar::Schedule& schedule, const solar::SolarData& d,
                      uint32_t fetched_utc, bool own_source) {
        data.solar = d;
        data.has_data = true;
        data.own_source = own_source;
        data.fetched_utc = fetched_utc;
        schedule.last_ok_utc = fetched_utc;
        schedule.storm = solar::is_storm(d);
    }

    void PropagationManager::rebuild_view(uint32_t now) {
        const auto& cfg = config::get();
        float lat, lon;
        view.location_from_grid = sun::station_location(cfg.latlon_set, cfg.lat, cfg.lon, cfg.grid, lat, lon);
        const bool moved = lat != last_lat || lon != last_lon;
        last_lat = lat;
        last_lon = lon;

        view.time_valid = now >= MIN_VALID_UTC;
        uint8_t month = 1;
        if (view.time_valid) {
            time_t t = now;
            struct tm tm_utc;
            gmtime_r(&t, &tm_utc);
            month = (uint8_t)(tm_utc.tm_mon + 1);
            view.light = sun::light_at(lat, lon, now);
            // The search costs ~100 ms of software double maths: only when the event has passed.
            if (moved || !view.next_sun_utc || now >= view.next_sun_utc) {
                view.next_sun_utc = sun::next_sun_event(lat, lon, now, view.next_rising);
            }
        }

        const auto& d = data.solar;
        const bands::Inputs in = {data.has_data ? d.sfi : (int16_t)0, data.has_data ? d.k_index : (int16_t)-1,
                                  d.a_index, lat, month};
        for (int i = 0; i < bands::BAND_COUNT; i++) {
            view.band_day[i] = bands::rate_band(i, in, false);
            view.band_night[i] = bands::rate_band(i, in, true);
        }
        for (int g = 0; g < bands::GROUP_COUNT; g++) {
            view.group_day[g] = bands::rate_group(g, in, false);
            view.group_night[g] = bands::rate_group(g, in, true);
        }
        view.storm = data.has_data && solar::is_storm(d);
        view.version++;
    }

    void PropagationManager::update() {
        const uint32_t now = (uint32_t)time(nullptr);
        bool changed = false;

        // After a reset: take the cached result once the clock is set.
        if (!cache_checked && now >= MIN_VALID_UTC) {
            cache_checked = true;
            if (rtc_cache.magic == CACHE_MAGIC && rtc_cache.check == cache_check(rtc_cache) &&
                rtc_cache.fetched_utc <= now && now - rtc_cache.fetched_utc < CACHE_MAX_AGE_S) {
                apply(data, schedule, rtc_cache.data, rtc_cache.fetched_utc, rtc_cache.own_source);
                Serial.printf("[SOLAR] Kept data from %lu min ago across the reset\n",
                              (unsigned long)((now - rtc_cache.fetched_utc) / 60));
                changed = true;
            }
        }

        bool ready = false, ok = false, own = false;
        solar::SolarData fresh;
        portENTER_CRITICAL(&result_mux);
        if (result_ready) {
            ready = true;
            ok = result_ok;
            own = result_own;
            if (ok) fresh = result_data;
            result_ready = false;
        }
        portEXIT_CRITICAL(&result_mux);
        if (ready && !ok) {
            data.failed_utc = now;
            changed = true;
        }
        if (ready && ok) {
            data.failed_utc = 0;
            apply(data, schedule, fresh, now, own);
            rtc_cache.magic = CACHE_MAGIC;
            rtc_cache.fetched_utc = now;
            rtc_cache.own_source = own;
            rtc_cache.data = fresh;
            rtc_cache.check = cache_check(rtc_cache);
            changed = true;
        }

        // Location (settings) or the minute changed: day/night and the data age move on.
        const auto& cfg = config::get();
        float lat, lon;
        sun::station_location(cfg.latlon_set, cfg.lat, cfg.lon, cfg.grid, lat, lon);
        if (lat != last_lat || lon != last_lon || now / 60 != last_view_minute) changed = true;
        if (changed) {
            last_view_minute = now / 60;
            rebuild_view(now);
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

    uint32_t PropagationManager::refresh_unlock_utc() {
        return solar::manual_unlock_utc(schedule, (uint32_t)time(nullptr));
    }

    uint32_t PropagationManager::next_retry_utc() {
        return data.failed_utc ? schedule.last_attempt_utc + solar::RETRY_S : 0;
    }

    bool PropagationManager::is_fetching() { return fetching; }
    const PropagationTelemetry& PropagationManager::get_telemetry() { return data; }
    const BandView& PropagationManager::get_view() { return view; }

}  // namespace services
