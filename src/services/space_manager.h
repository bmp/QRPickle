#pragma once
#include <stdint.h>
#include "space_parse.h"

// Satellites, SSTV and ARISS contacts (docs/TASKS.md v0.2.5). Downloads the space feed built by
// .github/workflows/space-feed.yml (or config::space_url) every 6 hours, computes the passes over
// the station for the next 24 hours with SGP4 in a background task, and drives the LED pass alert.
// The CYD sends no location anywhere: the feed is the same for everyone.
namespace services {

    struct SpaceView {
        uint32_t version;  // bumped when the data or the passes change
        bool has_feed;
        uint32_t fetched_utc;  // last successful download (0 = never)
        uint32_t failed_utc;  // last download failed at (0 = it succeeded)
        uint32_t generated_utc;  // when GitHub built the feed
        uint8_t sat_count, event_count, pass_count;
        bool computing;  // passes being computed
        uint32_t passes_utc;  // passes computed at (0 = not yet)
    };

    class SpaceManager {
    public:
        static void init();  // early at boot: allocates the records once
        static void update();  // main loop: schedules downloads and pass runs, LED alert
        static bool refresh_now();  // screen button; false while busy or within 15 min of the last try
        static const SpaceView& view();

        // The records, for screens on the main loop. Hold lock() while reading them (they are
        // replaced by background tasks); never wait on it from the main loop (timeout 0).
        static bool lock(uint32_t timeout_ms = 0);
        static void unlock();
        static const space::Sat* sats();
        static const space::Event* events();
        static const space::Pass* passes();
        // A geostationary satellite has no passes: its elevation now (degrees), else -128.
        static int8_t geo_elevation(uint8_t sat_index);
    };

}  // namespace services
