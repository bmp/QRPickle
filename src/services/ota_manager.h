#pragma once
#include <Arduino.h>

namespace services {
    namespace ota_manager {

        enum UpdateType {
            UPDATE_TYPE_FIRMWARE,
            UPDATE_TYPE_FILESYSTEM,
            UPDATE_TYPE_UNKNOWN
        };

        // Prepares partition locks and initializes verification vectors
        bool begin(UpdateType type);

        // Writes a sequential data chunk straight into the target flash block
        bool write_chunk(uint8_t* data, size_t len);

        // Validates flash checksum signatures and flips the passive boot flag
        bool end();

        // Aborts any stale or corrupted transfer sessions to clear memory
        void abort();

        // Thread-safe query to read native error string descriptions
        const char* get_error_string();
    }
}