#pragma once
#include <stdint.h>

namespace config {

struct Config {
    char    callsign[12];
    char    grid[8];
    uint8_t brightness;
    uint8_t theme_id;
    int8_t  tz_offset_hh;     
    uint8_t screen_timeout_min; 
    uint8_t forecast_slots;     
    bool    web_enabled;

    char    wifi_ssid[33];
    char    wifi_password[64];
    char    openweather_api_key[40];
    float   lat;
    float   lon;

    char dx_url_primary[64];
    uint16_t dx_port_primary;
    char dx_url_secondary[64];
    uint16_t dx_port_secondary;

    bool    aprs_enabled;
    char    aprs_passcode[8]; 
    int8_t  aprs_ssid;        
    char    aprs_comment[48]; 
    char    aprs_icon[4];     
    char    aprs_macros[5][64];

    char    hamalert_password[33];
};

void load();
void save();
const Config& get();
Config& mutable_get();
void reset_to_defaults();
void log_summary();

}  // namespace config
