#pragma once
#include <Arduino.h>
#include <vector>
#include <ArduinoJson.h> // Included to allow direct network payload mapping

namespace services {
    namespace profile_manager {

        struct ProfileData {
            char callsign[12] = "";
            char grid[8] = "";
            char wifi_ssid[34] = "";
            char wifi_password[64] = "";
            char openweather_api_key[45] = "";
            float lat = 0.0f;
            float lon = 0.0f;
            uint8_t brightness = 128;
            uint8_t theme_id = 0;
            int8_t tz_offset_hh = 0;
            uint8_t screen_timeout_min = 5;
            bool    aprs_enabled;
            char    aprs_passcode[8];
            int8_t  aprs_ssid;
        };

        std::vector<String> get_profile_list();
        bool read_profile(const char* name, ProfileData& data);
        bool write_profile(const char* name, const ProfileData& data);
        
        // FIXED: Added direct extraction method to remove business logic from Web Server
        bool save_profile_from_json(const char* name, JsonVariantConst json);
        
        bool apply_profile_to_live(const char* name);
    }
}
