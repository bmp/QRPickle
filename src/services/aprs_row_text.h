#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "../core/units.h"

// One APRS station as one monospace line for the APRS list (host-safe; test/test_parsers):
// call 8, type 13, distance 12, then bearing and compass point; one space between. The page
// header's columns sit at the same x (6 px per character). hide: screenshot builds show no
// position-revealing values (privacy.h). dist_unit: config::DIST_* (km or miles).
namespace services {

    inline void aprs_row_text(const char* call, const char* type, float dist_km, int bearing_deg, bool hide,
                              uint8_t dist_unit, char* out, size_t len) {
        if (!out || !len) return;
        static const char* const DIRS[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
                                           "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
        if (bearing_deg < 0 || bearing_deg >= 360) bearing_deg = 0;
        char dist[16], brg[16];
        if (hide) {
            snprintf(dist, sizeof(dist), "-- %s", units::dist_unit(dist_unit));
            snprintf(brg, sizeof(brg), "--");
        } else {
            units::fmt_dist(dist, sizeof(dist), dist_unit, dist_km);
            snprintf(brg, sizeof(brg), "%d\xC2\xB0 %s", bearing_deg, DIRS[((bearing_deg + 11) % 360) / 22 % 16]);
        }
        snprintf(out, len, "%-8.8s %-13.13s %-12.12s %s", call, type, dist, brg);
    }

}  // namespace services
