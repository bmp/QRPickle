#include <unity.h>
#include <cmath>
#include <cstring>
#include "../../src/config/config.h"
#include "../../src/config/config_validation.h"
#include "../../src/config/config_validation.cpp"  // host-safe; compiled into this test only

using namespace config;

void setUp() {}
void tearDown() {}

static Config base() {
    Config c{};
    strcpy(c.callsign, "VU3GLJ"); strcpy(c.grid, "MK82tw");
    c.brightness = 100; c.theme_id = 1; c.tz_offset_hh = 11; c.screen_timeout_min = 5;
    c.lat = 12.9f; c.lon = 77.6f; c.dx_port_primary = 7300; c.dx_port_secondary = 7373;
    c.aprs_ssid = 7; strcpy(c.admin_password, "abcd2345");
    return c;
}

void test_valid_config_is_normalised_not_reverted() {
    Config prev = base(), c = base();
    strcpy(c.callsign, "vu2abc"); strcpy(c.grid, "mk82");
    sanitize(c, prev);
    TEST_ASSERT_EQUAL_STRING("VU2ABC", c.callsign);
    TEST_ASSERT_EQUAL_STRING("MK82", c.grid);
}

void test_invalid_fields_revert_or_clamp() {
    Config prev = base(), c = base();
    strcpy(c.callsign, "<b>x");  c.brightness = 0; c.theme_id = 200; c.tz_offset_hh = -100;
    c.screen_timeout_min = 250; c.aprs_ssid = 40; c.lat = NAN; c.lon = 500.0f;
    c.dx_port_primary = 0; strcpy(c.admin_password, "short");
    sanitize(c, prev);
    TEST_ASSERT_EQUAL_STRING("VU3GLJ", c.callsign);
    TEST_ASSERT_EQUAL_UINT8(10, c.brightness);
    TEST_ASSERT_EQUAL_UINT8(5, c.theme_id);
    TEST_ASSERT_EQUAL_INT8(-24, c.tz_offset_hh);
    TEST_ASSERT_EQUAL_UINT8(60, c.screen_timeout_min);
    TEST_ASSERT_EQUAL_INT8(7, c.aprs_ssid);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.9f, c.lat);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 77.6f, c.lon);
    TEST_ASSERT_EQUAL_UINT16(7300, c.dx_port_primary);
    TEST_ASSERT_EQUAL_STRING("abcd2345", c.admin_password);
}

void test_admin_password_rules() {
    Config prev = base(), c = base();
    strcpy(c.admin_password, "has space1");
    sanitize(c, prev);
    TEST_ASSERT_EQUAL_STRING("abcd2345", c.admin_password);
    strcpy(c.admin_password, "N3w!Pass#2");
    sanitize(c, prev);
    TEST_ASSERT_EQUAL_STRING("N3w!Pass#2", c.admin_password);
}

void test_unterminated_strings_are_terminated() {
    Config prev = base(), c = base();
    memset(c.wifi_ssid, 'A', sizeof(c.wifi_ssid));
    sanitize(c, prev);
    TEST_ASSERT_EQUAL_UINT32(sizeof(c.wifi_ssid) - 1, strlen(c.wifi_ssid));
}

void test_profile_names() {
    TEST_ASSERT_TRUE(is_valid_profile_name("Field_Day-2"));
    TEST_ASSERT_FALSE(is_valid_profile_name(""));
    TEST_ASSERT_FALSE(is_valid_profile_name("../www/app"));
    TEST_ASSERT_FALSE(is_valid_profile_name("<img src=x>"));
    TEST_ASSERT_FALSE(is_valid_profile_name("a b"));
    TEST_ASSERT_FALSE(is_valid_profile_name("abcdefghijklmnopqrstuvwxy"));  // 25 chars
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_valid_config_is_normalised_not_reverted);
    RUN_TEST(test_invalid_fields_revert_or_clamp);
    RUN_TEST(test_admin_password_rules);
    RUN_TEST(test_unterminated_strings_are_terminated);
    RUN_TEST(test_profile_names);
    return UNITY_END();
}
