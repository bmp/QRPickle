#pragma once
#include <stdint.h>

namespace config {

struct Config {
    char    callsign[12];
    char    grid[8];
    uint8_t brightness;
    uint8_t theme_id;
    int8_t  tz_offset_hh;     // Internal tracking offset tracking via half-hours (-24..+28)
    bool    web_enabled;

    char    wifi_ssid[33];
    char    wifi_password[64];
    char    openweather_api_key[40];
    float   lat;
    float   lon;
};

// Initializes the NVS memory space and maps values directly into RAM cache
void load();

// Commits the current active configurations down to safe storage sectors
void save();

// Safe runtime lookup macros
const Config& get();
Config& mutable_get();

// Flushes localized allocations back to standard out-of-box configurations
void reset_to_defaults();

// Formatted safe diagnostic monitor logs (Masks sensitive credentials automatically)
void log_summary();

}  // namespace config
