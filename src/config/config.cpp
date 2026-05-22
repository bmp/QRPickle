#include "config.h"
#include "config_validation.h"
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

namespace config {

static Config cfg;
static const char* NS = "qrpclock"; 

static void mask(const char* s, char* out, size_t out_len) {
    size_t n = strnlen(s, 64);
    if (n == 0) { strncpy(out, "(unset)", out_len); return; }
    if (n <= 4) { strncpy(out, "****", out_len); return; }
    snprintf(out, out_len, "****%s", s + n - 4);
}

void reset_to_defaults() {
    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.callsign, "N0CALL", sizeof(cfg.callsign) - 1);
    strncpy(cfg.grid,     "MK82wb", sizeof(cfg.grid) - 1);
    cfg.brightness   = 180;
    cfg.theme_id     = 0;
    cfg.tz_offset_hh = 11; 
    cfg.screen_timeout_min = 5;
    cfg.forecast_slots = 0x0F; // Default to first 4 slots active (+3h, +6h, +9h, +12h)
    cfg.web_enabled  = true;
    cfg.wifi_ssid[0] = '\0';
    cfg.wifi_password[0] = '\0';
    cfg.openweather_api_key[0] = '\0';
    cfg.lat = 12.9716f;   
    cfg.lon = 77.5946f;
}

void load() {
    Preferences p;
    p.begin(NS, true); 
    reset_to_defaults();

    if (p.isKey("callsign")) {
        p.getString("callsign", cfg.callsign,      sizeof(cfg.callsign));
        p.getString("grid",     cfg.grid,          sizeof(cfg.grid));
        cfg.brightness   = p.getUChar("brightness", cfg.brightness);
        cfg.theme_id     = p.getUChar("theme_id",   cfg.theme_id);
        cfg.tz_offset_hh = p.getChar("tz_hh",       cfg.tz_offset_hh);
        cfg.screen_timeout_min = p.getUChar("scr_to", cfg.screen_timeout_min); 
        cfg.forecast_slots = p.getUChar("fc_slots", cfg.forecast_slots); 
        cfg.web_enabled  = p.getBool("web_en",      cfg.web_enabled);
        p.getString("wifi_ssid", cfg.wifi_ssid,    sizeof(cfg.wifi_ssid));
        p.getString("wifi_pw",   cfg.wifi_password,sizeof(cfg.wifi_password));
        p.getString("ow_key",    cfg.openweather_api_key, sizeof(cfg.openweather_api_key));
        cfg.lat          = p.getFloat("lat",        cfg.lat);
        cfg.lon          = p.getFloat("lon",        cfg.lon);
    }
    p.end();

    cfg.brightness   = clamp_brightness((int)cfg.brightness);
    cfg.theme_id     = clamp_theme_id((int)cfg.theme_id);
    cfg.tz_offset_hh = clamp_tz_hh((int)cfg.tz_offset_hh);
}

void save() {
    Preferences p;
    p.begin(NS, false); 
    p.putString("callsign",  cfg.callsign);
    p.putString("grid",      cfg.grid);
    p.putUChar("brightness", cfg.brightness);
    p.putUChar("theme_id",   cfg.theme_id);
    p.putChar("tz_hh",       cfg.tz_offset_hh);
    p.putUChar("scr_to",     cfg.screen_timeout_min); 
    p.putUChar("fc_slots",   cfg.forecast_slots); 
    p.putBool("web_en",      cfg.web_enabled);
    p.putString("wifi_ssid", cfg.wifi_ssid);
    p.putString("wifi_pw",   cfg.wifi_password);
    p.putString("ow_key",    cfg.openweather_api_key);
    p.putFloat("lat",        cfg.lat);
    p.putFloat("lon",        cfg.lon);
    p.end();
    Serial.println("[Storage] Transaction execution successfully committed.");
}

const Config& get()         { return cfg; }
Config&       mutable_get() { return cfg; }

void log_summary() {
    char pw[16], key[16];
    mask(cfg.wifi_password, pw, sizeof(pw));
    mask(cfg.openweather_api_key, key, sizeof(key));
    Serial.printf("[Config] Callsign: %s | Grid: %s | Brightness: %u | Theme: %u | TZ Half-Hours: %d\n",
                  cfg.callsign, cfg.grid, cfg.brightness, cfg.theme_id, (int)cfg.tz_offset_hh);
    Serial.printf("         SSID: %s | Password: %s | API Key: %s\n",
                  cfg.wifi_ssid[0] ? cfg.wifi_ssid : "(unset)", pw, key);
}

}  // namespace config