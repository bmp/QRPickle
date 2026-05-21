#include "timekeeper.h"
#include "../services/wifi_manager.h"
#include <Arduino.h>
#include <time.h>

static bool ntp_configured = false;
static char utc_str[24] = "00:00:00 UTC";
static char local_str[24] = "00:00:00 LOC";

void timekeeper_init() {
    // Prime the internal clock system with standard Unix epoch zero bounds
    Serial.println("[Timekeeper] Internal software clock initialized.");
}

void timekeeper_update() {
    // Refresh text string layouts 5 times per second to capture rolling increments smoothly
    static unsigned long last_tick = 0;
    if (millis() - last_tick < 200) return;
    last_tick = millis();

    // Trigger explicit NTP sync requests only after Wi-Fi has authenticated
    if (!ntp_configured && wifi_manager_is_connected()) {
        Serial.println("[Timekeeper] Wi-Fi Link Active. Initiating NTP synchronization loop...");

        // Configures built-in system clock routines targeting worldwide pools
        // Arguments: configTime(gmtOffset_sec, daylightOffset_sec, server1, server2)
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");

        ntp_configured = true;
    }

    // Capture the current system clock tracking value
    time_t now;
    time(&now);

    // Parse raw system epoch integers into readable calendar structures
    struct tm * utc_info = gmtime(&now);
    if (utc_info && now > 100000) { // Ensure clock has received an actual valid sync sequence
        snprintf(utc_str, sizeof(utc_str), "%02d:%02d:%02d UTC",
                 utc_info->tm_hour, utc_info->tm_min, utc_info->tm_sec);
    }

    // Compute Local Time (Example: Hardcoded -5 Hour Offset for Eastern Standard Time)
    // We will build a dynamic configuration setting for this offset variable in Milestone 4!
    time_t local_epoch = now + (+5.5 * 3600);
    struct tm * local_info = gmtime(&local_epoch);
    if (local_info && now > 100000) {
        snprintf(local_str, sizeof(local_str), "%02d:%02d:%02d LOC",
                 local_info->tm_hour, local_info->tm_min, local_info->tm_sec);
    }
}

const char * timekeeper_get_utc_string() {
    return utc_str;
}

const char * timekeeper_get_local_string() {
    return local_str;
}
