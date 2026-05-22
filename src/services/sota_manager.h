#pragma once
#include <stdint.h>
#include <stddef.h>

namespace services {
    struct SotaSpot {
        char time[6];
        char summit[14];
        float freq;
        char mode[8];
        char activator[12];
        char comment[40];
        bool is_qrp;
    };

    class SotaManager {
    public:
        static void fetch_async();
        static const SotaSpot* get_spots() { return spots; }
        static size_t get_spot_count() { return spot_count; }
        static bool is_fetching() { return fetching; }
        static bool is_dirty() { return dirty; }
        static void clear_dirty() { dirty = false; }

    private:
        static SotaSpot* spots; // FIXED: Shifted to dynamic memory pointer
        static size_t spot_count;
        static bool fetching;
        static bool dirty;
        static uint32_t last_fetch_time;

        static void fetch_task(void* param);
        static void deduce_mode(float freq, const char* comment, char* out_mode, size_t max_len);
    };
}