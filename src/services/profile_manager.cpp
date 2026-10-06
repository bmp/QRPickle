#include "profile_manager.h"
#include "../config/config.h"
#include "../config/config_validation.h"
#include "json_copy.h"
#include <LittleFS.h>

namespace services {
    namespace profile_manager {

        std::vector<String> get_profile_list() {
            std::vector<String> list;
            File dir = LittleFS.open("/profiles", "r");
            if (!dir || !dir.isDirectory()) return list;

            File file = dir.openNextFile();
            while (file) {
                String name = String(file.name());
                if (name.endsWith(".json")) {
                    name.replace(".json", "");
                    list.push_back(name);
                }
                file = dir.openNextFile();
            }
            dir.close();
            return list;
        }

        bool read_profile(const char* name, ProfileData& data) {
            if (!config::is_valid_profile_name(name)) return false;  // review 1.7
            String path = "/profiles/" + String(name) + ".json";
            File file = LittleFS.open(path, "r");
            if (!file) return false;

            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, file);
            file.close();
            if (err) return false;

            services::copy_str(data.callsign, doc["callsign"]);
            services::copy_str(data.grid, doc["grid"]);
            services::copy_str(data.wifi_ssid, doc["ssid"]);
            services::copy_str(data.wifi_password, doc["password"]);
            services::copy_str(data.openweather_api_key, doc["apikey"]);
            if (!doc["lat"].isNull())        data.lat = doc["lat"].as<float>();
            if (!doc["lon"].isNull())        data.lon = doc["lon"].as<float>();
            if (!doc["brightness"].isNull()) data.brightness = doc["brightness"].as<uint8_t>();
            if (!doc["theme_id"].isNull())   data.theme_id = doc["theme_id"].as<uint8_t>();
            if (!doc["offset"].isNull())     data.tz_offset_hh = (int8_t)(doc["offset"].as<float>() * 2.0f);
            if (!doc["scr_to"].isNull())     data.screen_timeout_min = doc["scr_to"].as<uint8_t>();

            if (!doc["aprs_en"].isNull())    data.aprs_enabled = doc["aprs_en"].as<bool>();
            if (!doc["aprs_ssid"].isNull())  data.aprs_ssid = doc["aprs_ssid"].as<int8_t>();
            services::copy_str(data.aprs_passcode, doc["aprs_pass"]);
            services::copy_str(data.aprs_comment, doc["aprs_cmt"]);
            services::copy_str(data.aprs_icon, doc["aprs_icn"]);

            return true;
        }

        bool write_profile(const char* name, const ProfileData& data) {
            if (!config::is_valid_profile_name(name)) return false;  // review 1.7
            String p_name = String(name);
            p_name.replace(" ", "_");
            String path = "/profiles/" + p_name + ".json";

            File file = LittleFS.open(path, "w");
            if (!file) return false;

            JsonDocument doc;
            doc["callsign"] = data.callsign;
            doc["grid"] = data.grid;
            doc["ssid"] = data.wifi_ssid;
            doc["password"] = data.wifi_password;
            doc["apikey"] = data.openweather_api_key;
            doc["lat"] = data.lat;
            doc["lon"] = data.lon;
            doc["offset"] = (float)data.tz_offset_hh / 2.0f;
            doc["brightness"] = data.brightness;
            doc["theme_id"] = data.theme_id;
            doc["scr_to"] = data.screen_timeout_min;

            doc["aprs_en"]   = data.aprs_enabled;
            doc["aprs_ssid"] = data.aprs_ssid;
            doc["aprs_pass"] = data.aprs_passcode;
            doc["aprs_cmt"]  = data.aprs_comment;
            doc["aprs_icn"]  = data.aprs_icon;

            serializeJson(doc, file);
            file.close();
            return true;
        }

        bool save_profile_from_json(const char* name, JsonVariantConst json) {
            ProfileData p_data;
            services::copy_str(p_data.callsign, json["callsign"]);
            services::copy_str(p_data.grid, json["grid"]);
            services::copy_str(p_data.wifi_ssid, json["ssid"]);
            services::copy_str(p_data.wifi_password, json["password"]);
            services::copy_str(p_data.openweather_api_key, json["apikey"]);
            if (!json["lat"].isNull())        p_data.lat = json["lat"].as<float>();
            if (!json["lon"].isNull())        p_data.lon = json["lon"].as<float>();
            if (!json["brightness"].isNull()) p_data.brightness = json["brightness"].as<uint8_t>();
            if (!json["theme_id"].isNull())   p_data.theme_id = json["theme_id"].as<uint8_t>();
            if (!json["offset"].isNull())     p_data.tz_offset_hh = (int8_t)(json["offset"].as<float>() * 2.0f);
            if (!json["scr_to"].isNull())     p_data.screen_timeout_min = json["scr_to"].as<uint8_t>();

            if (!json["aprs_en"].isNull())    p_data.aprs_enabled = json["aprs_en"].as<bool>();
            if (!json["aprs_ssid"].isNull())  p_data.aprs_ssid = json["aprs_ssid"].as<int8_t>();
            services::copy_str(p_data.aprs_passcode, json["aprs_pass"]);
            services::copy_str(p_data.aprs_comment, json["aprs_cmt"]);
            services::copy_str(p_data.aprs_icon, json["aprs_icn"]);

            // The UI no longer receives secrets (review 1.3): blank/"unset" means "use the live value".
            const auto& live = config::get();
            if (!p_data.wifi_password[0] || !strcmp(p_data.wifi_password, "unset"))
                strncpy(p_data.wifi_password, live.wifi_password, sizeof(p_data.wifi_password) - 1);
            if (!p_data.openweather_api_key[0] || !strcmp(p_data.openweather_api_key, "unset"))
                strncpy(p_data.openweather_api_key, live.openweather_api_key, sizeof(p_data.openweather_api_key) - 1);
            if (!p_data.aprs_passcode[0] || !strcmp(p_data.aprs_passcode, "unset"))
                strncpy(p_data.aprs_passcode, live.aprs_passcode, sizeof(p_data.aprs_passcode) - 1);
            return write_profile(name, p_data);
        }

        bool apply_profile_to_live(const char* name) {
            ProfileData data;
            if (!read_profile(name, data)) return false;

            const config::Config before = config::get();
            auto& c = config::mutable_get();
            strncpy(c.callsign, data.callsign, sizeof(c.callsign) - 1);
            strncpy(c.grid, data.grid, sizeof(c.grid) - 1);
            strncpy(c.wifi_ssid, data.wifi_ssid, sizeof(c.wifi_ssid) - 1);
            strncpy(c.wifi_password, data.wifi_password, sizeof(c.wifi_password) - 1);
            strncpy(c.openweather_api_key, data.openweather_api_key, sizeof(c.openweather_api_key) - 1);
            c.lat = data.lat;
            c.lon = data.lon;
            c.brightness = data.brightness;
            c.theme_id = data.theme_id;
            c.tz_offset_hh = data.tz_offset_hh;
            c.screen_timeout_min = data.screen_timeout_min;

            c.aprs_enabled = data.aprs_enabled;
            c.aprs_ssid = data.aprs_ssid;
            if (strlen(data.aprs_passcode) > 0) {
                strncpy(c.aprs_passcode, data.aprs_passcode, sizeof(c.aprs_passcode) - 1);
            }
            strncpy(c.aprs_comment, data.aprs_comment, sizeof(c.aprs_comment) - 1);
            strncpy(c.aprs_icon, data.aprs_icon, sizeof(c.aprs_icon) - 1);
            config::sanitize(c, before);

            config::save();
            return true;
        }
    }
}