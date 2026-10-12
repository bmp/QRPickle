#include "space_manager.h"
#include "net_lock.h"
#include "quiet_window.h"
#include "safe_client.h"
#include "sun_calc.h"
#include "wifi_manager.h"
#include "../config/config.h"
#include "../core/mem_stats.h"
#include "../core/metadata.h"
#include "../hw/led_rgb.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <time.h>
#include <Sgp4.h>  // last: it defines a macro named pi
#undef pi

namespace services {

    using namespace space;

    // Records, allocated once at boot (docs/MEMORY.md: long-lived memory early, once).
    static Sat* sats_ = nullptr;  // MAX_SATS
    static Event events_[MAX_EVENTS];
    static Pass passes_[MAX_PASSES];
    static int8_t geo_el_[MAX_SATS];
    static SpaceView view_ = {};
    static SemaphoreHandle_t mtx = nullptr;

    static std::atomic<bool> fetching{false}, computing{false};
    static std::atomic<bool> fetch_done{false}, fetch_ok{false}, passes_done{false};
    static uint32_t next_fetch_ms = 0, next_pass_ms = 0, last_try_ms = 0, last_alert_ms = 0;
    static uint32_t pass_inputs = 0;  // hash of what the passes depend on (location, filters)

    static constexpr uint32_t FIRST_FETCH_MS = 4 * 60000;  // after the boot rush and the update check
    static constexpr uint32_t FETCH_EVERY_MS = 6 * 3600000UL, RETRY_MS = 15 * 60000, PASS_EVERY_MS = 30 * 60000;
    static constexpr uint32_t MIN_VALID_UTC = 1700000000, WINDOW_S = 24 * 3600;
    static constexpr size_t OBJ_BUF = 1280;  // one feed object (a satellite is ~0.6 KB)

    static uint32_t jd_to_unix(double jd) { return (uint32_t)((jd - 2440587.5) * 86400.0 + 0.5); }

    // ---- Feed download ---------------------------------------------------------------------

    // Reads the next {...} of a JSON array into buf (strings and escapes respected). False at the
    // array's end or on a timeout; an object longer than buf is skipped (buf[0] = 0).
    static bool read_object(Stream& s, char* buf, size_t cap) {
        int depth = 0;
        size_t n = 0;
        bool in_str = false, esc = false, overflow = false;
        const uint32_t start = millis();
        while (millis() - start < 8000) {
            if (!s.available()) {
                vTaskDelay(pdMS_TO_TICKS(5));
                continue;
            }
            const char c = (char)s.read();
            if (depth == 0) {
                if (c == ']') return false;
                if (c != '{') continue;
            }
            if (n < cap - 1) buf[n++] = c;
            else overflow = true;
            if (in_str) {
                if (esc) esc = false;
                else if (c == '\\') esc = true;
                else if (c == '"') in_str = false;
                continue;
            }
            if (c == '"') in_str = true;
            else if (c == '{') depth++;
            else if (c == '}' && --depth == 0) {
                buf[overflow ? 0 : n] = '\0';
                return true;
            }
        }
        return false;
    }

    static void feed_url(char* out, size_t len) {
        const char* own = config::get().space_url;
        if (own[0]) {
            strlcpy(out, own, len);
            return;
        }
        snprintf(out, len, "https://raw.githubusercontent.com/%s/space-feed/space.json", meta::GITHUB_REPO);
    }

    static void fetch_task(void*) {
        char url[sizeof(config::Config::space_url) + 40];
        feed_url(url, sizeof(url));
        const bool own = config::get().space_url[0] != '\0';
        const bool https = strncmp(url, "https://", 8) == 0;
        if (https) {  // quiet window (quiet_window.h)
            quiet::acquire();
            for (int i = 0; i < 80 && !quiet::settled(); i++) vTaskDelay(pdMS_TO_TICKS(100));
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        bool ok = false;
        int code = 0, n_sats = 0, n_events = 0;
        uint32_t generated = 0;
        {
            NetLock lock;
            if (lock.held() && (!https || tls_heap_ready("Space feed"))) {
                SafeClient plain;  // thread-safe DNS (safe_client.h)
                SafeTlsClient tls;
                tls.setInsecure();  // public data; parsing rejects anything else
                HTTPClient http;
                http.useHTTP10(true);
                if (https ? http.begin(tls, url) : http.begin(plain, url)) {
                    char ua[40];
                    snprintf(ua, sizeof(ua), "%s/%s", meta::FW_NAME, meta::FW_VERSION);
                    http.setUserAgent(ua);
                    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
                    http.setTimeout(15000);
                    code = http.GET();
                    char* buf = code == HTTP_CODE_OK ? static_cast<char*>(malloc(OBJ_BUF)) : nullptr;
                    if (buf) {
                        Stream& s = http.getStream();
                        s.setTimeout(8000);
                        if (s.find("\"generated\":")) generated = (uint32_t)s.parseInt();
                        if (s.find("\"sats\":[")) {
                            xSemaphoreTake(mtx, portMAX_DELAY);
                            while (n_sats < MAX_SATS && read_object(s, buf, OBJ_BUF)) {
                                JsonDocument doc;
                                if (buf[0] && !deserializeJson(doc, buf) &&
                                    parse_sat(doc.as<JsonObjectConst>(), sats_[n_sats]))
                                    n_sats++;
                            }
                            xSemaphoreGive(mtx);
                        }
                        Event ev[MAX_EVENTS];
                        if (n_sats && s.find("\"events\":[")) {
                            while (n_events < MAX_EVENTS && read_object(s, buf, OBJ_BUF)) {
                                JsonDocument doc;
                                if (buf[0] && !deserializeJson(doc, buf) &&
                                    parse_event(doc.as<JsonObjectConst>(), ev[n_events]))
                                    n_events++;
                            }
                        }
                        free(buf);
                        ok = n_sats > 0;
                        if (ok) {
                            xSemaphoreTake(mtx, portMAX_DELAY);
                            memcpy(events_, ev, sizeof(Event) * n_events);
                            view_.sat_count = (uint8_t)n_sats;
                            view_.event_count = (uint8_t)n_events;
                            view_.generated_utc = generated;
                            xSemaphoreGive(mtx);
                        }
                    }
                    http.end();
                }
            }
        }
        if (https) quiet::release();
        if (ok) {
            Serial.printf("[Space] Feed from %s: %d satellites, %d events\n", own ? "own source" : "GitHub",
                          n_sats, n_events);
            hw::led_rgb::trigger_traffic_pulse();
        } else {
            Serial.printf("[Space] Feed from %s failed (HTTP %d)\n", own ? "own source" : "GitHub", code);
        }
        fetch_ok = ok;
        fetch_done = true;
        mem_stats::note_stack(mem_stats::STK_SPACE);
        fetching = false;
        vTaskDelete(NULL);
    }

    // ---- Passes ----------------------------------------------------------------------------

    static void pass_task(void*) {
        const uint32_t t0 = millis();
        const auto& cfg = config::get();
        float lat, lon;
        sun::station_location(cfg.latlon_set, cfg.lat, cfg.lon, cfg.grid, lat, lon);
        const uint8_t cats = cfg.sat_categories, min_el = cfg.sat_min_el;
        const uint32_t now = (uint32_t)time(nullptr);

        static Pass found[MAX_PASSES * 2];  // pass task only (one at a time)
        int n = 0;
        int8_t geo[MAX_SATS];
        Sgp4* sgp = new Sgp4();
        int count;
        xSemaphoreTake(mtx, portMAX_DELAY);
        count = view_.sat_count;
        xSemaphoreGive(mtx);
        if (sgp) {
            sgp->site(lat, lon, 0);
            for (int i = 0; i < count; i++) {
                geo[i] = -128;
                Sat s;
                xSemaphoreTake(mtx, portMAX_DELAY);
                s = sats_[i];
                xSemaphoreGive(mtx);
                if (!(s.categories & cats)) continue;
                char l1[130], l2[130];
                strlcpy(l1, s.tle1, sizeof(l1));
                strlcpy(l2, s.tle2, sizeof(l2));
                if (!sgp->init(s.name, l1, l2)) continue;
                if (sgp->revpday < 1.5) {  // geostationary: never rises or sets
                    sgp->findsat((unsigned long)now);
                    geo[i] = (int8_t)std::max(-90.0, std::min(90.0, sgp->satEl));
                    continue;
                }
                sgp->initpredpoint((unsigned long)now, 0.0);
                for (int k = 0; k < 8 && n < MAX_PASSES * 2; k++) {
                    passinfo info;
                    if (!sgp->nextpass(&info, 20, false, (double)min_el)) break;
                    const uint32_t aos = jd_to_unix(info.jdstart);
                    if (aos > now + WINDOW_S) break;
                    found[n++] = {(uint8_t)i, aos, jd_to_unix(info.jdmax), jd_to_unix(info.jdstop),
                                  (uint8_t)std::max(0.0, std::min(90.0, info.maxelevation)),
                                  (uint16_t)info.azstart, (uint16_t)info.azstop, 0};
                }
                vTaskDelay(1);  // let other tasks run between satellites
            }
        }
        std::sort(found, found + n, [](const Pass& a, const Pass& b) { return a.aos < b.aos; });
        if (n > MAX_PASSES) n = MAX_PASSES;

        // School contacts: audible here when the ISS is above the horizon at contact time.
        Event ev[MAX_EVENTS];
        int n_ev;
        int iss = -1;
        xSemaphoreTake(mtx, portMAX_DELAY);
        n_ev = view_.event_count;
        memcpy(ev, events_, sizeof(Event) * n_ev);
        for (int i = 0; i < count; i++)
            if (sats_[i].norad == ISS) iss = i;
        Sat iss_rec = {};
        if (iss >= 0) iss_rec = sats_[iss];
        xSemaphoreGive(mtx);
        if (sgp && iss >= 0) {
            char l1[130], l2[130];
            strlcpy(l1, iss_rec.tle1, sizeof(l1));
            strlcpy(l2, iss_rec.tle2, sizeof(l2));
            if (sgp->init(iss_rec.name, l1, l2)) {
                for (int e = 0; e < n_ev; e++) {
                    if (ev[e].kind != EVENT_CONTACT) continue;
                    sgp->findsat((unsigned long)ev[e].start);
                    ev[e].audible_here = sgp->satEl > 0;
                }
            }
        }
        delete sgp;

        xSemaphoreTake(mtx, portMAX_DELAY);
        mark_event_passes(found, n, sats_, ev, n_ev);
        memcpy(passes_, found, sizeof(Pass) * n);
        memcpy(geo_el_, geo, sizeof(int8_t) * count);
        memcpy(events_, ev, sizeof(Event) * n_ev);
        view_.pass_count = (uint8_t)n;
        view_.passes_utc = now;
        view_.version++;
        xSemaphoreGive(mtx);
        Serial.printf("[Space] %d passes in the next 24 h (%d satellites) in %lu ms\n", n, count,
                      (unsigned long)(millis() - t0));
        mem_stats::note_stack(mem_stats::STK_SPACE_PASS);
        passes_done = true;
        computing = false;
        vTaskDelete(NULL);
    }

    // ---- Schedule --------------------------------------------------------------------------

    static bool start_fetch() {
        bool expected = false;
        if (!fetching.compare_exchange_strong(expected, true)) return false;
        last_try_ms = millis();
        if (xTaskCreatePinnedToCore(fetch_task, "space_fetch", 8192, NULL, 1, NULL, 0) != pdPASS) {
            fetching = false;
            return false;
        }
        return true;
    }

    static void start_passes() {
        bool expected = false;
        if (!computing.compare_exchange_strong(expected, true)) return;
        view_.computing = true;
        if (xTaskCreatePinnedToCore(pass_task, "space_passes", 10240, NULL, 1, NULL, 0) != pdPASS) {
            computing = false;
            view_.computing = false;
        }
    }

    static uint32_t inputs_hash() {
        const auto& c = config::get();
        uint32_t h = 2166136261u;
        auto mix = [&](const void* p, size_t n) {
            for (size_t i = 0; i < n; i++) h = (h ^ static_cast<const uint8_t*>(p)[i]) * 16777619u;
        };
        mix(&c.lat, sizeof(c.lat));
        mix(&c.lon, sizeof(c.lon));
        mix(&c.latlon_set, sizeof(c.latlon_set));
        mix(c.grid, sizeof(c.grid));
        mix(&c.sat_categories, sizeof(c.sat_categories));
        mix(&c.sat_min_el, sizeof(c.sat_min_el));
        return h;
    }

    void SpaceManager::init() {
        mtx = xSemaphoreCreateMutex();
        sats_ = static_cast<Sat*>(calloc(MAX_SATS, sizeof(Sat)));
        memset(geo_el_, -128, sizeof(geo_el_));
        next_fetch_ms = FIRST_FETCH_MS;
        pass_inputs = inputs_hash();
    }

    void SpaceManager::update() {
        if (!sats_ || !mtx) return;
        const uint32_t now_ms = millis();
        const uint32_t now = (uint32_t)time(nullptr);
        const bool online = now >= MIN_VALID_UTC && wifi_manager_is_connected();

        if (fetch_done.exchange(false)) {
            if (fetch_ok) {
                view_.has_feed = true;
                view_.fetched_utc = now;
                view_.failed_utc = 0;
                next_fetch_ms = now_ms + FETCH_EVERY_MS;
                next_pass_ms = now_ms;  // new orbits and events
            } else {
                view_.failed_utc = now;
                next_fetch_ms = now_ms + RETRY_MS;
            }
            view_.version++;
        }
        if (passes_done.exchange(false)) view_.computing = false;

        if (online && !fetching && (int32_t)(now_ms - next_fetch_ms) >= 0) start_fetch();

        const uint32_t h = inputs_hash();
        if (h != pass_inputs) {  // location or filters changed in the settings
            pass_inputs = h;
            next_pass_ms = now_ms;
        }
        if (view_.sat_count && now >= MIN_VALID_UTC && !computing && (int32_t)(now_ms - next_pass_ms) >= 0) {
            next_pass_ms = now_ms + PASS_EVERY_MS;
            start_passes();
        }

        // LED pass alert, checked every 5 s.
        if (now_ms - last_alert_ms >= 5000) {
            last_alert_ms = now_ms;
            const auto& cfg = config::get();
            hw::led_rgb::PassAlert led = hw::led_rgb::PassAlert::NONE;
            if (cfg.pass_alert_enabled && view_.pass_count && xSemaphoreTake(mtx, 0) == pdTRUE) {
                const Alert a = alert_state(passes_, view_.pass_count, now, (uint32_t)cfg.pass_alert_min * 60);
                xSemaphoreGive(mtx);
                led = a == Alert::NOW ? hw::led_rgb::PassAlert::NOW
                                      : (a == Alert::SOON ? hw::led_rgb::PassAlert::SOON : hw::led_rgb::PassAlert::NONE);
            }
            hw::led_rgb::set_pass_alert(led);
        }
    }

    bool SpaceManager::refresh_now() {
        if (fetching || (last_try_ms && millis() - last_try_ms < RETRY_MS)) return false;
        return start_fetch();
    }

    const SpaceView& SpaceManager::view() { return view_; }
    bool SpaceManager::lock(uint32_t timeout_ms) { return mtx && xSemaphoreTake(mtx, pdMS_TO_TICKS(timeout_ms)) == pdTRUE; }
    void SpaceManager::unlock() { xSemaphoreGive(mtx); }
    const Sat* SpaceManager::sats() { return sats_; }
    const Event* SpaceManager::events() { return events_; }
    const Pass* SpaceManager::passes() { return passes_; }
    int8_t SpaceManager::geo_elevation(uint8_t i) { return i < MAX_SATS ? geo_el_[i] : -128; }

}  // namespace services
