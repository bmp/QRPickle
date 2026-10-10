#pragma once
#include <stddef.h>
#include <stdint.h>

// Solar-terrestrial data in the hamqsl.com "solarxml" format (N0NBH), and the fetch schedule.
// Host-safe (no Arduino): unit-tested in test/test_parsers.
namespace services {
    namespace solar {

        constexpr int16_t NONE = -1;  // field missing or "No Report"

        struct SolarData {
            bool valid;  // at least SFI and K parsed
            uint32_t updated_utc;  // the source's own timestamp (0 = unknown)
            int16_t sfi;  // 10.7 cm solar flux
            int16_t sunspots;
            int16_t a_index;
            int16_t k_index;
            int16_t aurora;  // hamqsl's aurora activity level
            float solar_wind;  // km/s, < 0 = none
            float bz;  // nT (interplanetary field, "magneticfield"); NAN = none
            char xray[8];  // flare class, e.g. "B9.2"; "" = none
            char geomag[12];  // e.g. "QUIET", "UNSETTLD", "MINOR STORM"
            char noise[10];  // e.g. "S2-S3"
            char muf[8];  // MHz, often "NoRpt"; "" = none
        };

        // Parses one solarxml document. False if it isn't one (no <solardata> or no SFI/K).
        bool parse_xml(const char* xml, SolarData& out);

        // " 10 Oct 2026 0727 GMT" -> seconds since 1970 (UTC); 0 if malformed.
        uint32_t parse_updated(const char* s);

        // Storm mode: K >= 5 or an M/X-class flare.
        bool is_storm(const SolarData& d);

        // Fetch schedule (docs/TASKS.md, v0.2.4): the indices are global and slow (SFI/A daily,
        // K every 3 h), so fetch 15 min after each 3-hour K update (00:15, 03:15, ... UTC), hourly
        // in storm mode, and never more than once an hour (hamqsl.com asks for at most hourly).
        // A failed fetch is retried after RETRY_S.
        constexpr uint32_t MIN_INTERVAL_S = 3600;
        constexpr uint32_t SLOT_S = 3 * 3600;
        constexpr uint32_t SLOT_OFFSET_S = 15 * 60;
        constexpr uint32_t RETRY_S = 15 * 60;

        struct Schedule {
            uint32_t last_ok_utc;  // last successful fetch (0 = never)
            uint32_t last_attempt_utc;  // last attempt, successful or not (0 = never)
            bool storm;
        };
        // Whether to fetch at now_utc (a real UTC time).
        bool fetch_due(const Schedule& s, uint32_t now_utc);
        // Whether a manual refresh is allowed now (same hourly limit).
        bool manual_allowed(const Schedule& s, uint32_t now_utc);

    }  // namespace solar
}  // namespace services
