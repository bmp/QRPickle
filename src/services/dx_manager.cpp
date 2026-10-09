#include "../core/crashlog.h"
#include "net_connect.h"
#include "dx_manager.h"
#include "../config/config.h"
#include <WiFi.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <atomic>

namespace services {

    WiFiClient client;
    
    DxStatus DxManager::status = DX_STATUS_DISCONNECTED;
    DxSpot* DxManager::spots = nullptr; 
    size_t DxManager::spot_count = 0;
    bool DxManager::buffer_dirty = false; 
    
    char DxManager::line_buffer[160] = {0};
    size_t DxManager::line_idx = 0;
    uint32_t DxManager::state_timer = 0;
    bool DxManager::using_secondary = false;

    // The blocking part of connecting (DNS + connect, up to ~6 s with both clusters down) runs
    // in this short-lived task instead of the UI thread (review 3.3/3.12). It touches `client`
    // only until it hands over by setting status = CONNECTING; after that only update() does.
    static std::atomic<bool> dial_alive{false};
    static std::atomic<bool> stop_requested{false};

    void DxManager::start() {
        if (status != DX_STATUS_DISCONNECTED || dial_alive) return;

        if (!spots) {
            spots = (DxSpot*)calloc(50, sizeof(DxSpot));
            if (!spots) {
                Serial.println("[DX Engine] CRITICAL: Heap memory full!");
                return;
            }
        }

        line_idx = 0;
        using_secondary = false;
        buffer_dirty = true;
        stop_requested = false;
        status = DX_STATUS_DIALING;
        dial_alive = true;
        if (xTaskCreate(dial_task, "dx_dial", 4096, NULL, 1, NULL) != pdPASS) {
            dial_alive = false;
            status = DX_STATUS_DISCONNECTED;
            Serial.println("[DX Engine] Dial task creation failed (heap).");
        }
    }

    void DxManager::dial_task(void*) {
        const auto& cfg = config::get();
        char host_p[sizeof(cfg.dx_url_primary)], host_s[sizeof(cfg.dx_url_secondary)];
        strncpy(host_p, cfg.dx_url_primary, sizeof(host_p));
        strncpy(host_s, cfg.dx_url_secondary, sizeof(host_s));
        uint16_t port_p = cfg.dx_port_primary, port_s = cfg.dx_port_secondary;

        Serial.printf("[DX Engine] Connecting to Primary Node: %s:%u\n", host_p, port_p);
        bool ok = connect_host(client, host_p, port_p, 3000, crashlog::SLOT_NET);
        if (!ok && !stop_requested) {
            Serial.println("[DX Engine] Primary connection failed, attempting Secondary...");
            using_secondary = true;
            ok = connect_host(client, host_s, port_s, 3000, crashlog::SLOT_NET);
        }

        if (stop_requested) {
            if (ok) client.stop();
            status = DX_STATUS_DISCONNECTED;
        } else if (ok) {
            state_timer = millis();
            status = DX_STATUS_CONNECTING;  // hand-over: update() owns the socket from here
        } else {
            Serial.println("[DX Engine] Secondary connection failed completely.");
            status = DX_STATUS_DISCONNECTED;
        }
        dial_alive = false;
        vTaskDelete(NULL);
    }

    void DxManager::stop() {
        if (dial_alive) {
            // The dial task still owns the socket; it closes it and finishes the state change.
            stop_requested = true;
            buffer_dirty = false;
            return;
        }
        if (client.connected()) {
            client.println("bye"); 
            client.stop();
        }
        status = DX_STATUS_DISCONNECTED;
        buffer_dirty = false;
        // We intentionally DO NOT free the spots array here anymore. 
        // This prevents Use-After-Free race conditions during time-slicing pauses!
        Serial.println("[DX Engine] Socket safely suspended for Time-Slicing.");
    }

    void DxManager::clear_spots() {
        if (spots) {
            memset(spots, 0, 50 * sizeof(DxSpot));
        }
        spot_count = 0;
        buffer_dirty = true;
    }

    void DxManager::update() {
        if (status == DX_STATUS_DISCONNECTED || status == DX_STATUS_DIALING || !spots) return;

        if (status == DX_STATUS_CONNECTING || status == DX_STATUS_AUTHORIZING) {
            if (millis() - state_timer > 10000) {
                Serial.println("[DX Engine] Connection or authorization timed out.");
                stop();
                return;
            }
        }

        while (client.available()) {
            char c = client.read();
            
            if (line_idx < sizeof(line_buffer) - 1) {
                line_buffer[line_idx++] = c;
                line_buffer[line_idx] = '\0';
            } else {
                line_idx = 0;
                line_buffer[0] = '\0';
            }

            if (status == DX_STATUS_CONNECTING) {
                if (strcasestr(line_buffer, "login:") || 
                    strcasestr(line_buffer, "call:") || 
                    strcasestr(line_buffer, "callsign:") || 
                    strcasestr(line_buffer, "enter your call")) {
                    
                    Serial.printf("[DX Engine] Sending automated credential authorization: %s\n", config::get().callsign);
                    client.println(config::get().callsign);
                    status = DX_STATUS_AUTHORIZING;
                    line_idx = 0;
                    line_buffer[0] = '\0';
                    buffer_dirty = true;
                    continue;
                }
            }

            if (status == DX_STATUS_AUTHORIZING) {
                if (c == '>' || strcasestr(line_buffer, "welcome") || strcasestr(line_buffer, "cluster")) {
                    status = DX_STATUS_CONNECTED;
                    Serial.println("[DX Engine] Full pipeline established.");
                    line_idx = 0;
                    line_buffer[0] = '\0';
                    buffer_dirty = true;
                    continue;
                }
            }

            if (status == DX_STATUS_CONNECTED) {
                if (c == '\n' || c == '\r') {
                    if (line_idx > 1) {
                        line_buffer[line_idx - 1] = '\0'; 
                        handle_line(line_buffer);
                    }
                    line_idx = 0;
                    line_buffer[0] = '\0';
                }
            }
        }

        if (!client.connected() && status != DX_STATUS_DISCONNECTED) {
            Serial.println("[DX Engine] Connection lost to remote node.");
            stop(); 
        }
    }

    void DxManager::handle_line(const char* line) {
        if (strncmp(line, "DX de ", 6) == 0) {
            parse_dx_line(line);
        }
    }

    void DxManager::parse_dx_line(const char* line) {
        if (strlen(line) < 60 || !spots) return;

        DxSpot spot{0};

        snprintf(spot.spotter, sizeof(spot.spotter), "%.9s", line + 6);
        char* colon = strchr(spot.spotter, ':');
        if (colon) *colon = '\0';
        for (int i = strlen(spot.spotter) - 1; i >= 0 && spot.spotter[i] == ' '; i--) spot.spotter[i] = '\0';

        char freq_buf[12];
        snprintf(freq_buf, sizeof(freq_buf), "%.9s", line + 16);
        spot.freq = strtof(freq_buf, nullptr);

        snprintf(spot.dx_call, sizeof(spot.dx_call), "%.12s", line + 26);
        for (int i = strlen(spot.dx_call) - 1; i >= 0 && spot.dx_call[i] == ' '; i--) spot.dx_call[i] = '\0';

        snprintf(spot.comment, sizeof(spot.comment), "%.29s", line + 39);
        for (int i = strlen(spot.comment) - 1; i >= 0 && spot.comment[i] == ' '; i--) spot.comment[i] = '\0';

        deduce_mode(spot.freq, spot.comment, spot.mode, sizeof(spot.mode));

        for (int i = 49; i > 0; i--) {
            spots[i] = spots[i - 1];
        }
        spots[0] = spot;

        if (spot_count < 50) spot_count++;
        
        buffer_dirty = true; 
    }

    void DxManager::deduce_mode(float freq, const char* comment, char* out_mode, size_t max_len) {
        if (strcasestr(comment, "FT8"))   { strncpy(out_mode, "FT8", max_len); return; }
        if (strcasestr(comment, "FT4"))   { strncpy(out_mode, "FT4", max_len); return; }
        if (strcasestr(comment, "CW"))    { strncpy(out_mode, "CW", max_len); return; }
        if (strcasestr(comment, "RTTY"))  { strncpy(out_mode, "RTTY", max_len); return; }
        if (strcasestr(comment, "SSB") || strcasestr(comment, "LSB") || strcasestr(comment, "USB")) { 
            strncpy(out_mode, "SSB", max_len); return; 
        }

        if (freq > 0.0f) {
            float mhz = (freq > 1000.0f) ? (freq / 1000.0f) : freq;
            if (abs(mhz - 14.074f) < 0.003f || abs(mhz - 7.074f) < 0.003f || abs(mhz - 21.074f) < 0.003f || abs(mhz - 3.573f) < 0.003f) {
                strncpy(out_mode, "FT8", max_len); return;
            }
            if (abs(mhz - 14.047f) < 0.003f || abs(mhz - 7.047f) < 0.003f) {
                strncpy(out_mode, "FT4", max_len); return;
            }
            if (mhz >= 14.000f && mhz < 14.070f) { strncpy(out_mode, "CW", max_len); return; }
            if (mhz >= 14.150f && mhz <= 14.350f) { strncpy(out_mode, "SSB", max_len); return; }
            if (mhz >= 7.000f && mhz < 7.040f) { strncpy(out_mode, "CW", max_len); return; }
            if (mhz >= 7.100f && mhz <= 7.300f) { strncpy(out_mode, "SSB", max_len); return; }
            if (mhz >= 21.000f && mhz < 21.070f) { strncpy(out_mode, "CW", max_len); return; }
            if (mhz >= 21.150f && mhz <= 21.450f) { strncpy(out_mode, "SSB", max_len); return; }
            if (mhz >= 28.000f && mhz < 28.070f) { strncpy(out_mode, "CW", max_len); return; }
            if (mhz >= 28.300f && mhz <= 29.300f) { strncpy(out_mode, "SSB", max_len); return; }
        }
        strncpy(out_mode, "OTHER", max_len);
    }

} // namespace services