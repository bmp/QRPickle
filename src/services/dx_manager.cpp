#include "dx_manager.h"
#include "../config/config.h"
#include <WiFi.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

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

    void DxManager::start() {
        if (status != DX_STATUS_DISCONNECTED) return;
        
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
        
        const auto& cfg = config::get();
        Serial.printf("[DX Engine] Connecting to Primary Node: %s:%u\n", cfg.dx_url_primary, cfg.dx_port_primary);
        
        if (client.connect(cfg.dx_url_primary, cfg.dx_port_primary)) {
            status = DX_STATUS_CONNECTING;
            state_timer = millis();
        } else {
            Serial.println("[DX Engine] Primary connection failed, attempting Secondary...");
            using_secondary = true;
            if (client.connect(cfg.dx_url_secondary, cfg.dx_port_secondary)) {
                status = DX_STATUS_CONNECTING;
                state_timer = millis();
            } else {
                Serial.println("[DX Engine] Secondary connection failed completely.");
                status = DX_STATUS_DISCONNECTED;
            }
        }
    }

    void DxManager::stop() {
        if (client.connected()) {
            client.println("bye"); 
            client.stop();
        }
        status = DX_STATUS_DISCONNECTED;
        buffer_dirty = false;
        // FIXED: We intentionally DO NOT free the spots array here anymore. 
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
        if (status == DX_STATUS_DISCONNECTED || !spots) return;

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