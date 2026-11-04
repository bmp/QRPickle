#include "dx_status_text.h"
#include <stdio.h>

namespace services {

    const char* dx_status_line(DxStatus status, size_t spots_total, size_t shown) {
        switch (status) {
            case DX_STATUS_DISCONNECTED: return "OFFLINE: cluster not connected; retrying.";
            case DX_STATUS_CONNECTED:
                if (spots_total == 0) return "CONNECTED: waiting for spots...";
                return shown ? "Tap a row to read the full spot." : "No spots match the filters.";
            default: return "CONNECTING to the DX cluster...";
        }
    }

    void dx_row_text(const DxSpot& s, char* out, size_t len) {
        if (!out || !len) return;
        snprintf(out, len, "%-9.9s %9.3f %-9.9s %-8.8s %s", s.dx_call, (double)s.freq, s.mode, s.spotter, s.comment);
    }

}  // namespace services
