#include "net_connect.h"
#include "../core/mem_stats.h"
#include "../core/crashlog.h"
#include "hamalert_manager.h"
#include "hamalert_parse.h"
#include "../config/config.h"
#include "../hw/led_rgb.h" // Needed for LED telemetry
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <cstring>
#include <cstdio>
#include <atomic>

namespace services {

    HamAlertMessage* HamAlertManager::messages = nullptr;
    size_t HamAlertManager::message_count = 0;
    bool HamAlertManager::connected = false;
    bool HamAlertManager::dirty = false;
    bool HamAlertManager::running = false;

    // The task is created once and then parks (socket closed, stack kept) instead of exiting when
    // stopped: deleting and re-creating it moved its 4 KB stack around the heap on every quiet
    // window and fragmented it (docs/MEMORY.md, v0.2.5 coordinator).
    static std::atomic<bool> task_alive{false};
    static std::atomic<bool> parked{false};

    void HamAlertManager::start() {
        if (running) return;
        if (strlen(config::get().hamalert_password) == 0) {
            Serial.println("[HamAlert-Engine] Token password missing. Aborting start.");
            return;
        }

        if (!messages) {
            messages = (HamAlertMessage*)calloc(10, sizeof(HamAlertMessage)); 
            if (!messages) return;
        }

        if (task_alive) {  // parked: wake it
            running = true;
            return;
        }
        Serial.println("[HamAlert-Engine] Initializing background monitoring task...");
        running = true;
        task_alive = true;
        TaskHandle_t handle = nullptr;
        if (xTaskCreate(task_loop, "hamalert_task", 4096, NULL, 1, &handle) /* 3072 was tight (review 3.9) */ != pdPASS) {
            running = false;
            task_alive = false;
            Serial.println("[HamAlert-Engine] Task creation failed (heap).");
        }
        if (handle) mem_stats::watch_stack(mem_stats::STK_HAMALERT, handle);
    }

    void HamAlertManager::stop() { 
        running = false; 
        connected = false; 
    }
    bool HamAlertManager::is_stopped() { return !task_alive || parked; }
    bool HamAlertManager::is_connected() { return connected; }
    bool HamAlertManager::is_dirty() { return dirty; }
    void HamAlertManager::clear_dirty() { dirty = false; }
    const HamAlertMessage* HamAlertManager::get_messages() { return messages; }
    size_t HamAlertManager::get_message_count() { return message_count; }

    void HamAlertManager::process_line(char* line) {
        Serial.printf("[HamAlert-Raw] '%s'\n", line);

        if (strstr(line, "DX de") == nullptr) return;

        hamalert::SpotFields f;
        if (hamalert::split_spot(line, f) < 3) return;
        char* remainder = f.rest;

        HamAlertMessage m{};
        strncpy(m.freq, f.freq, sizeof(m.freq) - 1);
        strncpy(m.callsign, f.call, sizeof(m.callsign) - 1);

        int r_len = strlen(remainder);
        while (r_len > 0 && remainder[r_len - 1] == ' ') {
            remainder[r_len - 1] = '\0';
            r_len--;
        }

        char* last_space = strrchr(remainder, ' ');
        if (last_space) {
            strncpy(m.time, last_space + 1, sizeof(m.time)-1);
            *last_space = '\0'; 
        } else {
            strncpy(m.time, remainder, sizeof(m.time)-1);
            remainder[0] = '\0';
        }

        char* z_char = strchr(m.time, 'Z');
        if (z_char) *z_char = '\0';

        char* comment = remainder;
        while (*comment == ' ') comment++; 
        
        if (strlen(comment) > 0) {
            strncpy(m.info, comment, sizeof(m.info)-1);
        } else {
            strncpy(m.info, "Alert Triggered", sizeof(m.info)-1);
        }

        if (strcasestr(comment, "SOTA")) strncpy(m.type, "SOTA", sizeof(m.type)-1);
        else if (strcasestr(comment, "POTA")) strncpy(m.type, "POTA", sizeof(m.type)-1);
        else if (strcasestr(comment, "WWFF")) strncpy(m.type, "WWFF", sizeof(m.type)-1);
        else strncpy(m.type, "DXCC", sizeof(m.type)-1);

        if (strcasestr(comment, "FT8")) strncpy(m.mode, "FT8", sizeof(m.mode)-1);
        else if (strcasestr(comment, "FT4")) strncpy(m.mode, "FT4", sizeof(m.mode)-1);
        else if (strcasestr(comment, "CW")) strncpy(m.mode, "CW", sizeof(m.mode)-1);
        else if (strcasestr(comment, "SSB")) strncpy(m.mode, "SSB", sizeof(m.mode)-1);
        else strncpy(m.mode, "DATA", sizeof(m.mode)-1);

        if (message_count < 10) {
            messages[message_count++] = m;
        } else {
            for (int i = 0; i < 9; i++) messages[i] = messages[i + 1];
            messages[9] = m;
        }
        dirty = true;
        
        // Fire the visual LED alert for the user
        hw::led_rgb::trigger_priority_strobe();
    }

    void HamAlertManager::task_loop(void* param) {
        vTaskDelay(pdMS_TO_TICKS(5000)); 

        WiFiClient client;
        client.setTimeout(5); 
        
        char buffer[256];
        size_t buf_idx = 0;

        for (;;) {
            while (running) {
                auto& cfg = config::get();

                if (!WiFi.isConnected()) {
                    if (connected || client.connected()) {
                        client.stop();
                        connected = false;
                    }
                    for (int i = 0; i < 30 && running; i++) vTaskDelay(pdMS_TO_TICKS(100));
                    continue;
                }

                if (!connected) {
                    client.stop();
                    Serial.println("[HamAlert-Socket] Directing link to hamalert.org:7300...");

                    crashlog::mark(crashlog::SLOT_HAMALERT, 2);
                    if (connect_host(client, "hamalert.org", 7300, 5000, crashlog::SLOT_HAMALERT)) {
                        unsigned long timeout_mark = millis();
                        bool authenticated = false;

                        buf_idx = 0;
                        buffer[0] = '\0';

                        while (client.connected() && (millis() - timeout_mark < 15000) && !authenticated && running) {
                            if (client.available()) {
                                char c = client.read();
                                buffer[buf_idx++] = c;
                                buffer[buf_idx] = '\0';

                                if (strstr(buffer, "login:") || strstr(buffer, "callsign:") || strstr(buffer, "Callsign:")) {
                                    client.printf("%s\r\n", cfg.callsign);
                                    buf_idx = 0;
                                    buffer[0] = '\0';
                                } else if (strstr(buffer, "password:")) {
                                    client.printf("%s\r\n", cfg.hamalert_password);
                                    buf_idx = 0;
                                    buffer[0] = '\0';
                                }
                                // Only HamAlert's greeting proves the login worked (review 3.8).
                                else if (strstr(buffer, "Hello ")) {
                                    connected = true;
                                    authenticated = true;
                                    buf_idx = 0;
                                    buffer[0] = '\0';
                                } else if (strcasestr(buffer, "fail") || strcasestr(buffer, "incorrect") || strcasestr(buffer, "invalid")) {
                                    Serial.println("[HamAlert-Socket] Login rejected; check callsign/password.");
                                    break;
                                } else if (c == '\n' || buf_idx >= sizeof(buffer) - 1) {
                                    buf_idx = 0;
                                    buffer[0] = '\0';
                                }
                            }
                            vTaskDelay(pdMS_TO_TICKS(10));
                        }

                        if (!authenticated && running) {
                            client.stop();
                            for (int i = 0; i < 300 && running; i++) vTaskDelay(pdMS_TO_TICKS(100));
                            continue;
                        }
                    } else {
                        client.stop();
                        for (int i = 0; i < 300 && running; i++) vTaskDelay(pdMS_TO_TICKS(100));
                        continue;
                    }
                }

                while (client.available() && running) {
                    char c = client.read();
                    if (c == '\n') {
                        buffer[buf_idx] = '\0';
                        if (buf_idx > 0 && buffer[buf_idx - 1] == '\r') buffer[buf_idx - 1] = '\0';

                        if (buffer[0] != '#' && strlen(buffer) > 10) {
                            crashlog::mark(crashlog::SLOT_HAMALERT, 5);
                            process_line(buffer);
                        }
                        buf_idx = 0;
                    } else if (buf_idx < sizeof(buffer) - 1) {
                        buffer[buf_idx++] = c;
                    }
                }

                if (!client.connected() && connected) {
                    client.stop();
                    connected = false;
                    for (int i = 0; i < 300 && running; i++) vTaskDelay(pdMS_TO_TICKS(100));
                }

                crashlog::mark(crashlog::SLOT_HAMALERT, 6);
                vTaskDelay(pdMS_TO_TICKS(50));
            }

            client.stop();
            connected = false;
            Serial.println("[HamAlert-Socket] Safely suspended for Time-Slicing.");
            parked = true;  // socket closed: quiet::settled() may go ahead
            while (!running) vTaskDelay(pdMS_TO_TICKS(200));
            parked = false;
            buf_idx = 0;
        }
    }
} // namespace services