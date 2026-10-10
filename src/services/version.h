#pragma once
#include <cctype>
#include <cstdlib>

// Compare "vMAJOR.MINOR.PATCH" (leading 'v' optional). Returns <0, 0, >0 like strcmp.
// Unparseable parts compare as 0. (Review 2.7: strcmp() offered downgrades.)
// No sscanf: it pulls ~10 KB of scanf code into the firmware (v0.2.4 size work).
namespace services {
    inline void parse_version(const char* s, int out[3]) {
        if (!s) return;
        if (*s == 'v' || *s == 'V') s++;
        for (int i = 0; i < 3; i++) {
            if (!isdigit((unsigned char)*s)) return;
            char* end;
            out[i] = (int)strtol(s, &end, 10);
            if (*end != '.') return;
            s = end + 1;
        }
    }

    inline int compare_versions(const char* a, const char* b) {
        int x[3] = {0, 0, 0}, y[3] = {0, 0, 0};
        parse_version(a, x);
        parse_version(b, y);
        for (int i = 0; i < 3; i++)
            if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
        return 0;
    }
}  // namespace services
