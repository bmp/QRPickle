#pragma once
#include <stdint.h>

// Quiet window: HamAlert and APRS paused while a TLS session (~40 KB contiguous heap) or xOTA needs
// the memory. Holders are counted: the first one pauses the services, the last one to release
// restarts the ones that were running before. Without the count, one holder finishing restarted
// the services in the middle of another's TLS handshake ("SSL - Memory allocation failed").
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
