#include "display_manager.h"
#include "../config/config.h"
#include <lvgl.h>

#define TFT_BL_PIN 21

namespace services {
    namespace display_manager {

        static bool sleeping = false;
        static unsigned long wake_timestamp = 0;
        static constexpr uint32_t GHOST_TAP_LOCKOUT_MS = 350; 

        void init() {
            pinMode(TFT_BL_PIN, OUTPUT);
            analogWrite(TFT_BL_PIN, config::get().brightness);
        }

        void update() {
            if (sleeping) return;

            // FIXED: Pull dynamic timer target from active configuration
            uint32_t timeout_min = config::get().screen_timeout_min;
            
            // 0 means manual sleep only (bypass inactivity checks entirely)
            if (timeout_min == 0) return;

            uint32_t timeout_ms = timeout_min * 60000;

            if (lv_display_get_inactive_time(lv_display_get_default()) > timeout_ms) {
                sleep();
            }
        }

        void sleep() {
            if (sleeping) return;
            sleeping = true;
            analogWrite(TFT_BL_PIN, 0); 
            Serial.println("[DisplayManager] Screen asleep.");
        }

        void wake() {
            if (!sleeping) return;
            sleeping = false;
            wake_timestamp = millis();

            analogWrite(TFT_BL_PIN, config::get().brightness);
            lv_display_trigger_activity(lv_display_get_default());
            
            Serial.println("[DisplayManager] Hardware interrupt. Screen awakened.");
        }

        void set_brightness(uint8_t level) {
            if (!sleeping) {
                analogWrite(TFT_BL_PIN, level);
            }
        }

        bool is_sleeping() { return sleeping; }

        bool should_block_touch() {
            if (millis() - wake_timestamp < GHOST_TAP_LOCKOUT_MS) return true;
            return false;
        }
    }
}