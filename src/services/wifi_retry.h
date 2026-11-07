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
}  // namespace services
