#include "sota_manager.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <cstring>
#include <ctype.h>
#include <strings.h>

namespace services {

    SotaSpot* SotaManager::spots = nullptr; 
    size_t SotaManager::spot_count = 0;
    bool SotaManager::fetching = false;
    bool SotaManager::dirty = false;
    uint32_t SotaManager::last_fetch_time = 0;

    void SotaManager::fetch_async() {
        if (!spots) {
            spots = (SotaSpot*)calloc(30, sizeof(SotaSpot));
            if (!spots) {
                Serial.println("[SOTA] Heap allocation failed for spots cache!");
                return;
            }
        }

        if (fetching || (millis() - last_fetch_time < 45000 && last_fetch_time != 0)) return;
        
        fetching = true;
        
        // FIXED: Safeguard against FreeRTOS stack starvation
        BaseType_t task_status = xTaskCreate(fetch_task, "sota_task", 8192, NULL, 1, NULL);
        if (task_status != pdPASS) {
            fetching = false;
            Serial.println("[SOTA] CRITICAL: FreeRTOS failed to allocate 8KB task stack! OOM.");
        }
    }

    void SotaManager::deduce_mode(float freq, const char* comment, char* out_mode, size_t max_len) {
        if (strcasestr(comment, "FT8"))   { strncpy(out_mode, "FT8", max_len); return; }
        if (strcasestr(comment, "CW"))    { strncpy(out_mode, "CW", max_len); return; }
        if (strcasestr(comment, "SSB"))   { strncpy(out_mode, "SSB", max_len); return; }
        if (freq > 0.0f) {
            if (freq >= 14.000f && freq < 14.070f) { strncpy(out_mode, "CW", max_len); return; }
            if (freq >= 14.150f && freq <= 14.350f) { strncpy(out_mode, "SSB", max_len); return; }
        }
        strncpy(out_mode, "OTHER", max_len);
    }

    void SotaManager::fetch_task(void* param) {
        Serial.println("[SOTA] Network stream request initiated...");

        HTTPClient http;
        http.useHTTP10(true); 
        http.begin("https://api2.sota.org.uk/api/spots/30/all"); 
        http.setTimeout(8000);
        
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            JsonDocument filter;
            filter[0]["timeStamp"] = true;
            filter[0]["associationCode"] = true;
            filter[0]["summitCode"] = true;
            filter[0]["frequency"] = true;
            filter[0]["mode"] = true;
            filter[0]["callsign"] = true;
            filter[0]["comments"] = true;

            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
            
            if (!err) {
                JsonArray arr = doc.as<JsonArray>();
                spot_count = 0;
                for (JsonVariant v : arr) {
                    if (spot_count >= 30) break;
                    
                    SotaSpot s{0};
                    
                    const char* t_str = v["timeStamp"];
                    if (t_str && strlen(t_str) >= 16) snprintf(s.time, sizeof(s.time), "%.5s", t_str + 11);
                    
                    if (!v["associationCode"].isNull() && !v["summitCode"].isNull()) {
                        snprintf(s.summit, sizeof(s.summit), "%s/%s", v["associationCode"].as<const char*>(), v["summitCode"].as<const char*>());
                    }
                    
                    if (!v["callsign"].isNull()) strncpy(s.activator, v["callsign"], sizeof(s.activator)-1);
                    if (!v["comments"].isNull()) strncpy(s.comment, v["comments"], sizeof(s.comment)-1);
                    
                    if (!v["frequency"].isNull()) {
                        s.freq = v["frequency"].as<float>();
                    }
                    
                    if (!v["mode"].isNull() && strlen(v["mode"]) > 0) {
                        strncpy(s.mode, v["mode"], sizeof(s.mode)-1);
                        for(char* p = s.mode; *p; ++p) *p = toupper(*p);
                    } else {
                        deduce_mode(s.freq, s.comment, s.mode, sizeof(s.mode));
                    }
                    
                    s.is_qrp = (strcasestr(s.comment, "QRP") != nullptr);
                    spots[spot_count++] = s;
                }
                dirty = true;
                last_fetch_time = millis();
                Serial.printf("[SOTA] Successfully mapped %u spots to cache.\n", spot_count);
            } else {
                Serial.printf("[SOTA] Deserialization crashed: %s\n", err.c_str());
            }
        } else {
            Serial.printf("[SOTA] Server rejected HTTP GET: %d\n", httpCode);
        }
        
        http.end();
        fetching = false;
        vTaskDelete(NULL);
    }
}