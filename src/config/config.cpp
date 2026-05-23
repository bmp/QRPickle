#include "config.h"
#include "config_validation.h"
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

namespace config {

    static Config cfg;
    static const char* NS = "qrpclock"; 

    static void mask(const char* s, char* out, size_t out_len) {
        if (!s || out_len == 0) return;
        size_t n = strnlen(s, 64);
        if (n == 0) { strncpy(out, "(unset)", out_len); return; }
        strncpy(out, "****", out_len);
    }

    void reset_to_defaults() {
        memset(&cfg, 0, sizeof(cfg));
        strncpy(cfg.callsign, "N0CALL", sizeof(cfg.callsign) - 1);
        strncpy(cfg.grid,     "MK82wb", sizeof(cfg.grid) - 1);
        cfg.brightness   = 180;
        cfg.theme_id     = 0;
        cfg.tz_offset_hh = 11; 
        cfg.screen_timeout_min = 5;
        cfg.forecast_slots = 0x0F; 
        cfg.web_enabled  = true;
        cfg.wifi_ssid[0] = '\0';
        cfg.wifi_password[0] = '\0';
        cfg.openweather_api_key[0] = '\0';
        cfg.lat = 12.97f;
        cfg.lon = 77.59f;
        
        strncpy(cfg.dx_url_primary, "dxspider.co.uk", sizeof(cfg.dx_url_primary) - 1);
        cfg.dx_port_primary = 7300;
        strncpy(cfg.dx_url_secondary, "dxc.w6bgr.com", sizeof(cfg.dx_url_secondary) - 1);
        cfg.dx_port_secondary = 7373;

        cfg.aprs_enabled = false;
        cfg.aprs_passcode[0] = '\0';
        cfg.aprs_ssid = 0;
        strncpy(cfg.aprs_comment, "ESP32 Dashboard", sizeof(cfg.aprs_comment) - 1);
        strncpy(cfg.aprs_icon, "/[", sizeof(cfg.aprs_icon) - 1);
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

            if (p.isKey("dx_url_p"))  p.getString("dx_url_p",  cfg.dx_url_primary, sizeof(cfg.dx_url_primary));
            if (p.isKey("dx_port_p")) cfg.dx_port_primary = p.getUInt("dx_port_p", cfg.dx_port_primary);
            if (p.isKey("dx_url_s"))  p.getString("dx_url_s",  cfg.dx_url_secondary, sizeof(cfg.dx_url_secondary));
            if (p.isKey("dx_port_s")) cfg.dx_port_secondary = p.getUInt("dx_port_s", cfg.dx_port_secondary);

            cfg.aprs_enabled = p.getBool("aprs_en", cfg.aprs_enabled);
            cfg.aprs_ssid    = p.getChar("aprs_ssid", cfg.aprs_ssid);
            if (p.isKey("aprs_pass")) p.getString("aprs_pass", cfg.aprs_passcode, sizeof(cfg.aprs_passcode));
            if (p.isKey("aprs_cmt"))  p.getString("aprs_cmt", cfg.aprs_comment, sizeof(cfg.aprs_comment));
            if (p.isKey("aprs_icn"))  p.getString("aprs_icn", cfg.aprs_icon, sizeof(cfg.aprs_icon));
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
        
        p.putString("dx_url_p",  cfg.dx_url_primary);
        p.putUInt("dx_port_p",   cfg.dx_port_primary);
        p.putString("dx_url_s",  cfg.dx_url_secondary);
        p.putUInt("dx_port_s",   cfg.dx_port_secondary);

        p.putBool("aprs_en",     cfg.aprs_enabled);
        p.putChar("aprs_ssid",   cfg.aprs_ssid);
        p.putString("aprs_pass", cfg.aprs_passcode);
        p.putString("aprs_cmt",  cfg.aprs_comment);
        p.putString("aprs_icn",  cfg.aprs_icon);
        
        p.end();
        Serial.println("[Storage] Transaction execution successfully committed.");
    }

    const Config& get()         { return cfg; }
    Config&       mutable_get() { return cfg; }

    void log_summary() {
        char pw[16];
        mask(cfg.wifi_password, pw, sizeof(pw));

        char aprs_pw[16];
        mask(cfg.aprs_passcode, aprs_pw, sizeof(aprs_pw));

        const char* key_display = (strlen(cfg.openweather_api_key) > 0) ? "(redacted)" : "(unset)";

        Serial.printf("[Config] Callsign: %s | Grid: %s | Brightness: %u | Theme: %u | TZ Half-Hours: %d\n",
                      cfg.callsign, cfg.grid, cfg.brightness, cfg.theme_id, (int)cfg.tz_offset_hh);
        Serial.printf("         SSID: %s | Password: %s | API Key: %s\n",
                      cfg.wifi_ssid[0] ? cfg.wifi_ssid : "(unset)", pw, key_display);
        Serial.printf("         DX Cluster Primary:   %s:%u\n", cfg.dx_url_primary, cfg.dx_port_primary);
        Serial.printf("         DX Cluster Secondary: %s:%u\n", cfg.dx_url_secondary, cfg.dx_port_secondary);
        Serial.printf("         APRS-IS: %s | SSID: -%d | Passcode: %s | Icon: %s\n", 
                      cfg.aprs_enabled ? "Enabled" : "Disabled", (int)cfg.aprs_ssid, aprs_pw, cfg.aprs_icon);
    }

} // namespace config