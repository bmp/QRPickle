#include "quiet_window.h"
#include "aprs_manager.h"
#include "hamalert_manager.h"
#include <Arduino.h>

namespace services {
    namespace quiet {

        static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
        static int holders = 0;
        static bool ham_was = false, aprs_was = false;

        void acquire() {
            portENTER_CRITICAL(&mux);
            const bool first = holders++ == 0;
            portEXIT_CRITICAL(&mux);
            if (!first) return;
            ham_was = !HamAlertManager::is_stopped();
            aprs_was = !AprsManager::is_stopped();
            HamAlertManager::stop();
            AprsManager::stop();
        }

        bool settled() { return HamAlertManager::is_stopped() && AprsManager::is_stopped(); }

        void release() {
            portENTER_CRITICAL(&mux);
            const bool last = holders > 0 && --holders == 0;
            portEXIT_CRITICAL(&mux);
            if (!last) return;
            if (ham_was) HamAlertManager::start();
            if (aprs_was) AprsManager::start();
        }

        Hold::Hold(uint32_t wait_ms) {
            acquire();
            for (uint32_t t = 0; t < wait_ms && !settled(); t += 100) vTaskDelay(pdMS_TO_TICKS(100));
        }

        Hold::~Hold() { release(); }

    }  // namespace quiet
}  // namespace services
