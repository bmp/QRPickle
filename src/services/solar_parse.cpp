#include "solar_parse.h"
#include <ctype.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace services {
    namespace solar {

        // Copies the trimmed text of the first <tag>...</tag> after `from` into out. False if absent.
        static bool tag_text(const char* from, const char* tag, char* out, size_t out_len) {
            char open[32];
            snprintf(open, sizeof(open), "<%s>", tag);
            const char* p = strstr(from, open);
            if (!p) return false;
            p += strlen(open);
            const char* end = strstr(p, "</");
            if (!end) return false;
            while (p < end && isspace((unsigned char)*p)) p++;
            while (end > p && isspace((unsigned char)end[-1])) end--;
            size_t n = (size_t)(end - p);
            if (n >= out_len) n = out_len - 1;
            memcpy(out, p, n);
            out[n] = '\0';
            return true;
        }

        // Whole-number field; NONE when missing or not a number ("No Report").
        static int16_t tag_int(const char* from, const char* tag) {
            char buf[16];
            if (!tag_text(from, tag, buf, sizeof(buf)) || !buf[0]) return NONE;
            char* end;
            long v = strtol(buf, &end, 10);
            if (end == buf || v < 0 || v > 9999) return NONE;
            return (int16_t)v;
        }

        static float tag_float(const char* from, const char* tag, float none) {
            char buf[16];
            if (!tag_text(from, tag, buf, sizeof(buf)) || !buf[0]) return none;
            char* end;
            float v = strtof(buf, &end);
            return (end == buf || !isfinite(v)) ? none : v;
        }

        bool parse_xml(const char* xml, SolarData& out) {
            memset(&out, 0, sizeof(out));
            out.sfi = out.sunspots = out.a_index = out.k_index = out.aurora = NONE;
            out.solar_wind = -1.0f;
            out.bz = NAN;
            if (!xml) return false;
            const char* d = strstr(xml, "<solardata>");
            if (!d) return false;

            char buf[32];
            if (tag_text(d, "updated", buf, sizeof(buf))) out.updated_utc = parse_updated(buf);
            out.sfi = tag_int(d, "solarflux");
            out.sunspots = tag_int(d, "sunspots");
            out.a_index = tag_int(d, "aindex");
            out.k_index = tag_int(d, "kindex");
            if (out.k_index > 9) out.k_index = NONE;
            out.aurora = tag_int(d, "aurora");
            out.solar_wind = tag_float(d, "solarwind", -1.0f);
            out.bz = tag_float(d, "magneticfield", NAN);
            tag_text(d, "xray", out.xray, sizeof(out.xray));
            tag_text(d, "geomagfield", out.geomag, sizeof(out.geomag));
            tag_text(d, "signalnoise", out.noise, sizeof(out.noise));
            tag_text(d, "muf", out.muf, sizeof(out.muf));
            if (strcmp(out.muf, "NoRpt") == 0) out.muf[0] = '\0';

            out.valid = out.sfi > 0 && out.k_index != NONE;
            return out.valid;
        }

        // Days since 1970-01-01 for a proleptic Gregorian date (H. Hinnant's days_from_civil).
        static int32_t days_from_civil(int32_t y, unsigned m, unsigned d) {
            y -= m <= 2;
            const int32_t era = (y >= 0 ? y : y - 399) / 400;
            const unsigned yoe = (unsigned)(y - era * 400);
            const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
            const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
            return era * 146097 + (int32_t)doe - 719468;
        }

        uint32_t parse_updated(const char* s) {
            static const char* MONTHS = "JanFebMarAprMayJunJulAugSepOctNovDec";
            int day, year, hhmm;
            char mon[4];
            if (!s || sscanf(s, " %d %3s %d %d", &day, mon, &year, &hhmm) != 4) return 0;
            const char* m = strstr(MONTHS, mon);
            if (!m || strlen(mon) != 3 || (m - MONTHS) % 3) return 0;
            const int hh = hhmm / 100, mm = hhmm % 100;
            if (day < 1 || day > 31 || year < 2000 || year > 2100 || hh > 23 || mm > 59) return 0;
            const int32_t days = days_from_civil(year, (unsigned)((m - MONTHS) / 3 + 1), (unsigned)day);
            return (uint32_t)days * 86400u + (uint32_t)(hh * 3600 + mm * 60);
        }

        bool is_storm(const SolarData& d) {
            return (d.k_index != NONE && d.k_index >= 5) || d.xray[0] == 'M' || d.xray[0] == 'X';
        }

        // Seconds since t; 0 if the clock has since been set back a little (NTP corrections), a very
        // large value if t is unset or more than a day in the future (the clock was wrong then).
        static uint32_t since(uint32_t now, uint32_t t) {
            if (!t || (t > now && t - now > 86400)) return UINT32_MAX;
            return now >= t ? now - t : 0;
        }

        bool fetch_due(const Schedule& s, uint32_t now_utc) {
            if (since(now_utc, s.last_attempt_utc) < RETRY_S) return false;
            if (!s.last_ok_utc) return true;
            if (since(now_utc, s.last_ok_utc) < MIN_INTERVAL_S) return false;
            if (s.storm) return true;
            if (now_utc < SLOT_OFFSET_S) return false;
            const uint32_t t = now_utc - SLOT_OFFSET_S;
            const uint32_t slot_start = t - t % SLOT_S + SLOT_OFFSET_S;
            return s.last_ok_utc < slot_start;
        }

        uint32_t manual_unlock_utc(const Schedule& s, uint32_t now_utc) {
            if (manual_allowed(s, now_utc)) return 0;
            uint32_t t = s.last_ok_utc ? s.last_ok_utc + MIN_INTERVAL_S : 0;
            if (s.last_attempt_utc && s.last_attempt_utc + 60 > t) t = s.last_attempt_utc + 60;
            return t;
        }

        bool manual_allowed(const Schedule& s, uint32_t now_utc) {
            if (since(now_utc, s.last_attempt_utc) < 60) return false;
            return since(now_utc, s.last_ok_utc) >= MIN_INTERVAL_S;
        }

    }  // namespace solar
}  // namespace services
