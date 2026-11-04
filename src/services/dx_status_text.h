#pragma once
#include <stddef.h>
#include "dx_manager.h"

// Texts for the DX Cluster page (host-safe; test/test_parsers).
namespace services {

    // The page's status line: follows the connection, not only new spots (after a restart the
    // page showed an empty list with no explanation).
    const char* dx_status_line(DxStatus status, size_t spots_total, size_t shown);

    // One spot as one monospace line (6 px per character): call 9, frequency 9, mode 9, spotter 8,
    // then the comment; the page header's columns sit at the same x (spots.cpp)
    // (one label per row instead of five keeps the page's LVGL objects down).
    void dx_row_text(const DxSpot& s, char* out, size_t len);

}  // namespace services
