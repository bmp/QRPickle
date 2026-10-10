#pragma once
#include <stdint.h>

namespace config {

    constexpr float DEFAULT_LAT = 12.97f, DEFAULT_LON = 77.59f;  // factory lat/lon (Bengaluru)
    constexpr uint8_t BAND_GROUPS_ALL = 0x1F;

    struct Config {
        char callsign[12];
        char grid[8];
        uint8_t brightness;
        bool auto_brightness;
        uint8_t theme_id;
        int8_t tz_offset_hh;  // UTC offset in HALF-hours (e.g. 11 = +5:30); no DST
        uint8_t screen_timeout_min;  // backlight sleep after N minutes idle; 0 = manual sleep only
        uint8_t forecast_slots;  // bitmask of forecast slots shown (set in the web UI; default 0x0F)
        bool web_enabled;

        char wifi_ssid[33];
        char wifi_password[64];
        char openweather_api_key[40];
        float lat;
        float lon;
        bool latlon_set;  // lat/lon entered by the user; else band conditions use the grid square
        uint8_t band_groups;  // groups on the dashboard band tile, bit 0 = 160-60 m ... bit 4 = 6 m

        char dx_url_primary[64];
        uint16_t dx_port_primary;
        char dx_url_secondary[64];
        uint16_t dx_port_secondary;

        // Solar data (propagation): own source in hamqsl.com's solarxml format; "" = hamqsl.com.
        char solar_url[96];

        bool aprs_enabled;
        char aprs_passcode[8];
        int8_t aprs_ssid;  // 0..15; 0 = no -SSID suffix
        char aprs_comment[48];
        char aprs_icon[4];
        char aprs_macros[5][64];

        char hamalert_password[33];

        // Web console login (user "admin") and setup-AP WPA2 key. Generated on first boot.
        char admin_password[17];
    };

    void load();
    void save();
    const Config& get();
    Config& mutable_get();
    void reset_to_defaults();
    void log_summary();

}  // namespace config
