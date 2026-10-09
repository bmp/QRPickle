#pragma once
#include <unity.h>
#include "../../src/services/aprs_parse.h"
#include "../../src/services/aprs_parse.cpp"  // host-safe; compiled into this suite

inline void test_aprs_uncompressed_position() {
    float lat, lon; char t, s;
    TEST_ASSERT_TRUE(services::aprs::parse_uncompressed_latlon("!4903.50N/07201.75W-Test", lat, lon, t, s));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 49.0583f, lat);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -72.0292f, lon);
    TEST_ASSERT_EQUAL_CHAR('/', t);
    TEST_ASSERT_EQUAL_CHAR('-', s);
    TEST_ASSERT_TRUE(services::aprs::parse_uncompressed_latlon("=1258.  N/07735.  E>", lat, lon, t, s));  // ambiguity
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.9667f, lat);
}

inline void test_aprs_rejects_compressed_and_garbage() {
    float lat, lon; char t, s;
    TEST_ASSERT_FALSE(services::aprs::parse_uncompressed_latlon("!/5L!!<*e7>7P[ab", lat, lon, t, s));        // compressed
    TEST_ASSERT_FALSE(services::aprs::parse_uncompressed_latlon("!/5L!!<*e7>7P[ comment long", lat, lon, t, s));
    TEST_ASSERT_FALSE(services::aprs::parse_uncompressed_latlon("!9903.50N/07201.75W-", lat, lon, t, s));   // lat > 90
    TEST_ASSERT_FALSE(services::aprs::parse_uncompressed_latlon("!4903.50X/07201.75W-", lat, lon, t, s));   // bad N/S
}

inline void test_aprs_addressee() {
    using services::aprs::addressed_to;
    TEST_ASSERT_TRUE(addressed_to("VU3GLJ   ", "VU3GLJ"));
    TEST_ASSERT_TRUE(addressed_to("vu3glj-7 ", "VU3GLJ"));
    TEST_ASSERT_TRUE(addressed_to("VU3GLJ-15", "VU3GLJ"));
    TEST_ASSERT_FALSE(addressed_to("VU3GLJX  ", "VU3GLJ"));
    TEST_ASSERT_FALSE(addressed_to("VU3GL    ", "VU3GLJ"));
    TEST_ASSERT_FALSE(addressed_to("VU3GLJ-  ", "VU3GLJ"));
}
