#include "timekeeper.h"
#include "../config/config.h" // Wire directly into our new NVS storage system
#include <WiFi.h>
#include <Arduino.h>
#include <time.h>
#include <string.h>

static char utc_time_str[32] = "--:--:-- UTC";
static char local_time_str[32] = "--:--:-- LOC";
static bool ntp_requested = false;
static bool ntp_synchronized = false;

void timekeeper_init() {
    // Internal software clock tracking initialization
    Serial.println("[Timekeeper] Internal software clock initialized.");
}

void timekeeper_update() {
    // Automatically trigger native SNTP sync the moment the Wi-Fi link establishes
    if (WiFi.status() == WL_CONNECTED && !ntp_requested) {
        Serial.println("[Timekeeper] Wi-Fi Link Active. Initiating native lwIP SNTP synchronization...");
        // Configures built-in core UDP socket infrastructure; time-sync operates asynchronously in background
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        ntp_requested = true;
    }

    time_t now = time(nullptr);

    // Validate sync state: Core epoch will instantly clear 1577836800 (Jan 01 2020) once internet packet lands
    if (!ntp_synchronized && now > 1577836800) {
        ntp_synchronized = true;
        Serial.println("[Timekeeper] NTP Synchronization Successful! System clock is accurate.");
    }

    if (ntp_synchronized) {
        // Calculate local offsets by decoding packed NVS integer half-hours (e.g., 11 * 0.5 = 5.5 hours for IST)
        time_t local_epoch = now + (time_t)(config::get().tz_offset_hh * 0.5f * 3600.0f);

        struct tm timeinfo;
        gmtime_r(&local_epoch, &timeinfo);
        snprintf(local_time_str, sizeof(local_time_str), "%02d:%02d:%02d LOC",
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

        gmtime_r(&now, &timeinfo);
        snprintf(utc_time_str, sizeof(utc_time_str), "%02d:%02d:%02d UTC",
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
        // Enforce safe placeholder boundaries while waiting for the background socket transaction
        strncpy(local_time_str, "--:--:-- LOC", sizeof(local_time_str) - 1);
        strncpy(utc_time_str, "--:--:-- UTC", sizeof(utc_time_str) - 1);
    }
}

bool timekeeper_is_synced() {
    return ntp_synchronized;
}

const char* timekeeper_get_utc_string() { return utc_time_str; }
const char* timekeeper_get_local_string() { return local_time_str; }
