#pragma once
#include <stdint.h>
#include <stddef.h>

namespace services {

    enum ConditionRating {
        RATING_POOR = 0,
        RATING_FAIR,
        RATING_GOOD
    };

    struct PropagationTelemetry {
        uint16_t sfi;
        uint8_t k_index;
        uint8_t a_index;
        char forecast[24];
        bool dirty;
    };

    class PropagationManager {
    public:
        static void init();
        
        // Scrapes incoming plaintext cluster line strings byte-by-byte for markers
        static void parse_cluster_line(const char* line);
        
        // Algorithmic evaluation engine based on solar ionization vs geomagnetic noise
        static ConditionRating get_band_rating(uint8_t band_group_idx, bool look_at_nighttime);
        
        static const PropagationTelemetry& get_telemetry();
        static void clear_dirty();

    private:
        static PropagationTelemetry data;
    };

} // namespace services