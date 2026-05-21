#include "config_manager.h"
#include <LittleFS.h>
#include <Arduino.h>
#include <string.h>

#define CONFIG_PATH "/foxconfig.bin"

// Instantiate our primary system runtime settings scratchpad allocation
static sys_config_t runtime_config;

void config_manager_init() {
    Serial.print("[Storage] Mounting LittleFS Partition... ");

    // Attempt to mount the file system. If it fails, format the flash partition automatically.
    if (!LittleFS.begin(true)) {
        Serial.println("FAIL! File system initialization crash.");
        return;
    }
    Serial.println("SUCCESS");

    // Check if a configuration file already exists in deep flash memory
    if (LittleFS.exists(CONFIG_PATH)) {
        File file = LittleFS.open(CONFIG_PATH, "r");
        if (file) {
            size_t bytes_read = file.read((uint8_t *)&runtime_config, sizeof(sys_config_t));
            file.close();

            if (bytes_read == sizeof(sys_config_t)) {
                Serial.println("[Storage] Configuration file loaded successfully.");
                Serial.printf("          Callsign: %s | TZ Offset: %.1f\n",
                              runtime_config.station_call,
                              runtime_config.timezone_offset);
                return;
            }
        }
    }

    // Fallback: Populate baseline defaults if no file was discovered or read failed
    Serial.println("[Storage] Configuration missing or corrupt. Deploying safe factory parameters...");
    strncpy(runtime_config.wifi_ssid, "YOUR_WIFI_SSID_PLACEHOLDER", sizeof(runtime_config.wifi_ssid));
    strncpy(runtime_config.wifi_secret, "YOUR_WIFI_PASSWORD_PLACEHOLDER", sizeof(runtime_config.wifi_secret));
    strncpy(runtime_config.station_call, "N0CALL", sizeof(runtime_config.station_call));
    runtime_config.timezone_offset = 0; // Universal Time (UTC) default anchor

    // Automatically write this fresh template profile back down to the partition
    config_manager_save();
}

bool config_manager_save() {
    Serial.print("[Storage] Committing parameters to flash partition... ");
    File file = LittleFS.open(CONFIG_PATH, "w");
    if (!file) {
        Serial.println("FAIL! Unable to open file descriptors.");
        return false;
    }

    size_t bytes_written = file.write((uint8_t *)&runtime_config, sizeof(sys_config_t));
    file.close();

    if (bytes_written == sizeof(sys_config_t)) {
        Serial.println("SUCCESS");
        return true;
    } else {
        Serial.println("FAIL! Byte allocation write mismatch.");
        return false;
    }
}

sys_config_t * config_get_runtime() {
    return &runtime_config;
}
