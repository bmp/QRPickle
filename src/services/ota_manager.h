#pragma once
#include <Arduino.h>

namespace services {
    namespace ota_manager {

        enum UpdateType {
            UPDATE_TYPE_FIRMWARE,
            UPDATE_TYPE_FILESYSTEM,
            UPDATE_TYPE_UNKNOWN
        };

        // Starts an update into the inactive app slot (firmware) or the LittleFS partition.
        // The size is taken from the partition table (review 2.1/2.2).
        bool begin(UpdateType type);
        bool write_chunk(uint8_t* data, size_t len);
        // Checks the image is complete and switches the boot partition (firmware).
        // Note: this is not a signature check.
        bool end();
        void abort();
        const char* get_error_string();

        // Trial-boot rollback (review 2.6). After a firmware update, the previous app slot is
        // remembered with 3 boot attempts. Each boot spends one; reaching 0 before the new
        // image is marked healthy boots the previous slot again.
        void arm_rollback_guard();     // call right before restarting into a new image
        void rollback_boot_check();    // call early in setup()
        void mark_healthy_if_ready(bool wifi_connected);  // call from loop()
    }
}
