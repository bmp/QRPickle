#include "mem_stats.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

namespace mem_stats {

    static Stats stats = {};
    static uint32_t last_ms = 0;
    static uint32_t stack_min[STK_COUNT] = {};
    static void* watched[STK_COUNT] = {};
    static const char* const STACK_NAMES[STK_COUNT] = {"aprs", "hamalert", "ota_check", "solar", "pota", "async_tcp", "loop", "space_fetch", "space_passes"};

    static void record(StackSlot slot, uint32_t free_bytes) {
        if (!stack_min[slot] || free_bytes < stack_min[slot]) stack_min[slot] = free_bytes;
    }

    void watch_stack(StackSlot slot, void* task_handle) { watched[slot] = task_handle; }
    // ESP-IDF's uxTaskGetStackHighWaterMark() returns bytes.
    void note_stack(StackSlot slot) { record(slot, uxTaskGetStackHighWaterMark(nullptr)); }
    uint32_t stack_free(StackSlot slot) { return stack_min[slot]; }
    const char* stack_name(StackSlot slot) { return STACK_NAMES[slot]; }

    void update() {
        const uint32_t now = millis();
        if (last_ms && now - last_ms < 5000) return;
        last_ms = now;

        stats.heap_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        stats.heap_min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
        stats.largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        if (!stats.largest_block_min || stats.largest_block < stats.largest_block_min)
            stats.largest_block_min = stats.largest_block;

        if (!watched[STK_ASYNC_TCP]) watched[STK_ASYNC_TCP] = xTaskGetHandle("async_tcp");  // created by the web server
        if (!watched[STK_LOOP]) watched[STK_LOOP] = xTaskGetHandle("loopTask");
        for (int i = 0; i < STK_COUNT; i++)
            if (watched[i]) record((StackSlot)i, uxTaskGetStackHighWaterMark((TaskHandle_t)watched[i]));

        lv_mem_monitor_t mon;
        lv_mem_monitor(&mon);
        stats.lvgl_total = mon.total_size;
        stats.lvgl_used = mon.total_size - mon.free_size;
        stats.lvgl_max_used = mon.max_used;
    }

    const Stats& get() { return stats; }

}  // namespace mem_stats
