    #pragma once
#include <Arduino.h>

namespace services {
    namespace display_manager {

        // Sets up the GPIO pins and logic boundaries for screen control
        void init();

        // Polled inside the main loop to handle inactivity timeouts
        void update();

        // Commands the backlight down to 0 and flags the system as asleep
        void sleep();

        // Commands the backlight back up and starts the touch "Ghost Tap" block timer
        void wake();

        // Returns true if the screen is currently dark
        bool is_sleeping();

        // Intercept query for touch.cpp: Returns true if LVGL should ignore touches right now
        bool should_block_touch();

        // Expose a dynamic hook to change the backlight PWM at runtime
        void set_brightness(uint8_t level);
    }
}
