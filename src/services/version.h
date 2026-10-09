#pragma once
#include <cstdio>

// Compare "vMAJOR.MINOR.PATCH" (leading 'v' optional). Returns <0, 0, >0 like strcmp.
// Unparseable strings compare as 0.0.0. (Review 2.7: strcmp() offered downgrades.)
namespace services {
    inline int compare_versions(const char* a, const char* b) {
        int x[3] = {0, 0, 0}, y[3] = {0, 0, 0};
        if (a && (*a == 'v' || *a == 'V')) a++;
        if (b && (*b == 'v' || *b == 'V')) b++;
        if (a) sscanf(a, "%d.%d.%d", &x[0], &x[1], &x[2]);
        if (b) sscanf(b, "%d.%d.%d", &y[0], &y[1], &y[2]);
        for (int i = 0; i < 3; i++) if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
        return 0;
    }
}
