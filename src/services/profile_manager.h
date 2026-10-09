#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include "../config/config.h"

// Profiles are JSON files under /profiles holding every setting except the admin password,
// with the keys of /api/config (config_json.h). Writing the filesystem image erases them.
namespace services {
    namespace profile_manager {

        enum class Secrets : uint8_t {
            Own,           // only the profile's own secrets (blank if it has none): viewing/editing
            LiveFallback   // a blank profile secret keeps the device's current one: applying
        };

        std::vector<String> get_profile_list();

        // Settings missing from the file keep the live values; the result is sanitized.
        bool read_profile(const char* name, config::Config& out, Secrets secrets);

        // Creates or updates a profile from web console JSON (keys as in /api/config). An existing
        // profile is the base for an edit; a new one starts from the live settings, with the live
        // secrets unless `without_live_secrets` (restoring a backup, which never contains secrets).
        bool save_profile_from_json(const char* name, JsonObjectConst json, bool without_live_secrets);

        bool delete_profile(const char* name);

        // Call from the main loop only.
        bool apply_profile_to_live(const char* name);

    }
}
