#pragma once
#include <stdint.h>

// Whether a visit of the xOTA page downloads, for manual and cycle visits alike; only the age limit
// differs. v0.2.8 had two paths and the manual one waited for HamAlert/APRS to stop, which they
// no longer do, so POTA never loaded when the page was opened by hand. Host-safe (test_parsers).
namespace ui {
    constexpr uint32_t XOTA_MANUAL_POTA_MAX_AGE_MS = 30000;  // PotaManager also throttles at 30 s
    constexpr uint32_t XOTA_CYCLE_POTA_MAX_AGE_MS = 5 * 60000;  // cycling shows the held list
    constexpr uint32_t XOTA_CYCLE_SOTA_MAX_AGE_MS = 10 * 60000;  // reconnect to the cluster

    // last_ms: the source's last fetch (0 = never); pota: the POTA tab (else SOTA).
    inline bool xota_should_fetch(bool cycle_visit, bool pota, uint32_t last_ms, uint32_t now_ms) {
        if (last_ms == 0) return true;
        const uint32_t age = now_ms - last_ms;
        if (pota) return age > (cycle_visit ? XOTA_CYCLE_POTA_MAX_AGE_MS : XOTA_MANUAL_POTA_MAX_AGE_MS);
        return !cycle_visit || age > XOTA_CYCLE_SOTA_MAX_AGE_MS;  // a manual SOTA visit always connects
    }
}  // namespace ui
