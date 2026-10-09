#include "profile_manager.h"
#include "../config/config_json.h"
#include "../config/config_validation.h"
#include <LittleFS.h>

namespace services {
    namespace profile_manager {

        static String path_for(const char* name) {
            return "/profiles/" + String(name) + ".json";
        }

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

        bool read_profile(const char* name, config::Config& out, Secrets secrets) {
            if (!config::is_valid_profile_name(name)) return false;  // review 1.7
            File file = LittleFS.open(path_for(name), "r");
            if (!file) return false;
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, file);
            file.close();
            if (err || !doc.is<JsonObject>()) return false;
            // Profiles written before 0.2.0 used "scr_to" for the sleep timeout.
            if (doc["timeout"].isNull() && !doc["scr_to"].isNull()) doc["timeout"] = doc["scr_to"];

            const config::Config& live = config::get();
            out = live;
            if (secrets == Secrets::Own) config::clear_secrets(out);
            config::from_json(out, doc.as<JsonObjectConst>());
            config::sanitize(out, live);
            return true;
        }

        static bool write_profile(const char* name, const config::Config& c) {
            if (!config::is_valid_profile_name(name)) return false;
            if (!LittleFS.exists("/profiles")) LittleFS.mkdir("/profiles");
            File file = LittleFS.open(path_for(name), "w");
            if (!file) return false;
            JsonDocument doc;
            config::to_json(c, doc.to<JsonObject>(), config::Secrets::Include);
            bool ok = serializeJson(doc, file) > 0;
            file.close();
            return ok;
        }

        bool save_profile_from_json(const char* name, JsonObjectConst json, bool without_live_secrets) {
            if (!config::is_valid_profile_name(name)) return false;
            config::Config c;
            if (!read_profile(name, c, Secrets::Own)) {
                c = config::get();
                if (without_live_secrets) config::clear_secrets(c);
            }
            config::from_json(c, json);
            config::sanitize(c, config::get());
            return write_profile(name, c);
        }

        bool delete_profile(const char* name) {
            if (!config::is_valid_profile_name(name)) return false;
            return LittleFS.remove(path_for(name));
        }

        bool apply_profile_to_live(const char* name) {
            config::Config c;
            if (!read_profile(name, c, Secrets::LiveFallback)) return false;
            config::mutable_get() = c;  // admin password: read_profile starts from the live one
            config::save();
            return true;
        }
    }
}
