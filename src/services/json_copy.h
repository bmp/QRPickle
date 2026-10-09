#pragma once
#include <ArduinoJson.h>
#include <cstring>

// Null-safe copies from JSON into fixed char arrays (review 1.4: `strncpy(dst, doc["x"], n)`
// dereferenced nullptr when "x" was not a string, e.g. {"callsign":5}).
namespace services {

    template <size_t N> inline void copy_str(char (&dst)[N], JsonVariantConst v) {
        if (!v.is<const char*>()) return;
        strncpy(dst, v.as<const char*>(), N - 1);
        dst[N - 1] = '\0';
    }

    // Secrets are never sent to the browser (review 1.3), so an empty or placeholder value
    // means "keep the current one".
    template <size_t N> inline void copy_secret(char (&dst)[N], JsonVariantConst v) {
        if (!v.is<const char*>()) return;
        const char* s = v.as<const char*>();
        if (s[0] == '\0' || strcmp(s, "unset") == 0) return;
        strncpy(dst, s, N - 1);
        dst[N - 1] = '\0';
    }

}  // namespace services
