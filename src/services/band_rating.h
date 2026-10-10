#pragma once
#include <stdint.h>

// Band-condition rating model (v0.2.4). Host-safe: unit-tested in test/test_parsers; documented
// in the manual appendix "How band conditions are rated". Estimates for a typical ~3000 km
// one-hop path at mid latitudes, not a prediction for a particular path.
namespace services {
    namespace bands {

        enum class Rating : uint8_t {
            UNKNOWN,  // no solar data yet
            POOR,
            FAIR,
            GOOD,
            ES_POSSIBLE,  // 6 m: sporadic-E season, daytime
            CLOSED,  // 6 m otherwise
        };

        struct Band {
            const char* name;  // "160m"
            float mhz;  // a typical operating frequency
            uint8_t group;  // index into GROUP_NAMES
        };
        constexpr int BAND_COUNT = 11;
        extern const Band BANDS[BAND_COUNT];
        constexpr int GROUP_COUNT = 5;  // bit i of config::band_groups = group i
        extern const char* const GROUP_NAMES[GROUP_COUNT];

        // All thresholds in one place (the appendix documents these numbers).
        struct Model {
            float muf_day_base, muf_day_per_ssn;  // daytime MUF = base + per_ssn * R
            float muf_night_base, muf_night_per_ssn;
            float fot_ratio;  // optimum working frequency = fot_ratio * MUF
            float luf_base, luf_per_sfi;  // daytime absorption limit = base + per_sfi * (SFI - 70)
            float luf_fair_ratio;  // below luf_fair_ratio * LUF a daytime band is at best fair
            float low_band_mhz;  // bands below this are rated for noise (K) too
            float six_m_mhz;
        };
        extern const Model MODEL;

        struct Inputs {
            int16_t sfi;  // <= 0: unknown
            int16_t k_index;  // < 0: unknown
            int16_t a_index;  // < 0: unknown (treated as quiet)
            float lat;  // station latitude, for the sporadic-E season
            uint8_t month;  // 1..12 (UTC), for the sporadic-E season
        };

        // Effective sunspot number from the solar flux (inverse of SFI = 63.7 + 0.728 R + 0.00089 R^2).
        float effective_ssn(int16_t sfi);
        // Geomagnetic MUF reduction factor (1.0 = none) from K and A; the stronger of the two.
        float storm_factor(int16_t k_index, int16_t a_index);
        // Estimated MUF in MHz, including the storm factor.
        float muf_mhz(const Inputs& in, bool night);
        // Daytime lowest usable frequency (D-layer absorption) in MHz.
        float luf_mhz(int16_t sfi);
        // Sporadic-E season at this latitude and month (May-Aug north, Nov-Feb south).
        bool es_season(float lat, uint8_t month);

        Rating rate_band(int band, const Inputs& in, bool night);
        // A group shows its best band ("something in this group works").
        Rating rate_group(int group, const Inputs& in, bool night);

    }  // namespace bands
}  // namespace services
