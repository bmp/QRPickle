#include "sota_manager.h"
#include "../config/config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include <cstring>
#include <strings.h>

namespace services {

    static const char* CLUSTER_HOST = "cluster.sota.org.uk";
    static const uint16_t CLUSTER_PORT = 7300;
    static const size_t MAX_SPOTS = 30;

    SotaSpot* SotaManager::spots = nullptr;
    size_t SotaManager::spot_count = 0;
    bool SotaManager::fetching = false;
    bool SotaManager::dirty = false;
    bool SotaManager::running = false;
    uint32_t SotaManager::last_fetch_time = 0;

    // Set before the task is created, cleared by the task as its last action.
    static std::atomic<bool> task_alive{false};
    // Guards the spot list while the task reorders it; the UI reads it from loop().
    static portMUX_TYPE spots_mux = portMUX_INITIALIZER_UNLOCKED;

    void SotaManager::start() {
        if (running || task_alive) return;  // previous task may still be exiting

        const char* call = config::get().callsign;
        if (strlen(call) < 3 || strcmp(call, "N0CALL") == 0) {
            Serial.println("[SOTA] Callsign not set; cluster login skipped.");
            return;
        }
        if (!spots) {
            spots = (SotaSpot*)calloc(MAX_SPOTS, sizeof(SotaSpot));
            if (!spots) return;
        }

        running = true;
        task_alive = true;
        fetching = true;
        if (xTaskCreate(task_loop, "sota_cluster", 4096, NULL, 1, NULL) != pdPASS) {
            running = false;
            task_alive = false;
            fetching = false;
            Serial.println("[SOTA] Task creation failed (heap).");
        }
    }

    void SotaManager::stop() { running = false; }

    bool SotaManager::is_stopped() { return !task_alive; }

    // Newest first; a new spot for an activator replaces that activator's older entry.
    void SotaManager::store_spot(const sota_cluster::ParsedSpot& p) {
        SotaSpot s{};
        strncpy(s.time, p.time, sizeof(s.time) - 1);
        strncpy(s.summit, p.summit, sizeof(s.summit) - 1);
        strncpy(s.mode, p.mode, sizeof(s.mode) - 1);
        strncpy(s.activator, p.activator, sizeof(s.activator) - 1);
        s.freq = p.freq_mhz;
        if (p.comment[0]) {
            strncpy(s.comment, p.comment, sizeof(s.comment) - 1);
            strncat(s.comment, " | ", sizeof(s.comment) - strlen(s.comment) - 1);
        }
        strncat(s.comment, "de ", sizeof(s.comment) - strlen(s.comment) - 1);
        strncat(s.comment, p.spotter, sizeof(s.comment) - strlen(s.comment) - 1);
        s.is_qrp = (strcasestr(s.comment, "QRP") != nullptr);

        portENTER_CRITICAL(&spots_mux);
        size_t i = 0;
        while (i < spot_count && strcmp(spots[i].activator, s.activator) != 0) i++;
        if (i == spot_count) {
            if (spot_count < MAX_SPOTS) spot_count++;  // use the free slot
            else i = MAX_SPOTS - 1;                    // drop the oldest
        }
        memmove(&spots[1], &spots[0], i * sizeof(SotaSpot));
        spots[0] = s;
        dirty = true;
        portEXIT_CRITICAL(&spots_mux);
    }

    void SotaManager::task_loop(void* param) {
        WiFiClient client;
        char line[192];
        size_t idx = 0;

        while (running) {
            if (!WiFi.isConnected()) {
                client.stop();
                for (int i = 0; i < 30 && running; i++) vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            if (!client.connected()) {
                Serial.printf("[SOTA] Connecting to %s:%u...\n", CLUSTER_HOST, CLUSTER_PORT);
                client.setTimeout(10);  // seconds; bounds the wait for the login prompt
                if (!client.connect(CLUSTER_HOST, CLUSTER_PORT, 5000) || !client.find("login:")) {
                    client.stop();
                    Serial.println("[SOTA] Cluster unreachable; retrying in 30 s.");
                    for (int i = 0; i < 300 && running; i++) vTaskDelay(pdMS_TO_TICKS(100));
                    continue;
                }
                client.printf("%s\r\n", config::get().callsign);
                idx = 0;
                fetching = false;
                Serial.println("[SOTA] Logged in to SOTA cluster.");
            }

            while (client.available() && running) {
                char c = client.read();
                if (c == '\n') {
                    line[idx] = '\0';
                    sota_cluster::ParsedSpot p;
                    if (sota_cluster::parse_line(line, p)) store_spot(p);
                    idx = 0;
                } else if (c != '\r' && idx < sizeof(line) - 1) {
                    line[idx++] = c;
                }
            }

            last_fetch_time = millis();  // live feed: never stale while connected
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        client.stop();
        fetching = false;
        Serial.println("[SOTA] Cluster session closed.");
        task_alive = false;
        vTaskDelete(NULL);
    }

} // namespace services
