#include "fresh_registry.h"
#include <Arduino.h>

namespace services {
    namespace fresh {

        namespace {
            const char* const NAMES[COUNT] = {"weather", "solar", "feed", "pota", "sota",
                                              "dx", "hamalert", "aprs", "ota_check"};
            // Normal refresh interval per source; STALE after twice this without a success. The
            // spot sources and connections only refresh while used, so their state matters while
            // their screen is open or cycled.
            const uint32_t INTERVAL_MS[COUNT] = {
                2 * 3600000UL,  // weather: every 2 h (3 min around sunrise/sunset)
                3 * 3600000UL,  // solar: after each 3-hourly update
                6 * 3600000UL,  // satellite feed
                5 * 60000UL,  // POTA: 5 min while cycled, 30 s on a manual visit
                15 * 60000UL,  // SOTA cluster
                15 * 60000UL,  // DX cluster
                15 * 60000UL,  // HamAlert
                15 * 60000UL,  // APRS-IS
                24 * 3600000UL  // update check
            };
            Freshness recs[COUNT];
            portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
        }  // namespace

        void ok(Source s) {
            if (s >= COUNT) return;
            portENTER_CRITICAL(&mux);
            recs[s].ok(millis());
            portEXIT_CRITICAL(&mux);
        }

        void fail(Source s) {
            if (s >= COUNT) return;
            portENTER_CRITICAL(&mux);
            recs[s].fail(millis());
            portEXIT_CRITICAL(&mux);
        }

        Freshness get(Source s) {
            Freshness f{};
            if (s >= COUNT) return f;
            portENTER_CRITICAL(&mux);
            f = recs[s];
            portEXIT_CRITICAL(&mux);
            return f;
        }

        uint32_t age_s(Source s) {
            const Freshness f = get(s);
            return f.last_ok_ms ? (millis() - f.last_ok_ms) / 1000 : UINT32_MAX;
        }

        FreshState state(Source s) { return s < COUNT ? fresh_state(get(s), millis(), INTERVAL_MS[s]) : FreshState::NONE; }

        void to_json(JsonObject out) {
            for (uint8_t i = 0; i < COUNT; i++) {
                const Source s = static_cast<Source>(i);
                const Freshness f = get(s);
                JsonObject o = out[NAMES[i]].to<JsonObject>();
                if (f.last_ok_ms) o["age_s"] = (millis() - f.last_ok_ms) / 1000;
                o["fails"] = f.fails;
                o["state"] = fresh_name(state(s));
            }
        }

    }  // namespace fresh
}  // namespace services
