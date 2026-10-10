#include "mem_stats.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

namespace mem_stats {

    static Stats stats = {};
    static uint32_t last_ms = 0;

    void update() {
        const uint32_t now = millis();
        if (last_ms && now - last_ms < 5000) return;
        last_ms = now;

        stats.heap_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        stats.heap_min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
        stats.largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        if (!stats.largest_block_min || stats.largest_block < stats.largest_block_min)
            stats.largest_block_min = stats.largest_block;

        lv_mem_monitor_t mon;
        lv_mem_monitor(&mon);
        stats.lvgl_total = mon.total_size;
        stats.lvgl_used = mon.total_size - mon.free_size;
        stats.lvgl_max_used = mon.max_used;
    }

    const Stats& get() { return stats; }

}  // namespace mem_stats
