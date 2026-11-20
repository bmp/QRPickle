#pragma once
// Display units and local time (issue #25). Host-safe: no Arduino, LVGL or config, so the
// native tests cover it. Readings stay metric everywhere else; only the text changes here.
// Callers pass the unit from config::get() (config::TEMP_* etc., 0 = metric); an unknown unit
// is shown as metric. Every fmt_* writes into buf (always NUL-terminated when len > 0) and
// returns the length written; NAN prints the offline form ("--°F", "-- inHg").
#include <stddef.h>
#include <stdint.h>
#include <time.h>

namespace units {

    // "21.5°C" / "70.7°F"; decimals 0 or 1.
    size_t fmt_temp(char* buf, size_t len, uint8_t unit, float celsius, uint8_t decimals);
    // "1013 hPa" / "29.92 inHg".
    size_t fmt_pres(char* buf, size_t len, uint8_t unit, float hpa);
    // "4.2 m/s" / "15 km/h" (never decimals) / "9.4 mph" / "8.2 kn".
    size_t fmt_wind(char* buf, size_t len, uint8_t unit, float mps, uint8_t decimals);
    // "12.3 km" / "7.6 mi".
    size_t fmt_dist(char* buf, size_t len, uint8_t unit, float km);
    // "14:05" / "2:05 PM", with seconds "14:05:33" / "2:05:33 PM"; h < 0 gives "--:--".
    size_t fmt_clock(char* buf, size_t len, bool h12, int h, int m, int s, bool with_seconds);

    // A distance in the chosen unit (km or miles), for labels with their own format.
    float dist_in(uint8_t unit, float km);

    const char* temp_unit(uint8_t unit);  // "°C" / "°F"
    const char* pres_unit(uint8_t unit);  // "hPa" / "inHg"
    const char* wind_unit(uint8_t unit);  // "m/s" / "km/h" / "mph" / "kn"
    const char* dist_unit(uint8_t unit);  // "km" / "mi"

    // UTC seconds plus the UTC offset in half-hours (config tz_offset_hh) as a broken-down
    // local time. False (out untouched) when utc is 0, i.e. no time yet.
    bool utc_to_local(uint32_t utc, int8_t tz_half_hours, struct tm* out);

}  // namespace units
