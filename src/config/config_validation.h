#pragma once
#include <stdint.h>
#include <stddef.h>

namespace config {

// Clamping limits to protect hardware profiles
uint8_t clamp_brightness(int v);    // Bound between 10 and 255
int8_t  clamp_tz_hh(int v);         // Bound between -24 and +28 (represents half-hour steps)
uint8_t clamp_theme_id(int v);      // Bound between 0 and 5 matching our theme table arrays

// Sanitizes and upper-cases amateur radio callsigns in place
bool normalize_callsign(char* s, size_t len);

// Format checker for Maidenhead grid identifiers (Enforces AA00aa case layout)
bool normalize_grid(char* s, size_t len);

}  // namespace config
