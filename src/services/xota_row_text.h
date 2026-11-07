#pragma once
#include <stddef.h>
#include <stdio.h>

// One POTA/SOTA spot as one monospace line for the xOTA page (host-safe; test/test_parsers):
// time 5, reference 10, frequency 7 (MHz, right-aligned), mode 4, activator 12; one space between.
// The page header's columns sit at the same x (6 px per character). One label per row instead of
// five (the page reached 49 KB of the LVGL pool, 2026-11-06).
namespace services {

    inline void xota_row_text(const char* time, const char* ref, float freq_mhz, const char* mode, const char* activator,
                              char* out, size_t len) {
        if (!out || !len) return;
        snprintf(out, len, "%-5.5s %-10.10s %7.3f %-4.4s %.12s", time, ref, (double)freq_mhz, mode, activator);
    }

}  // namespace services
