#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// How fresh each data source is (review F3/F4, v0.3.0). Nothing noticed a source that never
// succeeded (v0.2.8: POTA 0/5 while cycling; a manual xOTA visit never loaded), and only the band
// conditions screen showed its data's age. Every source records ok()/fail(); /api/status reports
// the state, screens show the age, and the test scripts fail on FAILING. Host-safe (test_parsers).
namespace services {

    struct Freshness {
        uint32_t last_ok_ms = 0;  // millis() of the last success (0 = never)
        uint32_t last_try_ms = 0;  // millis() of the last attempt (0 = never)
        uint16_t fails = 0;  // failures since the last success
        void ok(uint32_t now) {
            last_ok_ms = now ? now : 1;
            last_try_ms = last_ok_ms;
            fails = 0;
        }
        void fail(uint32_t now) {
            last_try_ms = now ? now : 1;
            if (fails < 0xFFFF) fails++;
        }
    };

    enum class FreshState : uint8_t { NONE,
                                      FRESH,
                                      STALE,
                                      FAILING };
    constexpr uint16_t FRESH_FAILS_FAILING = 3;

    // FRESH: a success within 2 x interval. STALE: older. FAILING: 3+ failures in a row and nothing
    // fresh. NONE: nothing yet (and fewer than 3 failures).
    inline FreshState fresh_state(const Freshness& f, uint32_t now_ms, uint32_t interval_ms) {
        const bool fresh = f.last_ok_ms && now_ms - f.last_ok_ms <= 2 * interval_ms;
        if (fresh) return FreshState::FRESH;
        if (f.fails >= FRESH_FAILS_FAILING) return FreshState::FAILING;
        return f.last_ok_ms ? FreshState::STALE : FreshState::NONE;
    }

    inline const char* fresh_name(FreshState s) {
        switch (s) {
            case FreshState::FRESH:   return "FRESH";
            case FreshState::STALE:   return "STALE";
            case FreshState::FAILING: return "FAILING";
            default:                  return "NONE";
        }
    }

    // "just now", "5 min ago", "2 h ago".
    inline void age_text(char* out, size_t n, uint32_t age_s) {
        if (age_s < 60) snprintf(out, n, "just now");
        else if (age_s < 3600) snprintf(out, n, "%u min ago", (unsigned)(age_s / 60));
        else snprintf(out, n, "%u h ago", (unsigned)(age_s / 3600));
    }

}  // namespace services
