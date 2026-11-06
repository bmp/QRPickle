#include "quiet_window.h"
#include <Arduino.h>

namespace services {
    namespace quiet {

        static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
        static int holders = 0;

        // HamAlert and APRS stay connected during downloads (2026-11-06): stopping them before each
        // HTTPS job and reconnecting after it made every reconnect place new socket buffers (1.4 KB
        // each, plus lwIP records) in the middle of the space TLS had just freed, which split the
        // heap's largest block (heap maps in docs/MEMORY_AUDIT.md). Kept as an interface: a later
        // need to pause them can go here again.
        void acquire() {
            portENTER_CRITICAL(&mux);
            holders++;
            portEXIT_CRITICAL(&mux);
        }

        bool settled() { return true; }

        void release() {
            portENTER_CRITICAL(&mux);
            if (holders > 0) holders--;
            portEXIT_CRITICAL(&mux);
        }

        Hold::Hold(uint32_t) { acquire(); }

        Hold::~Hold() { release(); }

    }  // namespace quiet
}  // namespace services
