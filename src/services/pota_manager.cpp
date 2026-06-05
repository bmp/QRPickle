#include "pota_manager.h"
#include "../core/metadata.h" // NEW: Pulls dynamic version
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <cstring>
#include <ctype.h>
#include <strings.h>

namespace services {

    PotaSpot* PotaManager::spots = nullptr;
    size_t PotaManager::spot_count = 0;
    bool PotaManager::fetching = false;
    bool PotaManager::dirty = false;
    uint32_t PotaManager::last_fetch_time = 0;

    void PotaManager::deduce_mode(float freq, const char* comment, char* out_mode, size_t max_len) {
        if (strcasestr(comment, "FT8"))   { strncpy(out_mode, "FT8", max_len); return; }
        if (strcasestr(comment, "CW"))    { strncpy(out_mode, "CW", max_len); return; }
        if (strcasestr(comment, "SSB"))   { strncpy(out_mode, "SSB", max_len); return; }
        if (freq > 0.0f) {
            if (freq >= 14.000f && freq < 14.070f) { strncpy(out_mode, "CW", max_len); return; }
            if (freq >= 14.150f && freq <= 14.350f) { strncpy(out_mode, "SSB", max_len); return; }
        }
        strncpy(out_mode, "OTHER", max_len);
    }

    static void extract_json_value(const char* json, const char* key, char* out, size_t max_len) {
        out[0] = '\0';
        char search[64];
        snprintf(search, sizeof(search), "\"%s\"", key);
        const char* ptr = strstr(json, search);
        if (!ptr) return;
        
        ptr += strlen(search);
        while (*ptr && (*ptr == ' ' || *ptr == ':' || *ptr == '"')) ptr++;
        
        size_t idx = 0;
        while (*ptr && *ptr != '"' && *ptr != ',' && *ptr != '}' && idx < max_len - 1) {
            out[idx++] = *ptr++;
        }
        out[idx] = '\0';
        
        while (idx > 0 && (out[idx - 1] == ' ' || out[idx - 1] == '\r' || out[idx - 1] == '\n')) {
            out[--idx] = '\0';
        }
    }

    static bool read_next_json_object(Stream& stream, char* buffer, size_t max_len) {
        int brace_depth = 0;
        size_t idx = 0;
        unsigned long start_ms = millis();
        
        while (millis() - start_ms < 4000) {
            if (!stream.available()) {
                delay(2); 
                continue;
            }
            char c = stream.read();
            
            if (brace_depth == 0) {
                if (c == '{') {
                    brace_depth = 1;
                    buffer[idx++] = c;
                } else if (c == ']') {
                    return false; 
                }
            } else {
                if (idx < max_len - 1) {
                    buffer[idx++] = c;
                }
                if (c == '{') brace_depth++;
                else if (c == '}') {
                    brace_depth--;
                    if (brace_depth == 0) {
                        buffer[idx] = '\0';
                        return true;
                    }
                }
            }
        }
        return false;
    }

    void PotaManager::fetch_async() {
        if (!spots) {
            spots = (PotaSpot*)calloc(30, sizeof(PotaSpot));
            if (!spots) return;
        }

        if (fetching || (millis() - last_fetch_time < 30000 && last_fetch_time != 0)) return;
        fetching = true;

        Serial.println("[POTA] Synchronous main-thread fetch started.");
        
        WiFiClientSecure secureClient;
        secureClient.setInsecure(); 

        HTTPClient http;
        http.useHTTP10(true); 
        http.begin(secureClient, "https://api.pota.app/spot/activator");
        
        // DYNAMIC USER AGENT
        char user_agent[64];
        snprintf(user_agent, sizeof(user_agent), "%s/%s", meta::FW_NAME, meta::FW_VERSION);
        http.addHeader("User-Agent", user_agent); 
        
        http.setTimeout(8000); 

        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            Stream& stream = http.getStream();
            stream.setTimeout(3000);

            if (stream.find('[')) {
                spot_count = 0;
                char chunk[512]; 

                while (spot_count < 30) {
                    if (!read_next_json_object(stream, chunk, sizeof(chunk))) break;

                    PotaSpot s{0};
                    char time_buf[24] = {0};
                    extract_json_value(chunk, "spotTime", time_buf, sizeof(time_buf));
                    if (strlen(time_buf) >= 16) snprintf(s.time, sizeof(s.time), "%.5s", time_buf + 11);

                    extract_json_value(chunk, "reference", s.reference, sizeof(s.reference));
                    extract_json_value(chunk, "activator", s.activator, sizeof(s.activator));
                    extract_json_value(chunk, "comments", s.comment, sizeof(s.comment));

                    char freq_buf[16] = {0};
                    extract_json_value(chunk, "frequency", freq_buf, sizeof(freq_buf));
                    float f = atof(freq_buf);
                    if (f > 0) s.freq = (f > 1000.0f) ? (f / 1000.0f) : f;

                    extract_json_value(chunk, "mode", s.mode, sizeof(s.mode));
                    if (strlen(s.mode) > 0) {
                        for(char* p = s.mode; *p; ++p) *p = toupper(*p);
                    } else {
                        deduce_mode(s.freq, s.comment, s.mode, sizeof(s.mode));
                    }
                    
                    s.is_qrp = (strcasestr(s.comment, "QRP") != nullptr);
                    spots[spot_count++] = s;
                }
                dirty = true;
                Serial.printf("[POTA] Success. Mapped %u spots.\n", spot_count);
            }
        } else {
            Serial.printf("[POTA] Failed. HTTP Code: %d\n", httpCode);
        }

        http.end();
        secureClient.stop();
        
        last_fetch_time = millis();
        fetching = false;
    }
}