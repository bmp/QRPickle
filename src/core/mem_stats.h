#pragma once
#include <stdint.h>

// Heap and LVGL memory figures, sampled on the main loop (LVGL's monitor isn't thread-safe) and
// reported by /api/status. "Largest block" is what a TLS handshake needs (~40 KB in one piece).
namespace mem_stats {

    struct Stats {
        uint32_t heap_free;  // now
        uint32_t heap_min_free;  // lowest since boot
        uint32_t largest_block;  // now
        uint32_t largest_block_min;  // lowest since boot
        uint32_t lvgl_total;  // LVGL's pool (LV_MEM_SIZE)
        uint32_t lvgl_used;  // now
        uint32_t lvgl_max_used;  // highest since boot
    };

    void update();  // main loop; samples every 5 s
    const Stats& get();

}  // namespace mem_stats
