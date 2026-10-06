#include "net_lock.h"
#include "../core/crashlog.h"
#include "cloud_ota.h"
#include "ota_manager.h"
#include "version.h"
#include "../core/metadata.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <atomic>
#include <mbedtls/sha256.h>

namespace services {
    namespace cloud_ota {

        static ReleaseInfo cached_info = {false, "", "", "", ""};
        static std::atomic<bool> check_running{false};
        static std::atomic<bool> is_flashing_active{false};
        static bool check_complete = false;

        static void fetch_github_metadata() {
            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 30) {
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                attempts++;
            }
            if (WiFi.status() != WL_CONNECTED) return;

            NetLock lock;
            if (!lock.held()) return;
            WiFiClientSecure client;
            client.setInsecure();  // TLS is not authenticated; see review 2.4 (SHA-256 below)
            HTTPClient http;

            char api_url[128];
            snprintf(api_url, sizeof(api_url), "https://api.github.com/repos/%s/releases/latest", meta::GITHUB_REPO);

            crashlog::mark(crashlog::SLOT_GH_OTA, 2); http.begin(client, api_url);
            http.addHeader("User-Agent", "QRPickle-ESP32");

            crashlog::mark(crashlog::SLOT_GH_OTA, 3); int httpCode = http.GET();
            if (httpCode == HTTP_CODE_OK) {
                crashlog::mark(crashlog::SLOT_GH_OTA, 4);
                JsonDocument filter;
                filter["tag_name"] = true;
                filter["body"] = true;
                filter["assets"][0]["browser_download_url"] = true;
                filter["assets"][0]["name"] = true;

                JsonDocument doc;
                DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
                if (!err) {
                    ReleaseInfo info = {false, "", "", "", ""};
                    strncpy(info.latest_version, doc["tag_name"] | "", sizeof(info.latest_version) - 1);
                    strncpy(info.release_notes, doc["body"] | "No release notes provided.", sizeof(info.release_notes) - 1);
                    for (JsonObject asset : doc["assets"].as<JsonArray>()) {
                        const char* name = asset["name"] | "";
                        const char* url = asset["browser_download_url"] | "";
                        if (strcmp(name, "firmware.bin") == 0) strncpy(info.firmware_url, url, sizeof(info.firmware_url) - 1);
                        if (strcmp(name, "firmware.bin.sha256") == 0) strncpy(info.sha256_url, url, sizeof(info.sha256_url) - 1);
                    }
                    // Only newer releases are offered (review 2.7: strcmp offered downgrades).
                    info.update_available = info.latest_version[0] &&
                                            compare_versions(info.latest_version, meta::FW_VERSION) > 0;
                    cached_info = info;
                }
            }
            crashlog::mark(crashlog::SLOT_GH_OTA, 5); http.end();
            crashlog::mark(crashlog::SLOT_GH_OTA, 6); check_complete = true;
        }

        static void background_check_task(void*) {
            fetch_github_metadata();
            check_running = false;
            vTaskDelete(NULL);
        }

        static void spawn_check() {
            bool expected = false;
            if (is_flashing_active || !check_running.compare_exchange_strong(expected, true)) return;
            if (xTaskCreatePinnedToCore(background_check_task, "gh_ota_check", 6144, NULL, 1, NULL, 0) != pdPASS) {
                check_running = false;
            }
        }

        void start_background_check() {
            if (!check_complete) spawn_check();
        }

        void force_update_check() {
            if (WiFi.status() == WL_CONNECTED) spawn_check();
        }

        bool is_update_available() { return cached_info.update_available; }
        bool is_check_running() { return check_running; }
        ReleaseInfo get_release_info() { return cached_info; }

        // On any failure the main loop is parked in the OTA lockdown with services stopped,
        // so a restart is the only way back to a working device (review 2.3).
        static void fail_and_restart(const char* why) {
            Serial.printf("[OTA Worker] FAILED: %s. Restarting in 3 s...\n", why);
            if (Update.isRunning()) Update.abort();
            vTaskDelay(pdMS_TO_TICKS(3000));
            ESP.restart();
        }

        // Fetches the 64-hex-digit SHA-256 published next to the firmware. Returns false if
        // unavailable or malformed.
        static bool fetch_expected_sha(char out_hex[65]) {
            if (!cached_info.sha256_url[0]) return false;
            NetLock lock;
            if (!lock.held()) return false;
            WiFiClientSecure client;
            client.setInsecure();
            HTTPClient http;
            http.begin(client, cached_info.sha256_url);
            http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
            http.setTimeout(15000);
            bool ok = false;
            if (http.GET() == HTTP_CODE_OK) {
                String body = http.getString();
                body.trim();
                if (body.length() >= 64) {
                    ok = true;
                    for (int i = 0; i < 64; i++) {
                        char c = tolower(body[i]);
                        if (!isxdigit((unsigned char)c)) { ok = false; break; }
                        out_hex[i] = c;
                    }
                    out_hex[64] = '\0';
                }
            }
            http.end();
            return ok;
        }

        static void ota_worker_task(void*) {
            vTaskDelay(200 / portTICK_PERIOD_MS);

            char expected_hex[65] = {0};
            bool verify = fetch_expected_sha(expected_hex);
            if (!verify) {
                if (cached_info.sha256_url[0]) fail_and_restart("could not read firmware.bin.sha256");
                Serial.println("[OTA Worker] WARNING: release has no firmware.bin.sha256; integrity not verified.");
            }

            NetLock lock(60000);  // whole download; other services are stopped by the lockdown
            if (!lock.held()) fail_and_restart("network busy");
            WiFiClientSecure client;
            client.setInsecure();
            HTTPClient http;
            Serial.printf("[OTA Worker] Downloading %s\n", cached_info.firmware_url);
            http.begin(client, cached_info.firmware_url);
            http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
            http.setRedirectLimit(3);
            http.setTimeout(15000);

            int httpCode = http.GET();
            if (httpCode != HTTP_CODE_OK) fail_and_restart("HTTP error");
            int total_len = http.getSize();
            if (total_len <= 0) fail_and_restart("unknown download size");
            if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) fail_and_restart(Update.errorString());

            mbedtls_sha256_context sha;
            mbedtls_sha256_init(&sha);
            mbedtls_sha256_starts_ret(&sha, 0);

            WiFiClient* stream = http.getStreamPtr();
            uint8_t buffer[1024];
            int remaining = total_len;
            uint32_t last_data = millis();
            while (remaining > 0) {
                if (!http.connected() && !stream->available()) break;
                size_t avail = stream->available();
                if (avail) {
                    int c = stream->readBytes(buffer, avail > sizeof(buffer) ? sizeof(buffer) : avail);
                    if (c <= 0) continue;
                    mbedtls_sha256_update_ret(&sha, buffer, c);
                    if (Update.write(buffer, c) != (size_t)c) fail_and_restart(Update.errorString());
                    remaining -= c;
                    last_data = millis();
                } else if (millis() - last_data > 20000) {
                    break;  // stalled download
                }
                vTaskDelay(1);
            }
            http.end();

            uint8_t digest[32];
            mbedtls_sha256_finish_ret(&sha, digest);
            mbedtls_sha256_free(&sha);
            if (remaining != 0) fail_and_restart("download incomplete");

            if (verify) {
                char got[65];
                for (int i = 0; i < 32; i++) sprintf(got + i * 2, "%02x", digest[i]);
                if (strcmp(got, expected_hex) != 0) fail_and_restart("SHA-256 mismatch");
                Serial.println("[OTA Worker] SHA-256 verified.");
            }

            if (!Update.end(true)) fail_and_restart(Update.errorString());
            ota_manager::arm_rollback_guard();
            Serial.println("[OTA Worker] Success. Restarting into the new image.");
            vTaskDelay(500 / portTICK_PERIOD_MS);
            ESP.restart();
        }

        bool execute_firmware_flash() {
            if (strlen(cached_info.firmware_url) == 0 || is_flashing_active) return false;
            is_flashing_active = true;
            if (xTaskCreatePinnedToCore(ota_worker_task, "ota_flash_worker", 8192, NULL, 5, NULL, 0) != pdPASS) {
                is_flashing_active = false;
                return false;
            }
            return true;
        }
    }
}
