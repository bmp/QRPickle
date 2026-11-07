#pragma once
#include <stdint.h>

// Quiet window: marks the time a TLS session or xOTA needs the memory. Until v0.2.7 it paused
// HamAlert and APRS; since v0.2.8 they stay connected (their reconnects split the heap, see
// quiet_window.cpp and docs/MEMORY.md) and holders are only counted. Kept as the one place to
// pause them again if a later feature needs it.
namespace services {
    namespace quiet {

        // Requests the pause and returns at once (stop() only asks the tasks to exit).
        void acquire();
        // True once both services have exited.
        bool settled();
        // Ends this holder's pause; the last holder restarts what was running.
        void release();

        // Blocking holder for background tasks: waits up to wait_ms for the services to exit.
        class Hold {
        public:
            explicit Hold(uint32_t wait_ms = 8000);
            ~Hold();
            Hold(const Hold&) = delete;
            Hold& operator=(const Hold&) = delete;
        };

    }  // namespace quiet
}  // namespace services
