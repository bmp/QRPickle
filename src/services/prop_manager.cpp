#include "prop_manager.h"
#include <HardwareSerial.h> // For Serial logging
#include <cstring>
#include <cstdlib>

namespace services {

    PropagationTelemetry PropagationManager::data;

    void PropagationManager::init() {
        data.sfi = 100;       
        data.k_index = 2;     
        data.a_index = 10;    
        std::strcpy(data.forecast, "No Storms");
        data.dirty = true;
    }

    void PropagationManager::parse_cluster_line(const char* line) {
        bool updated = false;

        const char* token_sfi = std::strstr(line, "SFI=");
        if (token_sfi) {
            int val = std::atoi(token_sfi + 4);
            if (val > 0) { data.sfi = val; data.dirty = true; updated = true; }
        }

        const char* token_k = std::strstr(line, "K=");
        if (token_k) {
            int val = std::atoi(token_k + 2);
            if (val >= 0 && val <= 9) { data.k_index = val; data.dirty = true; updated = true; }
        }

        const char* token_a = std::strstr(line, "A=");
        if (token_a) {
            int val = std::atoi(token_a + 2);
            if (val >= 0) { data.a_index = val; data.dirty = true; updated = true; }
        }

        const char* token_fore = std::strstr(line, "Forecast:");
        if (token_fore) {
            std::strncpy(data.forecast, token_fore + 9, sizeof(data.forecast) - 1);
            data.forecast[sizeof(data.forecast) - 1] = '\0';
            data.dirty = true;
            updated = true;
        }

        // DYNAMIC DIAGNOSTIC PRINT LINK
        if (updated) {
            Serial.printf("[PROP-ENGINE] Scraped solar data from stream -> SFI:%u | K-INDEX:%u | A-INDEX:%u | FCST:%s\n",
                          data.sfi, data.k_index, data.a_index, data.forecast);
        }
    }

    ConditionRating PropagationManager::get_band_rating(uint8_t band_group_idx, bool look_at_nighttime) {
        if (data.k_index >= 5) return RATING_POOR; 

        ConditionRating rating = RATING_POOR;

        switch (band_group_idx) {
            case 0: // 80m-40m
                if (look_at_nighttime) {
                    if (data.k_index <= 2) rating = RATING_GOOD;
                    else if (data.k_index == 3) rating = RATING_FAIR;
                    else rating = RATING_POOR;
                } else {
                    rating = RATING_POOR; 
                }
                break;

            case 1: // 30m-20m
                if (!look_at_nighttime) {
                    if (data.sfi >= 120) rating = RATING_GOOD;
                    else if (data.sfi >= 95) rating = RATING_FAIR;
                    else rating = RATING_POOR;
                } else {
                    if (data.sfi >= 110 && data.k_index <= 3) rating = RATING_FAIR;
                    else rating = RATING_POOR;
                }
                break;

            case 2: // 17m-15m
                if (!look_at_nighttime) {
                    if (data.sfi >= 130) rating = RATING_GOOD;
                    else if (data.sfi >= 100) rating = RATING_FAIR;
                    else rating = RATING_POOR;
                } else {
                    rating = RATING_POOR; 
                }
                break;

            case 3: // 12m-10m
                if (!look_at_nighttime) {
                    if (data.sfi >= 145) rating = RATING_GOOD;
                    else if (data.sfi >= 115) rating = RATING_FAIR;
                    else rating = RATING_POOR;
                } else {
                    rating = RATING_POOR;
                }
                break;
        }

        if (data.k_index == 4 && rating > RATING_POOR) {
            rating = (ConditionRating)(rating - 1);
        }

        return rating;
    }

    const PropagationTelemetry& PropagationManager::get_telemetry() { return data; }
    void PropagationManager::clear_dirty() { data.dirty = false; }

} // namespace services