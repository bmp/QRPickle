#pragma once
#include <Arduino.h>
#include "led_pattern.h"   // LEDState and the colour/timing rules

namespace hw {
    namespace led_rgb {

        // Initializes the GPIO allocations and launches the background FreeRTOS task
        void init();

        // Updates the active underlying persistent operational state
        void set_state(LEDState new_state);

        // Triggers an immediate 30ms dim Cyan pulse for standard packet arrivals
        void trigger_traffic_pulse();

        // Triggers a high-priority 3-second White/Magenta strobe loop for APRS or HamAlert
        void trigger_priority_strobe();
        
    }
}