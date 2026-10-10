#pragma once
#include <stdint.h>
#include <stddef.h>
#include "solar_parse.h"

namespace services {

    enum ConditionRating {
        RATING_POOR = 0,
        RATING_FAIR,
        RATING_GOOD
    };

    struct PropagationTelemetry {
        bool has_data;  // false until the first successful fetch: screens show "--"
        solar::SolarData solar;  // the latest solar-terrestrial data
        uint32_t fetched_utc;  // when it was fetched (0 = never)
        uint16_t sfi;
        uint8_t k_index;
        uint8_t a_index;
        char forecast[24];  // geomagnetic field summary, e.g. "QUIET"
        bool dirty;
    };

    // Solar data from hamqsl.com's solarxml (or the user's own source, config::solar_url), fetched
    // in a background task on the schedule in solar_parse.h; results are applied on the main loop.
    class PropagationManager {
    public:
        static void init();
        // Main loop: applies a finished fetch and starts the next one when it is due.
        static void update();
        // Manual refresh (screen button). False when rate-limited (at most hourly) or busy.
        static bool refresh_now();

        // Algorithmic evaluation engine based on solar ionization vs geomagnetic noise
        static ConditionRating get_band_rating(uint8_t band_group_idx, bool look_at_nighttime);

        static const PropagationTelemetry& get_telemetry();
        static void clear_dirty();

    private:
        static void fetch_task(void*);
        static bool start_fetch(uint32_t now_utc);
        static PropagationTelemetry data;
    };

}  // namespace services
