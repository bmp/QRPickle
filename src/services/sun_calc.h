#pragma once
#include <stdint.h>

// Station location and the sun: Maidenhead grid squares, solar elevation, day/night/greyline and
// the next sunrise/sunset. Host-safe (no Arduino): unit-tested in test/test_parsers.
namespace services {
    namespace sun {

        // Centre of a 4- or 6-character Maidenhead locator (case-insensitive). False if malformed.
        bool grid_to_latlon(const char* grid, float& lat, float& lon);
        // Whether lat/lon lie inside the locator's square (4 or 6 characters).
        bool latlon_in_grid(const char* grid, float lat, float lon);

        // Where the station is for band conditions: the user's lat/lon when they entered them (more
        // precise), else the centre of the grid square, else lat/lon anyway. Returns true for the grid.
        bool station_location(bool latlon_set, float lat, float lon, const char* grid, float& out_lat, float& out_lon);

        // Sun elevation in degrees at a place and time (simplified NOAA/Almanac formulas, ~0.1 deg).
        float elevation_deg(float lat, float lon, uint32_t utc);

        enum class Light : uint8_t {
            DAY,
            GREYLINE,
            NIGHT,
        };
        // Greyline: the sun within GREY_DEG of the horizon (about 30-60 min around sunrise/sunset).
        constexpr float GREY_DEG = 6.0f;
        Light light_at(float lat, float lon, uint32_t utc);

        // Next sunrise or sunset (upper limb at the horizon, -0.833 deg) after `from_utc`, within
        // 48 h. Returns 0 when there is none (polar day/night); `rising` tells which it is.
        uint32_t next_sun_event(float lat, float lon, uint32_t from_utc, bool& rising);

    }  // namespace sun
}  // namespace services
