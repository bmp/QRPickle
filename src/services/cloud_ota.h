#pragma once
#include <Arduino.h>

namespace services {
    namespace cloud_ota {

        struct ReleaseInfo {
            bool update_available;
            char latest_version[16];
            char release_notes[128];
            char firmware_url[96];
            char sha256_url[104];   // "firmware.bin.sha256" release asset, if published
        };

        // Starts a background task that checks GitHub for the latest release.
        void start_background_check();

        // Re-runs the check in the background (non-blocking; review 2.5). Ignored while a
        // check or flash is already running.
        void force_update_check();

        bool is_update_available();
        bool is_check_running();
        ReleaseInfo get_release_info();

        // Streams the release firmware into the inactive slot, verifying SHA-256 when the
        // release publishes it (review 2.4). Restarts on success and on failure (review 2.3).
        bool execute_firmware_flash();
    }
}
