#include "net_lock.h"
#include "../core/crashlog.h"
#include "cloud_ota.h"
#include "ota_manager.h"
#include "version.h"
#include "hamalert_manager.h"
#include "aprs_manager.h"
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

        // Where CI publishes ota.json + firmware.bin: https://<owner>.github.io/<repo>/ota/
        // (derived from meta::GITHUB_REPO). GitHub's release download host is not reachable with
        // this mbedTLS (review 2.9). Test builds may override with -DQRP_OTA_BASE_URL=\"http://...\".
        static String base_url() {
#ifdef QRP_OTA_BASE_URL
            return String(QRP_OTA_BASE_URL);
#else
            String repo(meta::GITHUB_REPO);
            int slash = repo.indexOf('/');
            return "https://" + repo.substring(0, slash) + ".github.io/" + repo.substring(slash + 1) + "/ota/";
#endif
        }

        // HTTPS without certificate checking for now (review 2.11: verification is intermittent on
        // this mbedTLS); integrity comes from the mandatory SHA-256. Plain http only in test builds.
        struct Transport {
            WiFiClient plain;
            WiFiClientSecure tls;
            WiFiClient& for_url(const String& url) {
                if (url.startsWith("http://")) return plain;
                tls.setInsecure();
                return tls;
            }
        };

        static bool valid_sha(const char* s) {
            if (strlen(s) != 64) return false;
            for (const char* p = s; *p; p++) if (!isxdigit((unsigned char)*p)) return false;
            return true;
        }

        static void fetch_ota_json() {
            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 30) {
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                attempts++;
            }
            if (WiFi.status() != WL_CONNECTED) return;

            NetLock lock;
            if (!lock.held()) return;
            const String url = base_url() + "ota.json";
            Transport t;
            HTTPClient http;
            crashlog::mark(crashlog::SLOT_GH_OTA, 2); http.begin(t.for_url(url), url);
            http.setTimeout(15000);
            crashlog::mark(crashlog::SLOT_GH_OTA, 3); int code = http.GET();
            if (code != HTTP_CODE_OK) {
                Serial.printf("[OTA] %s -> HTTP %d (%s), largest block %u B\n", url.c_str(), code,
                              http.errorToString(code).c_str(), (unsigned)ESP.getMaxAllocHeap());
            } else {
                crashlog::mark(crashlog::SLOT_GH_OTA, 4);
                JsonDocument doc;
                DeserializationError err = deserializeJson(doc, http.getStream());
                if (err) {
                    Serial.printf("[OTA] ota.json parse error: %s\n", err.c_str());
                } else {
                    ReleaseInfo info = {false, "", "", "", ""};
                    strncpy(info.latest_version, doc["version"] | "", sizeof(info.latest_version) - 1);
                    strncpy(info.release_notes, doc["notes"] | "No release notes provided.", sizeof(info.release_notes) - 1);
                    strncpy(info.sha256, doc["sha256"] | "", sizeof(info.sha256) - 1);
                    for (char* p = info.sha256; *p; p++) *p = tolower((unsigned char)*p);
                    String fw = base_url() + (doc["firmware"] | "firmware.bin");
                    strncpy(info.firmware_url, fw.c_str(), sizeof(info.firmware_url) - 1);
                    // Only newer releases with a valid hash are offered (reviews 2.4, 2.7).
                    info.update_available = info.latest_version[0] && valid_sha(info.sha256) &&
                                            compare_versions(info.latest_version, meta::FW_VERSION) > 0;
                    cached_info = info;
                }
            }
            crashlog::mark(crashlog::SLOT_GH_OTA, 5); http.end();
            crashlog::mark(crashlog::SLOT_GH_OTA, 6); check_complete = true;
        }

        // Quiet window (review 2.10): TLS needs ~33 KB of contiguous heap plus this task's stack,
        // but with HamAlert/APRS running the largest free block can be too small. Pause them for
        // the few seconds of the check (as xOTA does), then resume the ones that were running.
        static void background_check_task(void*) {
            const bool ham_was = !HamAlertManager::is_stopped();
            const bool aprs_was = !AprsManager::is_stopped();
            HamAlertManager::stop();
            AprsManager::stop();
            for (int i = 0; i < 80 && !(HamAlertManager::is_stopped() && AprsManager::is_stopped()); i++) {
                vTaskDelay(pdMS_TO_TICKS(100));  // stop() only requests the exit
            }
            fetch_ota_json();
            Serial.printf("[OTA] Update check %s (latest: %s, local: %s)\n",
                          cached_info.latest_version[0] ? "OK" : "FAILED",
                          cached_info.latest_version[0] ? cached_info.latest_version : "-", meta::FW_VERSION);
            if (ham_was) HamAlertManager::start();
            if (aprs_was) AprsManager::start();
            check_running = false;
            vTaskDelete(NULL);
        }

        static void spawn_check() {
            bool expected = false;
            if (is_flashing_active || !check_running.compare_exchange_strong(expected, true)) return;
            if (xTaskCreatePinnedToCore(background_check_task, "gh_ota_check", 8192, NULL, 1, NULL, 0) != pdPASS) {
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

        static void ota_worker_task(void*) {
            vTaskDelay(200 / portTICK_PERIOD_MS);

            if (!valid_sha(cached_info.sha256)) fail_and_restart("ota.json has no valid sha256");

            NetLock lock(60000);  // whole download; other services are stopped by the lockdown
            if (!lock.held()) fail_and_restart("network busy");
            Transport t;
            HTTPClient http;
            const String fw_url(cached_info.firmware_url);
            Serial.printf("[OTA Worker] Downloading %s\n", fw_url.c_str());
            http.begin(t.for_url(fw_url), fw_url);
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

            char got[65];
            for (int i = 0; i < 32; i++) sprintf(got + i * 2, "%02x", digest[i]);
            if (strcmp(got, cached_info.sha256) != 0) fail_and_restart("SHA-256 mismatch");
            Serial.println("[OTA Worker] SHA-256 verified.");

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
