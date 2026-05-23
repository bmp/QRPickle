#pragma once
#include <ArduinoJson.h>
#include <vector>

namespace services {
    namespace profile_manager {

        struct ProfileData {
            char    callsign[12];
            char    grid[8];
            char    wifi_ssid[33];
            char    wifi_password[64];
            char    openweather_api_key[40];
            float   lat;
            float   lon;
            uint8_t brightness;
            uint8_t theme_id;
            int8_t  tz_offset_hh;
            uint8_t screen_timeout_min;

            bool    aprs_enabled;
            char    aprs_passcode[8];
            int8_t  aprs_ssid;
            char    aprs_comment[48];
            char    aprs_icon[4];
        };

        std::vector<String> get_profile_list();
        bool read_profile(const char* name, ProfileData& data);
        bool write_profile(const char* name, const ProfileData& data);
        bool save_profile_from_json(const char* name, JsonVariantConst json);
        bool apply_profile_to_live(const char* name);

    }
}