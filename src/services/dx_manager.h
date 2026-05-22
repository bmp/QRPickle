#pragma once
#include <stdint.h>
#include <stddef.h>

namespace services {

    enum DxStatus {
        DX_STATUS_DISCONNECTED,
        DX_STATUS_CONNECTING,
        DX_STATUS_AUTHORIZING,
        DX_STATUS_CONNECTED
    };

    struct DxSpot {
        char dx_call[12];
        float freq;
        char mode[8];
        char spotter[12];
        char comment[32];
    };

    class DxManager {
    public:
        static void start();
        static void stop();
        static void update(); 

        static DxStatus get_status() { return status; }
        static const DxSpot* get_spots() { return spots; } 
        static size_t get_spot_count() { return spot_count; }
        static void clear_spots();

        // FIXED: Added high-speed cache dirty validation flags
        static bool is_dirty() { return buffer_dirty; }
        static void clear_dirty() { buffer_dirty = false; }

    private:
        static DxStatus status;
        static DxSpot* spots; 
        static size_t spot_count;
        static bool buffer_dirty; // Cache invalidation tracker
        
        static char line_buffer[160];
        static size_t line_idx;
        static uint32_t state_timer;
        static bool using_secondary;

        static void handle_line(const char* line);
        static void parse_dx_line(const char* line);
        static void deduce_mode(float freq, const char* comment, char* out_mode, size_t max_len);
    };

} // namespace services