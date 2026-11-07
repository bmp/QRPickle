#include "fresh_registry.h"
#include "sources.h"
#include <Arduino.h>

namespace services {
    namespace fresh {

        namespace {
            // Names and normal intervals come from the jobs table (sources.h, docs/DATA_FLOW.md);
            // the first COUNT jobs are the data sources, in the same order.
            constexpr bool same(jobs::Job j, Source f) { return static_cast<int>(j) == static_cast<int>(f); }
            static_assert(same(jobs::WEATHER, WEATHER) && same(jobs::SOLAR, SOLAR) && same(jobs::FEED, FEED) &&
                              same(jobs::POTA, POTA) && same(jobs::SOTA, SOTA) && same(jobs::DX, DX) &&
                              same(jobs::HAMALERT, HAMALERT) && same(jobs::APRS, APRS) &&
                              same(jobs::OTA_CHECK, OTA_CHECK),
                          "fresh::Source must follow the order of QRP_JOBS");
            const char* name_of(uint8_t i) { return jobs::job_name(static_cast<jobs::Job>(i)); }
            uint32_t interval_of(uint8_t i) { return jobs::job_interval_ms(static_cast<jobs::Job>(i)); }
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

        FreshState state(Source s) { return s < COUNT ? fresh_state(get(s), millis(), interval_of(s)) : FreshState::NONE; }

        void to_json(JsonObject out) {
            for (uint8_t i = 0; i < COUNT; i++) {
                const Source s = static_cast<Source>(i);
                const Freshness f = get(s);
                JsonObject o = out[name_of(i)].to<JsonObject>();
                if (f.last_ok_ms) o["age_s"] = (millis() - f.last_ok_ms) / 1000;
                o["fails"] = f.fails;
                o["state"] = fresh_name(state(s));
            }
        }

    }  // namespace fresh
}  // namespace services
