#include "units.h"
#include <math.h>
#include <stdio.h>
#include "../config/config.h"  // the unit enums only (plain header, host-safe)

namespace units {

    namespace {
        using config::DIST_MI;
        using config::PRES_INHG;
        using config::TEMP_F;
        using config::WIND_KMH;
        using config::WIND_KN;
        using config::WIND_MPH;

        size_t finish(char* buf, size_t len, int n) {
            if (n < 0) {
                buf[0] = '\0';
                return 0;
            }
            return (size_t)n < len ? (size_t)n : len - 1;
        }

        // Rounded to `decimals`, without "-0" for values that round to zero.
        float rounded(float v, uint8_t decimals) {
            const float scale = decimals ? 10.0f : 1.0f;
            const float r = roundf(v * scale) / scale;
            return r == 0.0f ? 0.0f : r;
        }

        // "<value><sep><unit>", or "--<sep><unit>" when v is NAN.
        size_t number(char* buf, size_t len, float v, uint8_t decimals, const char* sep, const char* unit) {
            if (!buf || !len) return 0;
            if (isnan(v)) return finish(buf, len, snprintf(buf, len, "--%s%s", sep, unit));
            return finish(buf, len, snprintf(buf, len, "%.*f%s%s", decimals, (double)rounded(v, decimals), sep, unit));
        }
    }  // namespace

    const char* temp_unit(uint8_t unit) { return unit == TEMP_F ? "\xC2\xB0"
                                                                  "F"
                                                                : "\xC2\xB0"
                                                                  "C"; }
    const char* pres_unit(uint8_t unit) { return unit == PRES_INHG ? "inHg" : "hPa"; }
    const char* dist_unit(uint8_t unit) { return unit == DIST_MI ? "mi" : "km"; }
    const char* wind_unit(uint8_t unit) {
        switch (unit) {
            case WIND_KMH: return "km/h";
            case WIND_MPH: return "mph";
            case WIND_KN:  return "kn";
            default:       return "m/s";
        }
    }

    size_t fmt_temp(char* buf, size_t len, uint8_t unit, float c, uint8_t decimals) {
        const float v = unit == TEMP_F ? c * 9.0f / 5.0f + 32.0f : c;
        return number(buf, len, v, decimals ? 1 : 0, "", temp_unit(unit));
    }

    size_t fmt_pres(char* buf, size_t len, uint8_t unit, float hpa) {
        if (unit == PRES_INHG) {
            if (!buf || !len) return 0;
            if (isnan(hpa)) return finish(buf, len, snprintf(buf, len, "-- inHg"));
            return finish(buf, len, snprintf(buf, len, "%.2f inHg", (double)(hpa * 0.029530f)));
        }
        return number(buf, len, hpa, 0, " ", "hPa");
    }

    size_t fmt_wind(char* buf, size_t len, uint8_t unit, float mps, uint8_t decimals) {
        float v = mps;
        switch (unit) {
            case WIND_KMH:
                v = mps * 3.6f;
                decimals = 0;
                break;
            case WIND_MPH: v = mps * 2.23694f; break;
            case WIND_KN:  v = mps * 1.94384f; break;
            default:       break;
        }
        return number(buf, len, v, decimals ? 1 : 0, " ", wind_unit(unit));
    }

    float dist_in(uint8_t unit, float km) { return unit == DIST_MI ? km * 0.621371f : km; }

    size_t fmt_dist(char* buf, size_t len, uint8_t unit, float km) {
        return number(buf, len, dist_in(unit, km), 1, " ", dist_unit(unit));
    }

    size_t fmt_clock(char* buf, size_t len, bool h12, int h, int m, int s, bool with_seconds) {
        if (!buf || !len) return 0;
        if (h < 0) return finish(buf, len, snprintf(buf, len, "--:--"));
        if (!h12) {
            if (with_seconds) return finish(buf, len, snprintf(buf, len, "%02d:%02d:%02d", h, m, s));
            return finish(buf, len, snprintf(buf, len, "%02d:%02d", h, m));
        }
        const char* ampm = h < 12 ? "AM" : "PM";
        const int h12v = h % 12 == 0 ? 12 : h % 12;  // 00:xx is 12:xx AM, 12:xx is 12:xx PM
        if (with_seconds) return finish(buf, len, snprintf(buf, len, "%d:%02d:%02d %s", h12v, m, s, ampm));
        return finish(buf, len, snprintf(buf, len, "%d:%02d %s", h12v, m, ampm));
    }

    bool utc_to_local(uint32_t utc, int8_t tz_half_hours, struct tm* out) {
        if (utc == 0 || !out) return false;
        const time_t local = (time_t)utc + (time_t)tz_half_hours * 1800;
        return gmtime_r(&local, out) != nullptr;
    }

}  // namespace units
