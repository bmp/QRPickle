#pragma once
#include <stdint.h>

namespace config {

    constexpr float DEFAULT_LAT = 12.97f, DEFAULT_LON = 77.59f;  // factory lat/lon (Bengaluru)
    constexpr uint8_t BAND_GROUPS_ALL = 0x1F;
    // Screen cycling (ui/screen_cycle.h): bit i = cycle screen i (Dashboard, Band Conditions,
    // Weather, DX Cluster, POTA, SOTA, APRS Radar).
    constexpr uint16_t CYCLE_SCREENS_ALL = 0x7F;
    constexpr uint16_t CYCLE_SECONDS_MIN = 10, CYCLE_SECONDS_MAX = 600, CYCLE_SECONDS_DEFAULT = 10;
    // Satellites (services/space_parse.h): category bits FM 1, linear 2, SSTV 4, voice 8, APRS 16.
    constexpr uint8_t SAT_CATEGORIES_ALL = 0x1F, SAT_CATEGORIES_DEFAULT = 0x0F;
    constexpr uint8_t SAT_MIN_EL_MAX = 60, SAT_MIN_EL_DEFAULT = 10;
    constexpr uint8_t PASS_ALERT_MIN_MAX = 120, PASS_ALERT_MIN_DEFAULT = 30;

    struct Config {
        char callsign[12];
        char grid[8];
        uint8_t brightness;
        bool auto_brightness;
        uint8_t theme_id;
        int8_t tz_offset_hh;  // UTC offset in HALF-hours (e.g. 11 = +5:30); no DST
        uint8_t screen_timeout_min;  // backlight sleep after N minutes idle; 0 = manual sleep only
        uint8_t forecast_slots;  // bitmask of forecast slots shown (set in the web UI; default 0x0F)

        char wifi_ssid[33];
        char wifi_password[64];
        char openweather_api_key[40];
        float lat;
        float lon;
        bool latlon_set;  // lat/lon entered by the user; else band conditions use the grid square
        uint8_t band_groups;  // groups on the dashboard band tile, bit 0 = 160-60 m ... bit 4 = 6 m
        bool cycle_enabled;  // show the chosen screens one after another (off by default)
        uint16_t cycle_screens;  // CYCLE_SCREENS_* bits
        uint16_t cycle_seconds;  // per screen, CYCLE_SECONDS_MIN..MAX

        // Satellites and ARISS events (services/space_manager.h).
        char space_url[96];  // own feed in the same format; "" = the project's space-feed branch
        uint8_t sat_categories;  // SAT_CATEGORIES_* bits shown and predicted
        uint8_t sat_min_el;  // degrees; lower passes are not listed
        bool pass_alert_enabled;  // LED before ISS passes with an SSTV event or school contact
        uint8_t pass_alert_min;  // minutes ahead, 1..PASS_ALERT_MIN_MAX

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
