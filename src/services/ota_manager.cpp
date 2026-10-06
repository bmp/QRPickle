#include "ota_manager.h"
#include <Update.h>
#include <Preferences.h>
#include <esp_ota_ops.h>

namespace services {
    namespace ota_manager {

        static const char* err_msg = "NO_ERROR";
        static const char* NS = "qrp_ota";
        static const uint8_t TRIAL_BOOTS = 3;
        static const uint32_t HEALTHY_AFTER_MS = 60000;
        static bool guard_active = false;

        bool begin(UpdateType type) {
            err_msg = "NO_ERROR";
            int cmd;
            if (type == UPDATE_TYPE_FIRMWARE) {
                cmd = U_FLASH;
                Serial.println("[OTA Engine] Initializing application firmware update...");
            } else if (type == UPDATE_TYPE_FILESYSTEM) {
                cmd = U_SPIFFS;
                Serial.println("[OTA Engine] Initializing LittleFS filesystem update...");
            } else {
                err_msg = "INVALID_UPDATE_TYPE";
                return false;
            }
            // UPDATE_SIZE_UNKNOWN = use the real partition size. The old hardcoded sizes
            // (0x180000 / 0xE0000) predated the current partitions.csv (review 2.1/2.2).
            if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
                err_msg = Update.errorString();
                Serial.printf("[OTA CRITICAL] Partition allocation failed: %s\n", err_msg);
                return false;
            }
            return true;
        }

        bool write_chunk(uint8_t* data, size_t len) {
            if (Update.write(data, len) != len) {
                err_msg = Update.errorString();
                Serial.printf("[OTA CRITICAL] Chunk write failure: %s\n", err_msg);
                return false;
            }
            return true;
        }

        bool end() {
            if (!Update.end(true)) {
                err_msg = Update.errorString();
                Serial.printf("[OTA CRITICAL] Image incomplete or invalid: %s\n", err_msg);
                return false;
            }
            Serial.println("[OTA Engine] Image complete (firmware: boot partition switched; filesystem: written).");
            return true;
        }

        void abort() {
            Update.abort();
            err_msg = "SESSION_ABORTED";
            Serial.println("[OTA Engine] Update session aborted.");
        }

        const char* get_error_string() { return err_msg; }

        void arm_rollback_guard() {
            const esp_partition_t* running = esp_ota_get_running_partition();
            if (!running) return;
            Preferences p;
            p.begin(NS, false);
            p.putString("prev", running->label);
            p.putUChar("trials", TRIAL_BOOTS);
            p.end();
            Serial.printf("[OTA Guard] Armed: %u trial boots, fallback slot %s\n", TRIAL_BOOTS, running->label);
        }

        void rollback_boot_check() {
            Preferences p;
            p.begin(NS, false);
            uint8_t trials = p.getUChar("trials", 0);
            if (trials == 0) { p.end(); return; }

            char prev[17] = {0};
            p.getString("prev", prev, sizeof(prev));
            const esp_partition_t* running = esp_ota_get_running_partition();
            if (running && strcmp(running->label, prev) == 0) {
                // Already back on the fallback slot (rolled back or never switched): disarm.
                p.putUChar("trials", 0);
                p.end();
                return;
            }
            trials--;
            p.putUChar("trials", trials);
            p.end();
            Serial.printf("[OTA Guard] Trial boot of new image, %u attempt(s) left after this one\n", trials);

            if (trials == 0) {
                const esp_partition_t* fallback =
                    esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, prev);
                if (fallback && esp_ota_set_boot_partition(fallback) == ESP_OK) {
                    Serial.printf("[OTA Guard] New image never became healthy; rolling back to %s\n", prev);
                    delay(200);
                    ESP.restart();
                }
                Serial.println("[OTA Guard] Rollback impossible (fallback slot missing); keeping image.");
                return;
            }
            guard_active = true;
        }

        void mark_healthy_if_ready(bool wifi_connected) {
            if (!guard_active || millis() < HEALTHY_AFTER_MS || !wifi_connected) return;
            guard_active = false;
            Preferences p;
            p.begin(NS, false);
            p.putUChar("trials", 0);
            p.end();
            Serial.println("[OTA Guard] New image healthy (60 s with WiFi); rollback disarmed.");
        }
    }
}
