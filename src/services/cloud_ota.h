#pragma once
#include <Arduino.h>

namespace services {
    namespace cloud_ota {

        // From ota.json, published by CI to GitHub Pages next to firmware.bin (review 2.9).
        struct ReleaseInfo {
            bool update_available;
            char latest_version[16];
            char release_notes[128];
            char firmware_url[112];
            char sha256[65];        // hex; a release without it is never flashed
        };

        // Starts a background task that reads ota.json.
        void start_background_check();

        // Re-runs the check in the background (non-blocking; review 2.5). Ignored while a
        // check or flash is already running.
        void force_update_check();

        bool is_update_available();
        bool is_check_running();
        // True once an update check has succeeded (boot retries stop then).
        bool is_check_complete();
        ReleaseInfo get_release_info();

        // Streams firmware.bin into the inactive slot and verifies its SHA-256 against ota.json
        // before switching (review 2.4). Restarts on success and on failure (review 2.3).
        bool execute_firmware_flash();
    }
}
