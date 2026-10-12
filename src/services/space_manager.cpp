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
#include <LittleFS.h>
#include <WiFiClientSecure.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <time.h>
#include <Sgp4.h>  // last: it defines a macro named pi
#undef pi

namespace services {

    using namespace space;

    // What stays in RAM (~3 KB); the feed itself is in LittleFS.
    static SatInfo sats_[MAX_SATS];
    static Event events_[MAX_EVENTS];
    static Pass passes_[MAX_PASSES];
    static int8_t geo_el_[MAX_SATS];
    static SpaceView view_ = {};
    static SemaphoreHandle_t mtx = nullptr;

    static std::atomic<bool> fetching{false}, computing{false};
    static std::atomic<bool> fetch_done{false}, fetch_ok{false}, passes_done{false};
    static uint32_t next_fetch_ms = 0, next_pass_ms = 0, last_try_ms = 0, last_alert_ms = 0;
    static uint32_t pass_inputs = 0;  // hash of what the last pass run used (location, filters)
    static bool cache_checked = false;

    static constexpr const char* FEED_PATH = "/space.json";
    static constexpr const char* TMP_PATH = "/space.tmp";
    static constexpr uint32_t FIRST_FETCH_MS = 4 * 60000;  // after the boot rush and the update check
    static constexpr uint32_t FETCH_EVERY_S = 6 * 3600, RETRY_MS = 15 * 60000, PASS_EVERY_MS = 30 * 60000;
    static constexpr uint32_t MIN_VALID_UTC = 1700000000, WINDOW_S = 24 * 3600;
    static constexpr size_t OBJ_BUF = 1280;  // one feed object (a satellite is ~0.6 KB)

    static uint32_t jd_to_unix(double jd) { return (uint32_t)((jd - 2440587.5) * 86400.0 + 0.5); }

    // ---- Reading the feed ------------------------------------------------------------------

    // Reads the next {...} of a JSON array into buf (strings and escapes respected). False at the
    // array's end or the end of the file; an object longer than buf is skipped (buf[0] = 0).
    static bool read_object(Stream& s, char* buf, size_t cap) {
        int depth = 0;
        size_t n = 0;
        bool in_str = false, esc = false, overflow = false;
        while (s.available()) {
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

    // Walks the usable satellites of a feed file in order (the order of the indices everywhere).
    // fn(index, sat) returns false to stop. Returns the number visited; generated is set if found.
    template <typename Fn>
    static int each_sat(File& f, char* buf, uint32_t* generated, Fn fn) {
        f.setTimeout(20);
        if (generated) *generated = f.find("\"generated\":") ? (uint32_t)f.parseInt() : 0;
        if (!f.find("\"sats\":[")) return 0;
        int n = 0;
        Sat s;
        while (n < MAX_SATS && read_object(f, buf, OBJ_BUF)) {
            JsonDocument doc;
            if (!buf[0] || deserializeJson(doc, buf) || !parse_sat(doc.as<JsonObjectConst>(), s)) continue;
            if (!fn(n, s)) return n + 1;
            n++;
        }
        return n;
    }

    // ---- Download --------------------------------------------------------------------------

    static void feed_url(char* out, size_t len) {
        const char* own = config::get().space_url;
        if (own[0]) {
            strlcpy(out, own, len);
            return;
        }
#ifdef QRP_SPACE_URL  // test builds: a feed served on the LAN
        strlcpy(out, QRP_SPACE_URL, len);
#else
        snprintf(out, len, "https://raw.githubusercontent.com/%s/space-feed/space.json", meta::GITHUB_REPO);
#endif
    }

    // Downloads into TMP_PATH, then replaces the feed if the file holds at least one satellite.
    static void fetch_task(void*) {
        char url[sizeof(config::Config::space_url) + 40];
        feed_url(url, sizeof(url));
        const bool https = strncmp(url, "https://", 8) == 0;
        if (https) {  // quiet window (quiet_window.h)
            quiet::acquire();
            for (int i = 0; i < 80 && !quiet::settled(); i++) vTaskDelay(pdMS_TO_TICKS(100));
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        int code = 0, written = 0;
        {
            NetLock lock;
            if (lock.held() && (!https || tls_heap_ready("Space feed"))) {
                SafeClient plain;  // thread-safe DNS (safe_client.h)
                SafeTlsClient tls;
                tls.setInsecure();  // public data; the parser rejects anything else
                HTTPClient http;
                http.useHTTP10(true);
                if (https ? http.begin(tls, url) : http.begin(plain, url)) {
                    char ua[40];
                    snprintf(ua, sizeof(ua), "%s/%s", meta::FW_NAME, meta::FW_VERSION);
                    http.setUserAgent(ua);
                    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
                    http.setTimeout(15000);
                    code = http.GET();
                    if (code == HTTP_CODE_OK) {
                        File f = LittleFS.open(TMP_PATH, "w");
                        if (f) {
                            written = http.writeToStream(&f);
                            f.close();
                        }
                    }
                    http.end();
                }
            }
        }
        if (https) quiet::release();

        int n_sats = 0;
        if (written > 0) {
            char* buf = static_cast<char*>(malloc(OBJ_BUF));
            File f = LittleFS.open(TMP_PATH, "r");
            if (buf && f) n_sats = each_sat(f, buf, nullptr, [](int, const Sat&) { return true; });
            if (f) f.close();
            free(buf);
        }
        const bool ok = n_sats > 0 && LittleFS.rename(TMP_PATH, FEED_PATH);
        if (!ok) LittleFS.remove(TMP_PATH);
        if (ok) {
            Serial.printf("[Space] Feed downloaded: %d satellites, %d B\n", n_sats, written);
            hw::led_rgb::trigger_traffic_pulse();
        } else {
            Serial.printf("[Space] Feed download failed (HTTP %d, %d B, %d satellites)\n", code, written, n_sats);
        }
        fetch_ok = ok;
        fetch_done = true;
        mem_stats::note_stack(mem_stats::STK_SPACE);
        fetching = false;
        vTaskDelete(NULL);
    }

    // ---- Passes ----------------------------------------------------------------------------

    // Reads the feed file and computes the passes (~12 s for 40 satellites, ~15 KB while it runs).
    // It doesn't take NetLock (the network would wait that long); it is scheduled away from the boot rush.
    static void pass_task(void*) {
        const uint32_t t0 = millis();
        const auto& cfg = config::get();
        float lat, lon;
        sun::station_location(cfg.latlon_set, cfg.lat, cfg.lon, cfg.grid, lat, lon);
        const uint8_t cats = cfg.sat_categories, min_el = cfg.sat_min_el;
        const uint32_t now = (uint32_t)time(nullptr);

        // Scratch for this run on the heap, freed at the end: none of it is held between runs.
        struct Scratch {
            SatInfo info[MAX_SATS];
            Pass found[MAX_PASSES];  // the earliest, sorted
            Event ev[MAX_EVENTS];
            int8_t geo[MAX_SATS];
            char l1[130], l2[130], iss1[70], iss2[70];
        };
        Scratch* sc = static_cast<Scratch*>(calloc(1, sizeof(Scratch)));
        if (!sc) {
            Serial.println("[Space] No memory for the pass run");
            passes_done = true;
            computing = false;
            vTaskDelete(NULL);
            return;
        }
        auto& info = sc->info;
        auto& found = sc->found;
        auto& ev = sc->ev;
        auto& geo = sc->geo;
        auto& l1 = sc->l1;
        auto& l2 = sc->l2;
        auto& iss1 = sc->iss1;
        auto& iss2 = sc->iss2;
        size_t n = 0;
        int count = 0, n_ev = 0;
        uint32_t generated = 0;
        bool have_iss = false;
        memset(geo, -128, sizeof(geo));

        char* buf = static_cast<char*>(malloc(OBJ_BUF));
        Sgp4* sgp = new Sgp4();
        File f = LittleFS.open(FEED_PATH, "r");
        if (buf && sgp && f) {
            sgp->site(lat, lon, 0);
            count = each_sat(f, buf, &generated, [&](int i, const Sat& s) {
                info[i] = {s.norad, {}, s.categories};
                strlcpy(info[i].name, s.name, sizeof(info[i].name));
                if (s.norad == ISS) {
                    memcpy(iss1, s.tle1, sizeof(iss1));
                    memcpy(iss2, s.tle2, sizeof(iss2));
                    have_iss = true;
                }
                if (!(s.categories & cats)) return true;
                strlcpy(l1, s.tle1, sizeof(l1));
                strlcpy(l2, s.tle2, sizeof(l2));
                char name[16];
                strlcpy(name, s.name, sizeof(name));
                if (!sgp->init(name, l1, l2)) return true;
                if (sgp->revpday < 1.5) {  // geostationary: never rises or sets
                    sgp->findsat((unsigned long)now);
                    geo[i] = (int8_t)std::max(-90.0, std::min(90.0, sgp->satEl));
                    return true;
                }
                sgp->initpredpoint((unsigned long)now, 0.0);
                for (int k = 0; k < 8; k++) {
                    passinfo p;
                    if (!sgp->nextpass(&p, 20, false, (double)min_el)) break;
                    const uint32_t aos = jd_to_unix(p.jdstart);
                    if (aos > now + WINDOW_S || (n == MAX_PASSES && aos >= found[n - 1].aos)) break;
                    const Pass pass = {(uint8_t)i, aos, jd_to_unix(p.jdmax), jd_to_unix(p.jdstop),
                                       (uint8_t)std::max(0.0, std::min(90.0, p.maxelevation)), (uint16_t)p.azstart,
                                       (uint16_t)p.azstop, 0};
                    n = insert_pass(found, n, MAX_PASSES, pass);
                }
                vTaskDelay(1);  // let other tasks run between satellites
                return true;
            });
            if (f.find("\"events\":[")) {
                while (n_ev < MAX_EVENTS && read_object(f, buf, OBJ_BUF)) {
                    JsonDocument doc;
                    if (buf[0] && !deserializeJson(doc, buf) && parse_event(doc.as<JsonObjectConst>(), ev[n_ev]))
                        n_ev++;
                }
            }
            // School contacts: audible here when the ISS is above the horizon at contact time.
            if (have_iss) {
                strlcpy(l1, iss1, sizeof(l1));
                strlcpy(l2, iss2, sizeof(l2));
                char name[] = "ISS";
                if (sgp->init(name, l1, l2)) {
                    for (int e = 0; e < n_ev; e++) {
                        if (ev[e].kind != EVENT_CONTACT) continue;
                        sgp->findsat((unsigned long)ev[e].start);
                        ev[e].audible_here = sgp->satEl > 0;
                    }
                }
            }
        }
        if (f) f.close();
        delete sgp;
        free(buf);
        mark_event_passes(found, n, info, ev, n_ev);

        xSemaphoreTake(mtx, portMAX_DELAY);
        memcpy(sats_, info, sizeof(SatInfo) * count);
        memcpy(passes_, found, sizeof(Pass) * n);
        memcpy(geo_el_, geo, sizeof(geo_el_));
        memcpy(events_, ev, sizeof(Event) * n_ev);
        view_.has_feed = count > 0;
        view_.sat_count = (uint8_t)count;
        view_.event_count = (uint8_t)n_ev;
        view_.pass_count = (uint8_t)n;
        if (generated) view_.generated_utc = generated;
        view_.passes_utc = now;
        view_.version++;
        xSemaphoreGive(mtx);

        Serial.printf("[Space] %u passes (%d satellites, %d events) in %lu ms\n", (unsigned)n, count, n_ev,
                      (unsigned long)(millis() - t0));
        for (size_t i = 0; i < n && i < 6; i++) {  // the next few, for checking against other trackers
            const time_t t = found[i].aos;
            struct tm tm;
            gmtime_r(&t, &tm);
            Serial.printf("[Space]   %02d:%02d:%02d UTC %-12s max %u deg, %u s\n", tm.tm_hour, tm.tm_min, tm.tm_sec,
                          info[found[i].sat].name, found[i].max_el, (unsigned)(found[i].los - found[i].aos));
        }
        free(sc);
        mem_stats::note_stack(mem_stats::STK_SPACE_PASS);
        passes_done = true;
        computing = false;
        vTaskDelete(NULL);
    }

    // ---- Schedule --------------------------------------------------------------------------

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

    // Fetch and pass runs never overlap: the pass run reads the file the fetch replaces.
    static bool start_fetch() {
        if (computing) return false;
        bool expected = false;
        if (!fetching.compare_exchange_strong(expected, true)) return false;
        last_try_ms = millis();
        if (xTaskCreatePinnedToCore(fetch_task, "space_fetch", 6144, NULL, 1, NULL, 0) != pdPASS) {
            fetching = false;
            return false;
        }
        return true;
    }

    static void start_passes() {
        if (fetching) return;
        bool expected = false;
        if (!computing.compare_exchange_strong(expected, true)) return;
        pass_inputs = inputs_hash();
        view_.computing = true;
        if (xTaskCreatePinnedToCore(pass_task, "space_passes", 8192, NULL, 1, NULL, 0) != pdPASS) {
            computing = false;
            view_.computing = false;
        }
    }

    void SpaceManager::init() {
        mtx = xSemaphoreCreateMutex();
        memset(geo_el_, -128, sizeof(geo_el_));
        next_fetch_ms = FIRST_FETCH_MS;
        next_pass_ms = UINT32_MAX / 2;  // set when there is a feed (cached or downloaded)
    }

    void SpaceManager::update() {
        if (!mtx) return;
        const uint32_t now_ms = millis();
        const uint32_t now = (uint32_t)time(nullptr);
        const bool clock_ok = now >= MIN_VALID_UTC;

        // A feed cached before the reboot: passes once the boot's network work is done (the update
        // check at 2 minutes needs the heap). LittleFS is mounted by the web server at boot.
        if (!cache_checked && clock_ok && now_ms >= FIRST_FETCH_MS) {
            cache_checked = true;
            if (LittleFS.exists(FEED_PATH)) next_pass_ms = now_ms;
        }

        if (fetch_done.exchange(false)) {
            if (fetch_ok) {
                view_.fetched_utc = now;
                view_.failed_utc = 0;
                next_fetch_ms = now_ms + FETCH_EVERY_S * 1000;
                next_pass_ms = now_ms;  // new orbits and events
            } else {
                view_.failed_utc = now;
                next_fetch_ms = now_ms + RETRY_MS;
            }
            view_.version++;
        }
        if (passes_done.exchange(false)) {
            view_.computing = false;
            // A fresh cached feed waits for its six hours (GitHub rebuilds it on that schedule).
            if (!view_.fetched_utc && view_.generated_utc && now - view_.generated_utc < FETCH_EVERY_S) {
                const uint32_t due_ms = now_ms + (view_.generated_utc + FETCH_EVERY_S - now) * 1000;
                if ((int32_t)(due_ms - next_fetch_ms) > 0) next_fetch_ms = due_ms;
            }
        }

        if (clock_ok && wifi_manager_is_connected() && (int32_t)(now_ms - next_fetch_ms) >= 0) start_fetch();

        if (view_.passes_utc && !computing && inputs_hash() != pass_inputs) {  // location or filters changed
            Serial.printf("[Space] inputs changed %08lx -> %08lx\n", (unsigned long)pass_inputs, (unsigned long)inputs_hash());
            pass_inputs = inputs_hash();
            next_pass_ms = now_ms;
        }
        if (clock_ok && !computing && !fetching && (int32_t)(now_ms - next_pass_ms) >= 0) {
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
        if (fetching || computing || (last_try_ms && millis() - last_try_ms < RETRY_MS)) return false;
        return start_fetch();
    }

    bool SpaceManager::load_sat(uint8_t sat_index, Sat& out) {
        if (sat_index >= MAX_SATS || fetching) return false;
        char* buf = static_cast<char*>(malloc(OBJ_BUF));
        File f = LittleFS.open(FEED_PATH, "r");
        bool found = false;
        if (buf && f) {
            each_sat(f, buf, nullptr, [&](int i, const Sat& s) {
                if (i != sat_index) return true;
                out = s;
                found = true;
                return false;
            });
        }
        if (f) f.close();
        free(buf);
        return found;
    }

    const SpaceView& SpaceManager::view() { return view_; }
    bool SpaceManager::lock(uint32_t timeout_ms) { return mtx && xSemaphoreTake(mtx, pdMS_TO_TICKS(timeout_ms)) == pdTRUE; }
    void SpaceManager::unlock() { xSemaphoreGive(mtx); }
    const SatInfo* SpaceManager::sats() { return sats_; }
    const Event* SpaceManager::events() { return events_; }
    const Pass* SpaceManager::passes() { return passes_; }
    int8_t SpaceManager::geo_elevation(uint8_t i) { return i < MAX_SATS ? geo_el_[i] : -128; }

}  // namespace services
