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
            
            // --- NEW: Print calculated day/night conditions for verification ---
            const char* rating_strs[] = {"POOR", "FAIR", "GOOD"};
            const char* band_names[]  = {"80m-40m", "30m-20m", "17m-15m", "12m-10m"};
            
            Serial.println("[PROP-ENGINE] Evaluated Band Conditions (Day / Night):");
            for (int i = 0; i < 4; i++) {
                ConditionRating r_day = get_band_rating(i, false);
                ConditionRating r_nt  = get_band_rating(i, true);
                Serial.printf("  -> %s: %s / %s\n", band_names[i], rating_strs[r_day], rating_strs[r_nt]);
            }
            Serial.println("---------------------------------------------------------");
        }
    }

    ConditionRating PropagationManager::get_band_rating(uint8_t band_group_idx, bool look_at_nighttime) {
        // K-Index >= 5 indicates a geomagnetic storm, wiping out most HF propagation
        if (data.k_index >= 5) return RATING_POOR;

        ConditionRating rating = RATING_POOR;

        switch (band_group_idx) {
            case 0: // 80m-40m (Lower HF: Heavily dependent on K-Index)
                if (look_at_nighttime) {
                    if (data.k_index <= 2) rating = RATING_GOOD;
                    else if (data.k_index <= 4) rating = RATING_FAIR;
                } else {
                    // Daytime absorption is high, but FAIR is possible with low noise
                    if (data.k_index <= 3) rating = RATING_FAIR;
                }
                break;

            case 1: // 30m-20m (Mid HF: The reliable workhorses)
                if (!look_at_nighttime) {
                    if (data.sfi >= 90) rating = RATING_GOOD;
                    else if (data.sfi >= 70) rating = RATING_FAIR;
                } else {
                    if (data.sfi >= 100 && data.k_index <= 2) rating = RATING_GOOD;
                    else if (data.sfi >= 80 && data.k_index <= 3) rating = RATING_FAIR;
                }
                break;

            case 2: // 17m-15m (Upper HF: Needs solid SFI)
                if (!look_at_nighttime) {
                    if (data.sfi >= 95) rating = RATING_GOOD;
                    else if (data.sfi >= 80) rating = RATING_FAIR;
                } else {
                    if (data.sfi >= 120) rating = RATING_GOOD;
                    else if (data.sfi >= 95) rating = RATING_FAIR;
                }
                break;

            case 3: // 12m-10m (Highest HF: Needs excellent SFI)
                if (!look_at_nighttime) {
                    if (data.sfi >= 110) rating = RATING_GOOD;
                    else if (data.sfi >= 90) rating = RATING_FAIR;
                } else {
                    if (data.sfi >= 150) rating = RATING_GOOD;
                    else if (data.sfi >= 120) rating = RATING_FAIR;
                }
                break;
        }

        // Apply a general penalty: K-Index of 4 (Minor disturbances) degrades marginal bands
        if (data.k_index == 4 && rating > RATING_POOR) {
            rating = (ConditionRating)(rating - 1);
        }

        return rating;
    }

    const PropagationTelemetry& PropagationManager::get_telemetry() { return data; }
    void PropagationManager::clear_dirty() { data.dirty = false; }

} // namespace services
