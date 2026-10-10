#include <unity.h>
#include <cmath>
#include <cstring>
#include "../../src/config/config.h"
#include "../../src/config/config_validation.h"
#include "../../src/config/config_validation.cpp"  // host-safe; compiled into this test only
#include "../../src/config/config_json.h"
#include "../../src/config/config_json.cpp"

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
    TEST_ASSERT_EQUAL_UINT8(THEME_ID_MAX, c.theme_id);  // 6 = E-Ink Monochrome Dark (all 7 themes valid)
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

static Config full() {
    Config c = base();
    strcpy(c.wifi_ssid, "home"); strcpy(c.wifi_password, "wifipass1"); strcpy(c.openweather_api_key, "k123");
    strcpy(c.aprs_passcode, "12345"); strcpy(c.hamalert_password, "hpw"); strcpy(c.aprs_icon, "/-");
    strcpy(c.dx_url_primary, "dx.example"); c.tz_offset_hh = -11;
    for (int i = 0; i < 5; i++) snprintf(c.aprs_macros[i], sizeof(c.aprs_macros[i]), "macro %d", i);
    c.forecast_slots = 0x81; c.auto_brightness = true;
    return c;
}

void test_json_round_trip_with_secrets() {
    Config src = full(), dst{};
    JsonDocument doc;
    to_json(src, doc.to<JsonObject>(), Secrets::Include);
    TEST_ASSERT_TRUE(doc["admin_pw"].isNull());           // never in JSON
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -5.5f, doc["offset"].as<float>());
    from_json(dst, doc.as<JsonObjectConst>());
    TEST_ASSERT_EQUAL_STRING("wifipass1", dst.wifi_password);
    TEST_ASSERT_EQUAL_STRING("hpw", dst.hamalert_password);
    TEST_ASSERT_EQUAL_STRING("/-", dst.aprs_icon);
    TEST_ASSERT_EQUAL_STRING("macro 4", dst.aprs_macros[4]);
    TEST_ASSERT_EQUAL_STRING("dx.example", dst.dx_url_primary);
    TEST_ASSERT_EQUAL_INT8(-11, dst.tz_offset_hh);
    TEST_ASSERT_EQUAL_UINT8(0x81, dst.forecast_slots);
    TEST_ASSERT_TRUE(dst.auto_brightness);
    TEST_ASSERT_EQUAL_STRING("", dst.admin_password);
}

void test_json_masked_secrets_are_kept_on_input() {
    Config src = full();
    JsonDocument doc;
    to_json(src, doc.to<JsonObject>(), Secrets::Mask);
    TEST_ASSERT_EQUAL_STRING("", doc["password"].as<const char*>());
    TEST_ASSERT_TRUE(doc["password_set"].as<bool>());
    doc["callsign"] = "VU2XYZ";
    Config dst = full();
    strcpy(dst.wifi_password, "other-pw");
    from_json(dst, doc.as<JsonObjectConst>());
    TEST_ASSERT_EQUAL_STRING("other-pw", dst.wifi_password);  // "" keeps the existing secret
    TEST_ASSERT_EQUAL_STRING("VU2XYZ", dst.callsign);
}

void test_json_missing_wrong_type_and_range() {
    Config c = full();
    JsonDocument doc;
    doc["callsign"] = 5;          // wrong type: kept
    doc["brightness"] = 999;      // clamped to the field
    doc["dx_port_p"] = -4;
    doc["aprs_macros"][0] = "only first";
    from_json(c, doc.as<JsonObjectConst>());
    TEST_ASSERT_EQUAL_STRING("VU3GLJ", c.callsign);
    TEST_ASSERT_EQUAL_UINT8(255, c.brightness);
    TEST_ASSERT_EQUAL_UINT16(0, c.dx_port_primary);   // sanitize() then restores the previous port
    TEST_ASSERT_EQUAL_STRING("only first", c.aprs_macros[0]);
    TEST_ASSERT_EQUAL_STRING("macro 1", c.aprs_macros[1]);
    TEST_ASSERT_EQUAL_STRING("home", c.wifi_ssid);    // missing: kept
}

void test_clear_secrets() {
    Config c = full();
    clear_secrets(c);
    TEST_ASSERT_EQUAL_STRING("", c.wifi_password);
    TEST_ASSERT_EQUAL_STRING("", c.openweather_api_key);
    TEST_ASSERT_EQUAL_STRING("", c.aprs_passcode);
    TEST_ASSERT_EQUAL_STRING("", c.hamalert_password);
    TEST_ASSERT_EQUAL_STRING("abcd2345", c.admin_password);
    TEST_ASSERT_EQUAL_STRING("home", c.wifi_ssid);
}

void test_solar_url() {
    TEST_ASSERT_TRUE(is_valid_solar_url(""));  // hamqsl.com
    TEST_ASSERT_TRUE(is_valid_solar_url("https://www.hamqsl.com/solarxml.php"));
    TEST_ASSERT_TRUE(is_valid_solar_url("http://192.168.0.10:8080/solar.xml"));
    TEST_ASSERT_FALSE(is_valid_solar_url("ftp://example.com/solar.xml"));
    TEST_ASSERT_FALSE(is_valid_solar_url("www.hamqsl.com/solarxml.php"));
    TEST_ASSERT_FALSE(is_valid_solar_url("https://"));
    TEST_ASSERT_FALSE(is_valid_solar_url("https:///path"));
    TEST_ASSERT_FALSE(is_valid_solar_url("https://a b.com/x"));
    TEST_ASSERT_FALSE(is_valid_solar_url("https://x.com/<script>"));

    Config prev = base(), c = base();
    strcpy(prev.solar_url, "https://ok.example/s.xml");
    strcpy(c.solar_url, "javascript:alert(1)");
    sanitize(c, prev);
    TEST_ASSERT_EQUAL_STRING("https://ok.example/s.xml", c.solar_url);  // invalid -> previous

    Config src = base(), dst{};
    strcpy(src.solar_url, "https://ok.example/s.xml");
    JsonDocument doc;
    to_json(src, doc.to<JsonObject>(), Secrets::Include);
    TEST_ASSERT_EQUAL_STRING("https://ok.example/s.xml", doc["solar_url"].as<const char*>());
    from_json(dst, doc.as<JsonObjectConst>());
    TEST_ASSERT_EQUAL_STRING("https://ok.example/s.xml", dst.solar_url);
}

void test_location_marker_and_band_groups() {
    Config prev = base(), c = base();
    prev.band_groups = 0x0E;
    c.band_groups = 0xE0;  // no valid bit -> previous choice
    sanitize(c, prev);
    TEST_ASSERT_EQUAL_HEX8(0x0E, c.band_groups);
    c.band_groups = 0xFF;  // extra bits dropped
    sanitize(c, prev);
    TEST_ASSERT_EQUAL_HEX8(BAND_GROUPS_ALL, c.band_groups);
    prev.band_groups = 0;
    c.band_groups = 0;  // nothing anywhere -> all
    sanitize(c, prev);
    TEST_ASSERT_EQUAL_HEX8(BAND_GROUPS_ALL, c.band_groups);

    Config src = base(), dst{};
    src.latlon_set = true;
    src.band_groups = 0x06;
    JsonDocument doc;
    to_json(src, doc.to<JsonObject>(), Secrets::Include);
    TEST_ASSERT_TRUE(doc["latlon_set"].as<bool>());
    from_json(dst, doc.as<JsonObjectConst>());
    TEST_ASSERT_TRUE(dst.latlon_set);
    TEST_ASSERT_EQUAL_HEX8(0x06, dst.band_groups);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_valid_config_is_normalised_not_reverted);
    RUN_TEST(test_invalid_fields_revert_or_clamp);
    RUN_TEST(test_admin_password_rules);
    RUN_TEST(test_unterminated_strings_are_terminated);
    RUN_TEST(test_profile_names);
    RUN_TEST(test_json_round_trip_with_secrets);
    RUN_TEST(test_json_masked_secrets_are_kept_on_input);
    RUN_TEST(test_json_missing_wrong_type_and_range);
    RUN_TEST(test_clear_secrets);
    RUN_TEST(test_solar_url);
    RUN_TEST(test_location_marker_and_band_groups);
    return UNITY_END();
}
