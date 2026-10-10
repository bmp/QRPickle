#include "config_validation.h"
#include "config.h"
#include <ctype.h>
#include <string.h>

namespace config {

    uint8_t clamp_brightness(int v) {
        if (v < 10) return 10;
        if (v > 255) return 255;
        return (uint8_t)v;
    }

    int8_t clamp_tz_hh(int v) {
        if (v < -24) return -24;
        if (v > 28) return 28;
        return (int8_t)v;
    }

    uint8_t clamp_theme_id(int v) {
        if (v < 0) return 0;
        if (v > THEME_ID_MAX) return THEME_ID_MAX;  // was 5: "E-Ink Monochrome Dark" (6) became 5
        return (uint8_t)v;
    }

    bool normalize_callsign(char* s, size_t len) {
        if (!s || len == 0) return false;
        size_t n = strnlen(s, len);
        if (n < 3 || n > 11) return false;
        for (size_t i = 0; i < n; i++) {
            char c = (char)toupper((unsigned char)s[i]);
            // Allow alphanumeric tokens and standard portable subnet dividers (/)
            if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/')) {
                return false;
            }
            s[i] = c;
        }
        return true;
    }

    bool normalize_grid(char* s, size_t len) {
        if (!s || len == 0) return false;
        size_t n = strnlen(s, len);
        if (n != 4 && n != 6) return false;

        // Positions 0 and 1 must be large geographic alpha fields (A-R)
        for (size_t i = 0; i < 2; i++) {
            char c = (char)toupper((unsigned char)s[i]);
            if (c < 'A' || c > 'R') return false;
            s[i] = c;
        }
        // Positions 2 and 3 must contain standard numeric digits (0-9)
        for (size_t i = 2; i < 4; i++) {
            if (s[i] < '0' || s[i] > '9') return false;
        }
        // Sub-square indexes 4 and 5 (if present) must be lower-case alpha fields (a-x)
        for (size_t i = 4; i < n; i++) {
            char c = (char)tolower((unsigned char)s[i]);
            if (c < 'a' || c > 'x') return false;
            s[i] = c;
        }
        return true;
    }

    bool is_valid_profile_name(const char* s) {
        if (!s) return false;
        size_t n = strnlen(s, 25);
        if (n == 0 || n > 24) return false;
        for (size_t i = 0; i < n; i++) {
            char c = s[i];
            if (!(isalnum((unsigned char)c) || c == '_' || c == '-')) return false;
        }
        return true;
    }

    // Web login and setup-AP WPA2 key: 8..16 printable ASCII, no spaces (shown on the Network screen).
    static bool valid_admin_password(const char* s) {
        size_t n = strlen(s);
        if (n < 8) return false;
        for (size_t i = 0; i < n; i++)
            if (s[i] < 0x21 || s[i] > 0x7E) return false;
        return true;
    }

    bool is_valid_solar_url(const char* s) {
        if (!s[0]) return true;
        const char* host;
        if (strncmp(s, "https://", 8) == 0) host = s + 8;
        else if (strncmp(s, "http://", 7) == 0) host = s + 7;
        else return false;
        if (!*host || *host == '/') return false;
        for (const char* p = s; *p; p++)
            if (*p < 0x21 || *p > 0x7E || *p == '"' || *p == '<' || *p == '>') return false;
        return true;
    }

    template <size_t N> static void terminate(char (&s)[N]) { s[N - 1] = '\0'; }

    void sanitize(Config& c, const Config& prev) {
        terminate(c.callsign);
        terminate(c.grid);
        terminate(c.wifi_ssid);
        terminate(c.wifi_password);
        terminate(c.openweather_api_key);
        terminate(c.dx_url_primary);
        terminate(c.dx_url_secondary);
        terminate(c.solar_url);
        terminate(c.aprs_passcode);
        terminate(c.aprs_comment);
        terminate(c.aprs_icon);
        for (auto& m : c.aprs_macros) terminate(m);
        terminate(c.hamalert_password);
        terminate(c.admin_password);

        if (!normalize_callsign(c.callsign, sizeof(c.callsign))) memcpy(c.callsign, prev.callsign, sizeof(c.callsign));
        if (!normalize_grid(c.grid, sizeof(c.grid))) memcpy(c.grid, prev.grid, sizeof(c.grid));
        c.brightness = clamp_brightness((int)c.brightness);
        c.theme_id = clamp_theme_id((int)c.theme_id);
        c.tz_offset_hh = clamp_tz_hh((int)c.tz_offset_hh);
        if (c.screen_timeout_min > 60) c.screen_timeout_min = 60;
        if (c.aprs_ssid < 0 || c.aprs_ssid > 15) c.aprs_ssid = prev.aprs_ssid;
        if (!(c.lat >= -90.0f && c.lat <= 90.0f)) c.lat = prev.lat;  // also rejects NaN
        if (!(c.lon >= -180.0f && c.lon <= 180.0f)) c.lon = prev.lon;
        c.band_groups &= BAND_GROUPS_ALL;
        if (!c.band_groups) c.band_groups = prev.band_groups ? prev.band_groups : BAND_GROUPS_ALL;  // at least one
        if (c.dx_port_primary == 0) c.dx_port_primary = prev.dx_port_primary;
        if (c.dx_port_secondary == 0) c.dx_port_secondary = prev.dx_port_secondary;
        if (!is_valid_solar_url(c.solar_url)) memcpy(c.solar_url, prev.solar_url, sizeof(c.solar_url));
        if (!valid_admin_password(c.admin_password)) memcpy(c.admin_password, prev.admin_password, sizeof(c.admin_password));
    }

}  // namespace config
