// Display units and local time (core/units.h, issue #25): conversions, text, 12-hour clock.
#include <unity.h>
#include <math.h>
#include <string.h>
#include "../../src/core/units.h"

using namespace units;

void setUp() {}
void tearDown() {}

#define DEG "\xC2\xB0"

static char buf[32];

void test_temperature() {
    fmt_temp(buf, sizeof(buf), 0, 21.5f, 1);
    TEST_ASSERT_EQUAL_STRING("21.5" DEG "C", buf);
    fmt_temp(buf, sizeof(buf), 1, 0.0f, 1);
    TEST_ASSERT_EQUAL_STRING("32.0" DEG "F", buf);
    fmt_temp(buf, sizeof(buf), 1, -40.0f, 1);
    TEST_ASSERT_EQUAL_STRING("-40.0" DEG "F", buf);
    fmt_temp(buf, sizeof(buf), 1, 100.0f, 0);
    TEST_ASSERT_EQUAL_STRING("212" DEG "F", buf);
    fmt_temp(buf, sizeof(buf), 0, -0.4f, 1);
    TEST_ASSERT_EQUAL_STRING("-0.4" DEG "C", buf);
    fmt_temp(buf, sizeof(buf), 0, -0.04f, 1);
    TEST_ASSERT_EQUAL_STRING("0.0" DEG "C", buf);  // no "-0.0"
    fmt_temp(buf, sizeof(buf), 0, -0.4f, 0);
    TEST_ASSERT_EQUAL_STRING("0" DEG "C", buf);
    fmt_temp(buf, sizeof(buf), 1, NAN, 1);
    TEST_ASSERT_EQUAL_STRING("--" DEG "F", buf);
}

void test_pressure() {
    fmt_pres(buf, sizeof(buf), 0, 1013.25f);
    TEST_ASSERT_EQUAL_STRING("1013 hPa", buf);
    fmt_pres(buf, sizeof(buf), 1, 1013.25f);
    TEST_ASSERT_EQUAL_STRING("29.92 inHg", buf);
    fmt_pres(buf, sizeof(buf), 1, NAN);
    TEST_ASSERT_EQUAL_STRING("-- inHg", buf);
}

void test_wind() {
    fmt_wind(buf, sizeof(buf), 0, 10.0f, 1);
    TEST_ASSERT_EQUAL_STRING("10.0 m/s", buf);
    fmt_wind(buf, sizeof(buf), 1, 10.0f, 1);
    TEST_ASSERT_EQUAL_STRING("36 km/h", buf);  // km/h never has decimals
    fmt_wind(buf, sizeof(buf), 2, 10.0f, 1);
    TEST_ASSERT_EQUAL_STRING("22.4 mph", buf);
    fmt_wind(buf, sizeof(buf), 3, 10.0f, 1);
    TEST_ASSERT_EQUAL_STRING("19.4 kn", buf);
    fmt_wind(buf, sizeof(buf), 0, 4.2f, 0);
    TEST_ASSERT_EQUAL_STRING("4 m/s", buf);
    fmt_wind(buf, sizeof(buf), 3, NAN, 1);
    TEST_ASSERT_EQUAL_STRING("-- kn", buf);
}

void test_distance() {
    fmt_dist(buf, sizeof(buf), 0, 1.0f);
    TEST_ASSERT_EQUAL_STRING("1.0 km", buf);
    fmt_dist(buf, sizeof(buf), 1, 1.0f);
    TEST_ASSERT_EQUAL_STRING("0.6 mi", buf);
    fmt_dist(buf, sizeof(buf), 1, 12.34f);
    TEST_ASSERT_EQUAL_STRING("7.7 mi", buf);
    fmt_dist(buf, sizeof(buf), 1, NAN);
    TEST_ASSERT_EQUAL_STRING("-- mi", buf);
}

void test_dist_in() {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, dist_in(0, 100.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 62.14f, dist_in(1, 100.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, dist_in(9, 5.0f));  // unknown -> km
}

void test_unit_labels_and_unknown_units() {
    TEST_ASSERT_EQUAL_STRING(DEG "F", temp_unit(1));
    TEST_ASSERT_EQUAL_STRING("inHg", pres_unit(1));
    TEST_ASSERT_EQUAL_STRING("km/h", wind_unit(1));
    TEST_ASSERT_EQUAL_STRING("mi", dist_unit(1));
    TEST_ASSERT_EQUAL_STRING(DEG "C", temp_unit(9));  // unknown -> metric
    TEST_ASSERT_EQUAL_STRING("m/s", wind_unit(9));
    fmt_wind(buf, sizeof(buf), 9, 10.0f, 1);
    TEST_ASSERT_EQUAL_STRING("10.0 m/s", buf);
}

void test_clock_24h() {
    fmt_clock(buf, sizeof(buf), false, 9, 5, 0, false);
    TEST_ASSERT_EQUAL_STRING("09:05", buf);
    fmt_clock(buf, sizeof(buf), false, 14, 5, 33, true);
    TEST_ASSERT_EQUAL_STRING("14:05:33", buf);
    fmt_clock(buf, sizeof(buf), false, -1, 0, 0, false);
    TEST_ASSERT_EQUAL_STRING("--:--", buf);
}

void test_clock_12h() {
    fmt_clock(buf, sizeof(buf), true, 0, 5, 0, false);
    TEST_ASSERT_EQUAL_STRING("12:05 AM", buf);
    fmt_clock(buf, sizeof(buf), true, 11, 59, 0, false);
    TEST_ASSERT_EQUAL_STRING("11:59 AM", buf);
    fmt_clock(buf, sizeof(buf), true, 12, 0, 0, false);
    TEST_ASSERT_EQUAL_STRING("12:00 PM", buf);
    fmt_clock(buf, sizeof(buf), true, 23, 59, 0, false);
    TEST_ASSERT_EQUAL_STRING("11:59 PM", buf);
    fmt_clock(buf, sizeof(buf), true, 14, 5, 33, true);
    TEST_ASSERT_EQUAL_STRING("2:05:33 PM", buf);
    fmt_clock(buf, sizeof(buf), true, -1, 0, 0, false);
    TEST_ASSERT_EQUAL_STRING("--:--", buf);
}

void test_small_buffer_is_terminated() {
    char small[4];
    memset(small, 'x', sizeof(small));
    size_t n = fmt_pres(small, sizeof(small), 1, 1013.25f);
    TEST_ASSERT_EQUAL_STRING("29.", small);
    TEST_ASSERT_EQUAL_size_t(3, n);
    TEST_ASSERT_EQUAL_size_t(0, fmt_temp(nullptr, 0, 0, 1.0f, 1));
}

void test_utc_to_local() {
    struct tm t;
    const uint32_t new_year = 1767225600;  // 2026-01-01 00:00 UTC
    TEST_ASSERT_TRUE(utc_to_local(new_year, 11, &t));  // +5:30
    TEST_ASSERT_EQUAL_INT(5, t.tm_hour);
    TEST_ASSERT_EQUAL_INT(30, t.tm_min);
    TEST_ASSERT_EQUAL_INT(1, t.tm_mday);
    TEST_ASSERT_EQUAL_INT(0, t.tm_mon);
    TEST_ASSERT_TRUE(utc_to_local(new_year, -7, &t));  // -3:30: the day before
    TEST_ASSERT_EQUAL_INT(20, t.tm_hour);
    TEST_ASSERT_EQUAL_INT(30, t.tm_min);
    TEST_ASSERT_EQUAL_INT(31, t.tm_mday);
    TEST_ASSERT_EQUAL_INT(11, t.tm_mon);
    TEST_ASSERT_EQUAL_INT(125, t.tm_year);
    TEST_ASSERT_TRUE(utc_to_local(new_year, 28, &t));  // +14:00 (Kiribati)
    TEST_ASSERT_EQUAL_INT(14, t.tm_hour);
    TEST_ASSERT_FALSE(utc_to_local(0, 11, &t));  // no time yet
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_temperature);
    RUN_TEST(test_pressure);
    RUN_TEST(test_wind);
    RUN_TEST(test_distance);
    RUN_TEST(test_dist_in);
    RUN_TEST(test_unit_labels_and_unknown_units);
    RUN_TEST(test_clock_24h);
    RUN_TEST(test_clock_12h);
    RUN_TEST(test_small_buffer_is_terminated);
    RUN_TEST(test_utc_to_local);
    return UNITY_END();
}
