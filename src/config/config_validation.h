#pragma once
#include <stdint.h>
#include <stddef.h>

namespace config {

    // Clamping limits to protect hardware profiles
    uint8_t clamp_brightness(int v);  // Bound between 10 and 255
    int8_t clamp_tz_hh(int v);  // Bound between -24 and +28 (represents half-hour steps)
    // Highest theme id (ui::THEME_COUNT - 1; theme.cpp static_asserts that they match).
    constexpr uint8_t THEME_ID_MAX = 6;
    uint8_t clamp_theme_id(int v);  // 0 .. THEME_ID_MAX

    // Sanitizes and upper-cases amateur radio callsigns in place
    bool normalize_callsign(char* s, size_t len);

    // Format checker for Maidenhead grid identifiers (Enforces AA00aa case layout)
    bool normalize_grid(char* s, size_t len);

    // Profile file names: [A-Za-z0-9_-], 1..24 chars (no paths, no markup).
    bool is_valid_profile_name(const char* s);

    // Own solar data URL: "" (use hamqsl.com) or http(s)://host/..., printable ASCII without spaces.
    bool is_valid_solar_url(const char* s);

    // The cycling order: true only for the 7 screen ids 0..6, each once, comma-separated, no
    // spaces (e.g. "2,0,1,3,4,5,6"); out gets them in order.
    bool parse_cycle_order(const char* s, uint8_t out[7]);

    // The next position in order (after position pos, -1 = start) whose screen is chosen in mask
    // (CYCLE_SCREENS_* bits); -1 when none is chosen.
    int cycle_next(const uint8_t order[7], uint16_t mask, int pos);

    struct Config;
    // Validate a candidate config in place before it goes live. Invalid callsign/grid/ports/
    // coordinates revert to `previous`; numeric fields are clamped; strings are terminated.
    void sanitize(Config& c, const Config& previous);

}  // namespace config
