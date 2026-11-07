#pragma once
#include <stdint.h>

// Dead-link detection for the always-connected services (review F2, v0.3.0). HamAlert, APRS-IS,
// the DX and SOTA clusters had no idle detection: after a router restart or a NAT timeout a socket
// can stay "connected" for hours with no data. TCP keepalive (net_connect.h) finds most dead peers
// in ~90 s; this catches the rest from the last received byte. Host-safe (test/test_parsers).
namespace services {
    // APRS-IS servers send a "#" keepalive about every 20 s; the clusters are quieter.
    constexpr uint32_t LINK_SILENT_APRS_MS = 3 * 60000;
    constexpr uint32_t LINK_SILENT_DX_MS = 15 * 60000;
    // HamAlert and SOTA can be quiet for hours on a healthy link, and every reconnect splits the heap
    // (docs/MEMORY.md): TCP keepalive finds dead peers; this only catches a server that stops sending.
    constexpr uint32_t LINK_SILENT_QUIET_MS = 4 * 3600000UL;

    // True when nothing arrived for longer than limit_ms (last_rx_ms 0 = nothing yet: not judged).
    inline bool link_silent(uint32_t last_rx_ms, uint32_t now_ms, uint32_t limit_ms) {
        return last_rx_ms != 0 && now_ms - last_rx_ms > limit_ms;
    }
}  // namespace services
