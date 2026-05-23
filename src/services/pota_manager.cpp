#include "pota_manager.h"
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <cstring>
#include <ctype.h>
#include <strings.h>

namespace services {

    PotaSpot* PotaManager::spots = nullptr;
    size_t PotaManager::spot_count = 0;
    bool PotaManager::fetching = false;
    bool PotaManager::dirty = false;
    uint32_t PotaManager::last_fetch_time = 0;

    void PotaManager::fetch_async() {
        if (!spots) {
            spots = (PotaSpot*)calloc(30, sizeof(PotaSpot));
            if (!spots) {
                Serial.println("[POTA] Heap allocation failed for spots cache!");
                return; 
            }
        }

        if (fetching || (millis() - last_fetch_time < 45000 && last_fetch_time != 0)) return;
        
        fetching = true;
        
        BaseType_t task_status = xTaskCreate(fetch_task, "pota_task", 8192, NULL, 1, NULL);
        if (task_status != pdPASS) {
            fetching = false;
            Serial.println("[POTA] CRITICAL: FreeRTOS failed to allocate 8KB task stack! OOM.");
        }
    }

    void PotaManager::deduce_mode(float freq, const char* comment, char* out_mode, size_t max_len) {
        if (strcasestr(comment, "FT8"))   { strncpy(out_mode, "FT8", max_len); return; }
        if (strcasestr(comment, "FT4"))   { strncpy(out_mode, "FT4", max_len); return; }
        if (strcasestr(comment, "CW"))    { strncpy(out_mode, "CW", max_len); return; }
        if (strcasestr(comment, "RTTY"))  { strncpy(out_mode, "RTTY", max_len); return; }
        if (strcasestr(comment, "SSB") || strcasestr(comment, "LSB") || strcasestr(comment, "USB")) { 
            strncpy(out_mode, "SSB", max_len); return; 
        }

        if (freq > 0.0f) {
            float mhz = (freq > 1000.0f) ? (freq / 1000.0f) : freq;
            if (abs(mhz - 14.074f) < 0.003f || abs(mhz - 7.074f) < 0.003f || abs(mhz - 21.074f) < 0.003f) {
                strncpy(out_mode, "FT8", max_len); return;
            }
            if (mhz >= 14.000f && mhz < 14.070f) { strncpy(out_mode, "CW", max_len); return; }
            if (mhz >= 14.150f && mhz <= 14.350f) { strncpy(out_mode, "SSB", max_len); return; }
            if (mhz >= 7.000f && mhz < 7.040f) { strncpy(out_mode, "CW", max_len); return; }
            if (mhz >= 7.100f && mhz <= 7.300f) { strncpy(out_mode, "SSB", max_len); return; }
            if (mhz >= 28.300f && mhz <= 29.300f) { strncpy(out_mode, "SSB", max_len); return; }
        }
        strncpy(out_mode, "OTHER", max_len);
    }

    void PotaManager::fetch_task(void* param) {
        Serial.println("[POTA] Network stream request initiated...");

        WiFiClientSecure secureClient;
        secureClient.setInsecure();
        secureClient.setTimeout(10); 

        HTTPClient http;
        http.useHTTP10(true);
        http.begin(secureClient, "https://api.pota.app/spot/activator");
        http.setTimeout(10000); 

        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            Stream& payloadStream = http.getStream();
            payloadStream.setTimeout(5000);

            // --- THE MEMORY FIX: ELEMENT-BY-ELEMENT STREAMING ---
            // Find the opening bracket of the JSON array
            if (payloadStream.find('[')) {
                spot_count = 0;

                // Parse exactly ONE spot object at a time
                do {
                    if (spot_count >= 30) {
                        Serial.println("[POTA] Reached 30 spots. Severing connection early to save RAM/Radio...");
                        break; 
                    }

                    JsonDocument doc; // Only holds a single spot (uses virtually no RAM)
                    DeserializationError err = deserializeJson(doc, payloadStream);

                    if (err) {
                        Serial.printf("[POTA] Parsing loop ended: %s\n", err.c_str());
                        break;
                    }

                    PotaSpot s{0};

                    const char* t_str = doc["spotTime"];
                    if (t_str && strlen(t_str) >= 16) snprintf(s.time, sizeof(s.time), "%.5s", t_str + 11);

                    if (!doc["reference"].isNull()) strncpy(s.reference, doc["reference"], sizeof(s.reference)-1);
                    if (!doc["activator"].isNull()) strncpy(s.activator, doc["activator"], sizeof(s.activator)-1);
                    if (!doc["comments"].isNull())  strncpy(s.comment, doc["comments"], sizeof(s.comment)-1);

                    if (!doc["frequency"].isNull()) {
                        float f = doc["frequency"].as<float>();
                        s.freq = (f > 1000.0f) ? (f / 1000.0f) : f;
                    }

                    if (!doc["mode"].isNull() && strlen(doc["mode"]) > 0) {
                        strncpy(s.mode, doc["mode"], sizeof(s.mode)-1);
                        for(char* p = s.mode; *p; ++p) *p = toupper(*p);
                    } else {
                        deduce_mode(s.freq, s.comment, s.mode, sizeof(s.mode));
                    }

                    s.is_qrp = (strcasestr(s.comment, "QRP") != nullptr);

                    spots[spot_count++] = s;

                // Move the stream cursor to the start of the next element in the array
                } while (payloadStream.findUntil(",", "]"));

                dirty = true;
                last_fetch_time = millis();
                Serial.printf("[POTA] Successfully mapped %u spots to cache.\n", spot_count);
            } else {
                Serial.println("[POTA] Failed to find JSON array start.");
            }
        } else {
            Serial.printf("[POTA] Server rejected HTTP GET: %d\n", httpCode);
        }

        // Immediately close the TCP socket. Discards the unneeded 40KB of data.
        http.end(); 
        fetching = false;
        vTaskDelete(NULL);
    }
}