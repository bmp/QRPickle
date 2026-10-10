#pragma once
#include <stdint.h>
#include <stddef.h>
#include "band_rating.h"
#include "solar_parse.h"
#include "sun_calc.h"

namespace services {

    struct PropagationTelemetry {
        bool has_data;  // false until the first successful fetch: screens show "--"
        bool own_source;  // fetched from config::solar_url, not hamqsl.com
        solar::SolarData solar;  // the latest solar-terrestrial data
        uint32_t fetched_utc;  // when it was fetched (0 = never)
        uint32_t failed_utc;  // the last fetch failed at this time (0 = it succeeded)
    };

    // Everything the band tile and screen draw, recomputed on the main loop when the data, the
    // location or the minute changes. Screens compare `version` with the one they last drew.
    struct BandView {
        uint32_t version;
        bands::Rating band_day[bands::BAND_COUNT];
        bands::Rating band_night[bands::BAND_COUNT];
        bands::Rating group_day[bands::GROUP_COUNT];
        bands::Rating group_night[bands::GROUP_COUNT];
        bool time_valid;  // clock set (NTP); light and sun times need it
        sun::Light light;  // at the station now
        uint32_t next_sun_utc;  // next sunrise/sunset, 0 = none within 48 h (polar)
        bool next_rising;
        bool location_from_grid;  // lat/lon not entered: grid square centre
        bool storm;  // K >= 5 or M/X flare: fetching hourly
    };

    // Solar data from hamqsl.com's solarxml (or the user's own source, config::solar_url), fetched
    // in a background task on the schedule in solar_parse.h; results are applied on the main loop.
    class PropagationManager {
    public:
        static void init();
        // Main loop: applies a finished fetch, starts the next one when due, refreshes the view.
        static void update();
        // Manual refresh (screen button). False when rate-limited (at most hourly) or busy.
        static bool refresh_now();
        // When the refresh button unlocks (0 = now).
        static uint32_t refresh_unlock_utc();
        static bool is_fetching();
        // After a failed fetch: when it is retried (0 = no failure pending).
        static uint32_t next_retry_utc();

        static const PropagationTelemetry& get_telemetry();
        static const BandView& get_view();

    private:
        static void fetch_task(void*);
        static bool start_fetch(uint32_t now_utc);
        static void rebuild_view(uint32_t now_utc);
        static PropagationTelemetry data;
        static BandView view;
    };

}  // namespace services
