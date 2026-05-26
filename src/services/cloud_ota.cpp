#include "cloud_ota.h"
#include "ota_manager.h"
#include "../core/metadata.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include <Update.h>

namespace services {
    namespace cloud_ota {

        static ReleaseInfo cached_info = {false, "", "", ""};
        static bool check_complete = false;
        static bool is_flashing_active = false; 

        static void fetch_github_metadata() {
            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 30) {
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                attempts++;
            }

            if (WiFi.status() != WL_CONNECTED) return;

            WiFiClientSecure client;
            client.setInsecure(); 
            HTTPClient http;

            char api_url[128];
            snprintf(api_url, sizeof(api_url), "https://api.github.com/repos/%s/releases/latest", meta::GITHUB_REPO);

            http.begin(client, api_url);
            http.addHeader("User-Agent", "QRPickle-ESP32");
            
            int httpCode = http.GET();
            if (httpCode == HTTP_CODE_OK) {
                JsonDocument filter;
                filter["tag_name"] = true;
                filter["body"] = true;
                filter["assets"][0]["browser_download_url"] = true;
                filter["assets"][0]["name"] = true;

                JsonDocument doc;
                DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));

                if (!err) {
                    const char* tag = doc["tag_name"] | "";
                    const char* body = doc["body"] | "No release notes provided.";
                    
                    strncpy(cached_info.latest_version, tag, sizeof(cached_info.latest_version) - 1);
                    strncpy(cached_info.release_notes, body, sizeof(cached_info.release_notes) - 1);

                    JsonArray assets = doc["assets"].as<JsonArray>();
                    for (JsonObject asset : assets) {
                        const char* asset_name = asset["name"] | "";
                        if (strcmp(asset_name, "firmware.bin") == 0) {
                            strncpy(cached_info.firmware_url, asset["browser_download_url"] | "", sizeof(cached_info.firmware_url) - 1);
                            break;
                        }
                    }

                    if (strcmp(cached_info.latest_version, meta::FW_VERSION) != 0 && strlen(cached_info.latest_version) > 0) {
                        cached_info.update_available = true;
                    }
                }
            }
            http.end();
            check_complete = true;
        }

        static void background_check_task(void* pvParameters) {
            fetch_github_metadata();
            vTaskDelete(NULL);
        }

        void start_background_check() {
            if (!check_complete) {
                xTaskCreatePinnedToCore(background_check_task, "gh_ota_check", 6144, NULL, 1, NULL, 0);
            }
        }

        void force_update_check() {
            if (WiFi.status() == WL_CONNECTED) {
                fetch_github_metadata();
            }
        }

        bool is_update_available() {
            return cached_info.update_available;
        }

        ReleaseInfo get_release_info() {
            return cached_info;
        }

        static void ota_worker_task(void* pvParameters) {
            vTaskDelay(200 / portTICK_PERIOD_MS); 

            WiFiClientSecure client;
            client.setInsecure(); 

            HTTPClient http;
            Serial.printf("[OTA Worker] Activating secure stream to CDN: %s\n", cached_info.firmware_url);
            
            http.begin(client, cached_info.firmware_url);
            http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
            http.setRedirectLimit(3); 
            http.setTimeout(15000); 

            int httpCode = http.GET();
            if (httpCode != HTTP_CODE_OK) {
                Serial.printf("[OTA Worker] Link connection rejected. HTTP Code: %d\n", httpCode);
                http.end();
                is_flashing_active = false;
                vTaskDelete(NULL);
                return;
            }

            int total_len = http.getSize();
            if (total_len <= 0) {
                Serial.println("[OTA Worker] Invalid payload signature dimension.");
                http.end();
                is_flashing_active = false;
                vTaskDelete(NULL);
                return;
            }

            Serial.println("[OTA Worker] Initializing flash streams...");
            if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
                Serial.printf("[OTA Worker] CRITICAL: Flash allocation failed. Error: %s\n", Update.errorString());
                http.end();
                is_flashing_active = false;
                vTaskDelete(NULL);
                return;
            }

            WiFiClient* stream = http.getStreamPtr();
            uint8_t buffer[1024];
            int written_accumulated = 0;

            Serial.println("[OTA Worker] Streaming binary block patterns into flash sectors...");
            while (http.connected() && (total_len > 0 || total_len == -1)) {
                size_t size = stream->available();
                if (size) {
                    int c = stream->readBytes(buffer, ((size > sizeof(buffer)) ? sizeof(buffer) : size));
                    if (Update.write(buffer, c) != c) {
                        Serial.printf("[OTA Worker] Write aborted. Error: %s\n", Update.errorString());
                        Update.abort();
                        http.end();
                        is_flashing_active = false;
                        vTaskDelete(NULL);
                        return;
                    }
                    if (total_len > 0) total_len -= c;
                    written_accumulated += c;
                }
                vTaskDelay(1); 
            }

            Serial.println("[OTA Worker] Stream finished. Verifying checksum patterns...");
            if (Update.end(true)) { 
                Serial.println("[OTA Worker] Success! Core restart sequence authorized.");
                vTaskDelay(500 / portTICK_PERIOD_MS);
                ESP.restart();
            } else {
                Serial.printf("[OTA Worker] Verification failure. Error: %s\n", Update.errorString());
                is_flashing_active = false;
                http.end();
                vTaskDelete(NULL);
            }
        }

        bool execute_firmware_flash() {
            if (strlen(cached_info.firmware_url) == 0 || is_flashing_active) return false;

            is_flashing_active = true;
            xTaskCreatePinnedToCore(ota_worker_task, "ota_flash_worker", 6144, NULL, 5, NULL, 0);
            return true; 
        }
    }
}