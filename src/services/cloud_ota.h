#pragma once
#include <Arduino.h>

namespace services {
    namespace cloud_ota {
        
        struct ReleaseInfo {
            bool update_available;
            char latest_version[16];
            char release_notes[128];
            char firmware_url[96];
        };

        // Kicks off a FreeRTOS task to silently check GitHub on boot
        void start_background_check();
        
        // Returns true if a newer version was found during the background check
        bool is_update_available();
        
        // Retrieves the cached metadata from the last check
        ReleaseInfo get_release_info();
        
        // Halts background tracking and streams the firmware into flash memory
        bool execute_firmware_flash();
    }
}
