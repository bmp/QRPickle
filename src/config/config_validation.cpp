#include "config_validation.h"
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
    if (v > 5) return 5;
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

}  // namespace config
