#pragma once
#include <ArduinoJson.h>
#include "config.h"

// One JSON field table for the web API (/api/config) and profile files (review 1.9).
// The admin password is not in the table: profiles never hold it and the API only accepts it
// explicitly ("admin_pw" in /api/config/save).
namespace config {

    enum class Secrets : uint8_t {
        Mask,     // "" plus "<key>_set": true/false (web API, review 1.3)
        Include   // the stored value (profile files on the device only)
    };

    void to_json(const Config& c, JsonObject out, Secrets secrets);

    // Keys that are missing or of the wrong type keep c's value; a blank or "unset" secret keeps
    // c's secret. Numbers are range-limited to their field; call sanitize() afterwards.
    void from_json(Config& c, JsonObjectConst in);

    // Blanks WiFi password, API key, APRS passcode and HamAlert password.
    void clear_secrets(Config& c);

}  // namespace config
