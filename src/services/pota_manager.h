#ifndef POTA_MANAGER_H
#define POTA_MANAGER_H

#include <cstdint>
#include <cstddef>

namespace services {

    struct PotaSpot {
        char time[12];
        char reference[16];
        float freq;
        char mode[12];
        char activator[16];
        char comment[64];
        bool is_qrp;
    };

    class PotaManager {
    public:
        static void fetch_async();
        static const PotaSpot* get_spots() { return spots; }
        static size_t get_spot_count() { return spot_count; }
        static bool is_fetching() { return fetching; }
        static bool is_dirty() { return dirty; }
        static void clear_dirty() { dirty = false; }
        static uint32_t get_last_fetch_time() { return last_fetch_time; }
        static void expire_timer() { last_fetch_time = 0; } // NEW: Forces refresh on button tap

    private:
        static PotaSpot* spots;
        static size_t spot_count;
        static bool fetching;
        static bool dirty;
        static uint32_t last_fetch_time;

        static void deduce_mode(float freq, const char* comment, char* out_mode, size_t max_len);
    };

} // namespace services

#endif // POTA_MANAGER_H