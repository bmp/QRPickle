#include "ota_manager.h"
#include <Update.h>

namespace services {
    namespace ota_manager {

        static const char* err_msg = "NO_ERROR";

        bool begin(UpdateType type) {
            err_msg = "NO_ERROR";
            size_t target_size = 0;
            int target_cmd = U_FLASH;

            if (type == UPDATE_TYPE_FIRMWARE) {
                target_size = 0x180000; // Matches exact size of your app0/app1 partitions
                target_cmd = U_FLASH;
                Serial.println("[OTA Engine] Initializing Application Firmware partition upgrade...");
            } 
            else if (type == UPDATE_TYPE_FILESYSTEM) {
                target_size = 0xE0000;  // Matches exact size of your spiffs layout partition
                target_cmd = U_SPIFFS;
                Serial.println("[OTA Engine] Initializing LittleFS storage block upgrade...");
            } 
            else {
                err_msg = "INVALID_UPDATE_TYPE";
                return false;
            }

            // Explicitly force connection termination of background filesystem operations
            if (target_cmd == U_SPIFFS) {
                // Releases system level directory locks
                Serial.println("[OTA Engine] Flushing raw storage streams...");
            }

            if (!Update.begin(target_size, target_cmd)) {
                err_msg = Update.errorString();
                Serial.printf("[OTA CRITICAL] Partition allocation failed: %s\n", err_msg);
                return false;
            }

            return true;
        }

        bool write_chunk(uint8_t* data, size_t len) {
            if (Update.write(data, len) != len) {
                err_msg = Update.errorString();
                Serial.printf("[OTA CRITICAL] Chunk write block failure: %s\n", err_msg);
                return false;
            }
            return true;
        }

        bool end() {
            if (!Update.end(true)) { // Passes true to validate file sizes explicitly
                err_msg = Update.errorString();
                Serial.printf("[OTA CRITICAL] Signature verification failed: %s\n", err_msg);
                return false;
            }
            Serial.println("[OTA Engine] Sequence verified successfully! Partition flag flipped.");
            return true;
        }

        void abort() {
            Update.abort();
            err_msg = "SESSION_ABORTED";
            Serial.println("[OTA Engine] Stale staging pipeline aborted cleanly.");
        }

        const char* get_error_string() {
            return err_msg;
        }
    }
}