#pragma once
#include <stdint.h>

// What the main loop does about NetLock being held for long (net_lock.h). A download that never
// returns keeps the lock, every other download is skipped and, in a heap-map test run (2026-11-06),
// the board stopped answering on the network for over an hour while its screens kept cycling.
// Every download takes seconds; a Cloud OTA install holds the lock longer, but the main loop is
// parked then and doesn't check. Host-safe (test/test_parsers).
namespace services {

    enum class LockWatch { OK,
                           WARN,
                           RESTART };
    constexpr unsigned LOCK_WARN_S = 60;
    constexpr unsigned LOCK_RESTART_S = 180;

    // Seconds the lock has been held. The start time is stored as millis() | 1 (0 means free), so it
    // can be 1 ms after the watch's own millis(); the unsigned difference then wrapped to 4294967 s
    // and restarted the board three times in the v0.2.8 stress run. A start "in the future" is 0.
    inline unsigned lock_held_s(uint32_t since_ms, uint32_t now_ms) {
        const int32_t d = static_cast<int32_t>(now_ms - since_ms);
        return d > 0 ? static_cast<unsigned>(d) / 1000 : 0;
    }

    inline LockWatch lock_watch(unsigned held_s) {
        if (held_s > LOCK_RESTART_S) return LockWatch::RESTART;
        if (held_s > LOCK_WARN_S) return LockWatch::WARN;
        return LockWatch::OK;
    }

}  // namespace services
