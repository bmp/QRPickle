#include "aprs_parse.h"
#include <cctype>
#include <initializer_list>
#include <cstdlib>
#include <cstring>
#include <strings.h>

namespace services {
namespace aprs {

    static bool digit_or_space(char c) { return isdigit((unsigned char)c) || c == ' '; }

    // "DDMM.mm" / "DDDMM.mm" with ambiguity spaces -> decimal degrees
    static float to_degrees(const char* s, int deg_digits) {
        char buf[10];
        int n = deg_digits + 5;
        for (int i = 0; i < n; i++) buf[i] = (s[i] == ' ') ? '0' : s[i];
        buf[n] = '\0';
        char deg[4] = {0};
        memcpy(deg, buf, deg_digits);
        return (float)atoi(deg) + (float)atof(buf + deg_digits) / 60.0f;
    }

    bool parse_uncompressed_latlon(const char* info, float& lat, float& lon, char& table, char& symbol) {
        if (!info || strlen(info) < 20) return false;  // type + 8 lat + table + 9 lon + symbol
        const char* la = info + 1;   // DDMM.mmN
        const char* lo = info + 10;  // DDDMM.mmE
        for (int i : {0, 1, 2, 3, 5, 6}) if (!digit_or_space(la[i])) return false;
        if (la[4] != '.' || (la[7] != 'N' && la[7] != 'S')) return false;
        for (int i : {0, 1, 2, 3, 4, 6, 7}) if (!digit_or_space(lo[i])) return false;
        if (lo[5] != '.' || (lo[8] != 'E' && lo[8] != 'W')) return false;

        float lat_v = to_degrees(la, 2), lon_v = to_degrees(lo, 3);
        if (lat_v > 90.0f || lon_v > 180.0f) return false;
        lat = (la[7] == 'S') ? -lat_v : lat_v;
        lon = (lo[8] == 'W') ? -lon_v : lon_v;
        table = info[9];
        symbol = info[19];  // info[18] is the E/W of the longitude
        return true;
    }

    bool addressed_to(const char* addressee, const char* mycall) {
        if (!addressee || !mycall || !*mycall) return false;
        size_t n = strlen(mycall);
        if (strncasecmp(addressee, mycall, n) != 0) return false;
        const char* rest = addressee + n;
        if (*rest == '\0' || *rest == ' ') return true;          // exact
        if (*rest != '-') return false;                          // e.g. "VU3GLJX"
        rest++;
        if (!isalnum((unsigned char)*rest)) return false;        // "-" needs an SSID
        while (isalnum((unsigned char)*rest)) rest++;
        return *rest == '\0' || *rest == ' ';
    }

}  // namespace aprs
}  // namespace services
