#pragma once
#include <unity.h>
#include "../../src/services/sun_calc.h"

// Reference values from the astral 3.2 library (computed 2026-10-10; elevations include its
// refraction correction, hence the 0.5 deg tolerance near the horizon).

inline void test_grid_to_latlon() {
    using namespace services::sun;
    float lat, lon;
    TEST_ASSERT_TRUE(grid_to_latlon("MK82tw", lat, lon));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.9375f, lat);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 77.625f, lon);
    TEST_ASSERT_TRUE(grid_to_latlon("mk82TW", lat, lon));  // case-insensitive
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.9375f, lat);
    TEST_ASSERT_TRUE(grid_to_latlon("IO91", lat, lon));  // 4 characters: centre of the square
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 51.5f, lat);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, lon);
    TEST_ASSERT_TRUE(grid_to_latlon("AA00aa", lat, lon));  // south-west corner of the world
    TEST_ASSERT_FLOAT_WITHIN(0.03f, -89.98f, lat);
    TEST_ASSERT_FALSE(grid_to_latlon("SK82tw", lat, lon));  // field letters A..R only
    TEST_ASSERT_FALSE(grid_to_latlon("MK8", lat, lon));
    TEST_ASSERT_FALSE(grid_to_latlon("MK82zz", lat, lon));  // subsquare letters a..x only
    TEST_ASSERT_FALSE(grid_to_latlon("", lat, lon));
    TEST_ASSERT_FALSE(grid_to_latlon(nullptr, lat, lon));

    TEST_ASSERT_TRUE(latlon_in_grid("MK82tw", 12.94f, 77.62f));
    TEST_ASSERT_FALSE(latlon_in_grid("MK82tw", 12.97f, 77.59f));  // the factory default is just north of it
    TEST_ASSERT_TRUE(latlon_in_grid("MK82", 12.97f, 77.59f));
}

inline void test_station_location() {
    using namespace services::sun;
    float lat, lon;
    TEST_ASSERT_FALSE(station_location(true, 12.95f, 77.60f, "MK82tw", lat, lon));  // entered lat/lon win
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 12.95f, lat);
    TEST_ASSERT_TRUE(station_location(false, 12.97f, 77.59f, "MK82tw", lat, lon));  // not entered: grid centre
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.9375f, lat);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 77.625f, lon);
    TEST_ASSERT_FALSE(station_location(false, 12.97f, 77.59f, "bad", lat, lon));  // no usable grid
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 77.59f, lon);
}

inline void test_sun_elevation() {
    using namespace services::sun;
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 70.297f, elevation_deg(12.97f, 77.59f, 1791613800u));  // Bengaluru 06:30Z
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 7.448f, elevation_deg(12.97f, 77.59f, 1791633600u));  // 12:00Z, low sun
    TEST_ASSERT_FLOAT_WITHIN(0.2f, -79.116f, elevation_deg(12.97f, 77.59f, 1791655200u));
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 31.709f, elevation_deg(51.5f, -0.13f, 1791633600u));  // London noon
    TEST_ASSERT_FLOAT_WITHIN(0.2f, -42.715f, elevation_deg(-33.87f, 151.21f, 1791633600u));  // Sydney night

    TEST_ASSERT_TRUE(light_at(12.97f, 77.59f, 1791613800u) == Light::DAY);
    TEST_ASSERT_TRUE(light_at(51.5f, -0.13f, 1791613800u) == Light::GREYLINE);  // London just after sunrise
    TEST_ASSERT_TRUE(light_at(51.5f, -0.13f, 1791655200u) == Light::NIGHT);  // -7.3 deg
}

inline void test_next_sun_event() {
    using namespace services::sun;
    bool rising = false;
    // Bengaluru, 10 Oct 2026: from midnight UTC the next event is sunrise 00:39:34, then sunset 12:33:37.
    uint32_t t = next_sun_event(12.97f, 77.59f, 1791590400u, rising);
    TEST_ASSERT_TRUE(rising);
    TEST_ASSERT_UINT32_WITHIN(90, 1791592774u, t);
    t = next_sun_event(12.97f, 77.59f, t + 60, rising);
    TEST_ASSERT_FALSE(rising);
    TEST_ASSERT_UINT32_WITHIN(90, 1791635617u, t);
    // London, 21 Dec: sunrise 08:04:04.
    t = next_sun_event(51.5f, -0.13f, 1797811200u, rising);
    TEST_ASSERT_TRUE(rising);
    TEST_ASSERT_UINT32_WITHIN(90, 1797840244u, t);
    // Sydney, southern winter: sunset 06:53:35Z on 21 Jun.
    t = next_sun_event(-33.87f, 151.21f, 1782000000u, rising);
    TEST_ASSERT_FALSE(rising);
    TEST_ASSERT_UINT32_WITHIN(90, 1782024815u, t);
    // Tromso: midnight sun in June, polar night in December -> no event within 48 h.
    TEST_ASSERT_EQUAL_UINT32(0, next_sun_event(69.65f, 18.96f, 1782000000u, rising));
    TEST_ASSERT_EQUAL_UINT32(0, next_sun_event(69.65f, 18.96f, 1797811200u, rising));
}
