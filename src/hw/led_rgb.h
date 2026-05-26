#pragma once
#include <Arduino.h>

namespace hw {
    namespace led_rgb {

        // Operational Status and Diagnostic System States
        enum LEDState {
            STATE_OFF,
            STATE_BOOT_HW,       // Solid Amber (Core Verification)
            STATE_BOOT_WIFI,     // Solid Blue (Network Search)
            STATE_BOOT_SYNC,     // Breathing Cyan (NTP/Services Sync)
            STATE_BOOT_READY,    // Temporary Dim Green (Operational Pass)
            STATE_WIFI_LOST,     // Breathing Magenta/Red Warning
            STATE_FAULT          // Rhythmic Triple Red Flash Loop
        };

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