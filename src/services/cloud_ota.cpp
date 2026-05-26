#include "cloud_ota.h"
#include "ota_manager.h"
#include "../core/metadata.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

namespace services {
    namespace cloud_ota {

        static ReleaseInfo cached_info = {false, "", "", ""};
        static bool check_complete = false;

        static void fetch_github_metadata() {

            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 30) {
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                attempts++;
            }

            if (WiFi.status() != WL_CONNECTED) return;

            WiFiClientSecure client;
            client.setInsecure(); // Bypass cert verification to save RAM constraints
            HTTPClient http;

            char api_url[128];
            snprintf(api_url, sizeof(api_url), "https://api.github.com/repos/%s/releases/latest", meta::GITHUB_REPO);

            http.begin(client, api_url);
            http.addHeader("User-Agent", "QRPickle-ESP32");
            
            int httpCode = http.GET();
            if (httpCode == HTTP_CODE_OK) {
                // Strict memory filter: Only parse what we need to prevent OOM panics
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

                    // Check string arrays for firmware.bin
                    JsonArray assets = doc["assets"].as<JsonArray>();
                    for (JsonObject asset : assets) {
                        const char* asset_name = asset["name"] | "";
                        if (strcmp(asset_name, "firmware.bin") == 0) {
                            strncpy(cached_info.firmware_url, asset["browser_download_url"] | "", sizeof(cached_info.firmware_url) - 1);
                            break;
                        }
                    }

                    // Compare tag against metadata
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
                // Spawn isolated task with 6KB stack for heavy TLS networking
                xTaskCreatePinnedToCore(background_check_task, "gh_ota_check", 6144, NULL, 1, NULL, 0);
            }
        }

        bool is_update_available() {
            return cached_info.update_available;
        }

        ReleaseInfo get_release_info() {
            return cached_info;
        }

        bool execute_firmware_flash() {
            if (strlen(cached_info.firmware_url) == 0) return false;

            WiFiClientSecure client;
            client.setInsecure();
            HTTPClient http;

            http.begin(client, cached_info.firmware_url);
            http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
            
            int httpCode = http.GET();
            if (httpCode != HTTP_CODE_OK) {
                http.end();
                return false;
            }

            int total_len = http.getSize();
            if (total_len <= 0) return false;

            if (!ota_manager::begin(ota_manager::UPDATE_TYPE_FIRMWARE)) return false;

            WiFiClient* stream = http.getStreamPtr();
            uint8_t buffer[1024];
            int written = 0;

            while (http.connected() && (total_len > 0 || total_len == -1)) {
                size_t size = stream->available();
                if (size) {
                    int c = stream->readBytes(buffer, ((size > sizeof(buffer)) ? sizeof(buffer) : size));
                    if (!ota_manager::write_chunk(buffer, c)) {
                        ota_manager::abort();
                        http.end();
                        return false;
                    }
                    if (total_len > 0) total_len -= c;
                }
                delay(1); // Feed WDT
            }

            bool success = ota_manager::end();
            http.end();
            return success;
        }
    }
}
