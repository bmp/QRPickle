#pragma once
#include <stdint.h>

// How often the fallback setup AP retries the saved network. Many boots miss the first 20 s connect
// (6/10 in 2026-11 tests, on v0.2.7 and v0.2.8 alike); retry quickly for 5 minutes, then every
// minute. A connect attempt takes up to ~10 s, so 15 s leaves room. Host-safe (test/test_parsers).
namespace services {
    constexpr uint32_t STA_RETRY_FAST_MS = 15000, STA_RETRY_SLOW_MS = 60000, STA_RETRY_FAST_FOR_MS = 300000;
    inline uint32_t sta_retry_interval_ms(uint32_t since_fallback_ms) {
        return since_fallback_ms < STA_RETRY_FAST_FOR_MS ? STA_RETRY_FAST_MS : STA_RETRY_SLOW_MS;
    }

    // While the board wants its saved network (at boot or after a lost link), start a new attempt
    // when the station has given up: it reported a disconnect since our last begin() and has been
    // quiet for STA_QUIET_MS. Core 3 retries a failed first connect once, then stops on AUTH_FAIL
    // (reason 202), which some routers send to a board that reset seconds ago (2026-11-20: 1-2 of 5
    // boots, v0.3.0 too). Wrap-safe for millis().
    constexpr uint32_t STA_QUIET_MS = 3000;
    inline bool sta_retry_due(bool connected, bool disconnected_since_begin, uint32_t now_ms,
                              uint32_t last_disconnect_ms) {
        return !connected && disconnected_since_begin && now_ms - last_disconnect_ms >= STA_QUIET_MS;
    }
}  // namespace services
