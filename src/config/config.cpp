#include "config.h"
#include "config_validation.h"
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>
#include <stddef.h>
#include <esp_system.h>

namespace config {

    // The single live config (review 1.10: replaces a heap pointer behind `#define cfg`).
    static Config cfg;
    static bool initialized = false;

    // One table drives NVS load and save (review 1.9). NVS keys are unchanged so existing
    // devices keep their settings. Add a field here once instead of in two lists.
    enum class Kind : uint8_t { STR, U8, I8, U16, BOOL, F32 };
    struct Field { const char* key; Kind kind; size_t offset; size_t size; };
    #define FIELD(key, kind, member) { key, Kind::kind, offsetof(Config, member), sizeof(Config::member) }
    #define FIELD_STR_AT(key, member, idx) { key, Kind::STR, offsetof(Config, member) + (idx) * sizeof(Config::member[0]), sizeof(Config::member[0]) }
    // clang-format off
    static const Field FIELDS[] = {
        FIELD("callsign",   STR,  callsign),
        FIELD("grid",       STR,  grid),
        FIELD("brightness", U8,   brightness),
        FIELD("auto_bl",    BOOL, auto_brightness),
        FIELD("theme_id",   U8,   theme_id),
        FIELD("tz_hh",      I8,   tz_offset_hh),
        FIELD("scr_to",     U8,   screen_timeout_min),
        FIELD("fc_slots",   U8,   forecast_slots),
        FIELD("web_en",     BOOL, web_enabled),
        FIELD("wifi_ssid",  STR,  wifi_ssid),
        FIELD("wifi_pw",    STR,  wifi_password),
        FIELD("ow_key",     STR,  openweather_api_key),
        FIELD("lat",        F32,  lat),
        FIELD("lon",        F32,  lon),
        FIELD("ll_set",     BOOL, latlon_set),
        FIELD("band_grp",   U8,   band_groups),
        FIELD("dx_url_p",   STR,  dx_url_primary),
        FIELD("dx_port_p",  U16,  dx_port_primary),
        FIELD("dx_url_s",   STR,  dx_url_secondary),
        FIELD("dx_port_s",  U16,  dx_port_secondary),
        FIELD("solar_url",  STR,  solar_url),
        FIELD("aprs_en",    BOOL, aprs_enabled),
        FIELD("aprs_ssid",  I8,   aprs_ssid),
        FIELD("aprs_pass",  STR,  aprs_passcode),
        FIELD("aprs_cmt",   STR,  aprs_comment),
        FIELD("aprs_icn",   STR,  aprs_icon),
        FIELD_STR_AT("mac0", aprs_macros, 0),
        FIELD_STR_AT("mac1", aprs_macros, 1),
        FIELD_STR_AT("mac2", aprs_macros, 2),
        FIELD_STR_AT("mac3", aprs_macros, 3),
        FIELD_STR_AT("mac4", aprs_macros, 4),
        FIELD("ham_pass",   STR,  hamalert_password),
        FIELD("admin_pw",   STR,  admin_password),
    };
    #undef FIELD
    #undef FIELD_STR_AT
    // clang-format on

    static void read_field(Preferences& p, const Field& f) {
        void* dst = reinterpret_cast<uint8_t*>(&cfg) + f.offset;
        switch (f.kind) {
            case Kind::STR:  p.getString(f.key, static_cast<char*>(dst), f.size); static_cast<char*>(dst)[f.size - 1] = '\0'; break;
            case Kind::U8:   *static_cast<uint8_t*>(dst)  = p.getUChar(f.key, *static_cast<uint8_t*>(dst)); break;
            case Kind::I8:   *static_cast<int8_t*>(dst)   = p.getChar(f.key, *static_cast<int8_t*>(dst)); break;
            case Kind::U16:  *static_cast<uint16_t*>(dst) = (uint16_t)p.getUInt(f.key, *static_cast<uint16_t*>(dst)); break;
            case Kind::BOOL: *static_cast<bool*>(dst)     = p.getBool(f.key, *static_cast<bool*>(dst)); break;
            case Kind::F32:  *static_cast<float*>(dst)    = p.getFloat(f.key, *static_cast<float*>(dst)); break;
        }
    }

    static void write_field(Preferences& p, const Field& f) {
        const void* src = reinterpret_cast<const uint8_t*>(&cfg) + f.offset;
        switch (f.kind) {
            case Kind::STR:  p.putString(f.key, static_cast<const char*>(src)); break;
            case Kind::U8:   p.putUChar(f.key, *static_cast<const uint8_t*>(src)); break;
            case Kind::I8:   p.putChar(f.key, *static_cast<const int8_t*>(src)); break;
            case Kind::U16:  p.putUInt(f.key, *static_cast<const uint16_t*>(src)); break;
            case Kind::BOOL: p.putBool(f.key, *static_cast<const bool*>(src)); break;
            case Kind::F32:  p.putFloat(f.key, *static_cast<const float*>(src)); break;
        }
    }

    static const char* NS = "qrpclock"; 

    static void mask(const char* s, char* out, size_t out_len) {
        if (!s || out_len == 0) return;
        size_t n = strnlen(s, 64);
        if (n == 0) { strncpy(out, "(unset)", out_len); return; }
        strncpy(out, "****", out_len);
    }

    void reset_to_defaults() {
        memset(&cfg, 0, sizeof(cfg));
        initialized = true;

        strncpy(cfg.callsign, "N0CALL", sizeof(cfg.callsign) - 1);
        strncpy(cfg.grid,     "MK82wb", sizeof(cfg.grid) - 1);
        cfg.brightness   = 180;
        cfg.auto_brightness = false;
        cfg.theme_id     = 0;
        cfg.tz_offset_hh = 11; 
        cfg.screen_timeout_min = 5;
        cfg.forecast_slots = 0x0F; 
        cfg.web_enabled  = true;
        cfg.wifi_ssid[0] = '\0';
        cfg.wifi_password[0] = '\0';
        cfg.openweather_api_key[0] = '\0';
        cfg.lat = DEFAULT_LAT;
        cfg.lon = DEFAULT_LON;
        cfg.latlon_set = false;
        cfg.band_groups = BAND_GROUPS_ALL;

        strncpy(cfg.dx_url_primary, "dxspider.co.uk", sizeof(cfg.dx_url_primary) - 1);
        cfg.dx_port_primary = 7300;
        strncpy(cfg.dx_url_secondary, "dxc.w6bgr.com", sizeof(cfg.dx_url_secondary) - 1);
        cfg.dx_port_secondary = 7373;

        cfg.aprs_enabled = false;
        cfg.aprs_passcode[0] = '\0';
        cfg.aprs_ssid = 0;
        strncpy(cfg.aprs_comment, "ESP32 Dashboard", sizeof(cfg.aprs_comment) - 1);
        strncpy(cfg.aprs_icon, "/[", sizeof(cfg.aprs_icon) - 1);

        strncpy(cfg.aprs_macros[0], "QRT. Packing up gear.", sizeof(cfg.aprs_macros[0]) - 1);
        strncpy(cfg.aprs_macros[1], "CQ POTA, spotting active now.", sizeof(cfg.aprs_macros[1]) - 1);
        strncpy(cfg.aprs_macros[2], "All OK, monitoring frequency.", sizeof(cfg.aprs_macros[2]) - 1);
        strncpy(cfg.aprs_macros[3], "Changing bands shortly.", sizeof(cfg.aprs_macros[3]) - 1);
        strncpy(cfg.aprs_macros[4], "Testing APRS-IS link.", sizeof(cfg.aprs_macros[4]) - 1);

        cfg.hamalert_password[0] = '\0';
    }

    void load() {
        if (!initialized) reset_to_defaults();

        Preferences p;
        p.begin(NS, true);
        // Settings exist once a callsign has been saved; the admin password is managed on its own.
        const bool has_settings = p.isKey("callsign");
        for (const Field& f : FIELDS) {
            bool is_admin = strcmp(f.key, "admin_pw") == 0;
            if ((has_settings || is_admin) && p.isKey(f.key)) read_field(p, f);
        }
        // Upgrade from v0.2.3 (no marker yet): lat/lon other than the factory values were entered.
        if (has_settings && !p.isKey("ll_set")) cfg.latlon_set = cfg.lat != DEFAULT_LAT || cfg.lon != DEFAULT_LON;
        p.end();

        // First boot (or upgrade): generate the web/AP password and persist it on its own.
        if (strlen(cfg.admin_password) < 8) {
            static const char CHARS[] = "abcdefghjkmnpqrstuvwxyz23456789";  // no 0/o/1/l/i
            for (int i = 0; i < 8; i++) cfg.admin_password[i] = CHARS[esp_random() % (sizeof(CHARS) - 1)];
            cfg.admin_password[8] = '\0';
            Preferences w;
            w.begin(NS, false);
            w.putString("admin_pw", cfg.admin_password);
            w.end();
        }

        cfg.brightness   = clamp_brightness((int)cfg.brightness);
        cfg.theme_id     = clamp_theme_id((int)cfg.theme_id);
        cfg.tz_offset_hh = clamp_tz_hh((int)cfg.tz_offset_hh);
    }

    void save() {
        if (!initialized) return;
        Preferences p;
        p.begin(NS, false);
        for (const Field& f : FIELDS) write_field(p, f);
        p.end();
        Serial.println("[Storage] Transaction execution successfully committed.");
    }

    const Config& get()         { return cfg; }
    Config&       mutable_get() { return cfg; }

    void log_summary() {
        if (!initialized) return;
        char pw[16]; mask(cfg.wifi_password, pw, sizeof(pw));
        char aprs_pw[16]; mask(cfg.aprs_passcode, aprs_pw, sizeof(aprs_pw));
        const char* key_display = (strlen(cfg.openweather_api_key) > 0) ? "(redacted)" : "(unset)";

        Serial.printf("[Config] Callsign: %s | Grid: %s | Brightness: %u (Auto: %s) | Theme: %u | TZ Half-Hours: %d\n",
                      cfg.callsign, cfg.grid, cfg.brightness, cfg.auto_brightness ? "ON" : "OFF", cfg.theme_id, (int)cfg.tz_offset_hh);
        Serial.printf("         SSID: %s | Password: %s | API Key: %s\n",
                      cfg.wifi_ssid[0] ? cfg.wifi_ssid : "(unset)", pw, key_display);
        Serial.printf("         DX Cluster Primary:   %s:%u\n", cfg.dx_url_primary, cfg.dx_port_primary);
        Serial.printf("         DX Cluster Secondary: %s:%u\n", cfg.dx_url_secondary, cfg.dx_port_secondary);
        Serial.printf("         APRS-IS: %s | SSID: -%d | Passcode: %s | Icon: %s\n", 
                      cfg.aprs_enabled ? "Enabled" : "Disabled", (int)cfg.aprs_ssid, aprs_pw, cfg.aprs_icon);
        // Masked like the other secrets (owner decision 2026-10-10, reversing the earlier one): the
        // password is shown on the device's Network screen.
        char admin_pw[16];
        mask(cfg.admin_password, admin_pw, sizeof(admin_pw));
        Serial.printf("         Web console login: admin / %s (see the Network screen)\n", admin_pw);
    }

} // namespace config
