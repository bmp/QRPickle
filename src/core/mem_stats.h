#pragma once
#include <stdint.h>

// Heap and LVGL memory figures, sampled on the main loop (LVGL's monitor isn't thread-safe) and
// reported by /api/status. "Largest block" is what a TLS handshake needs (~40 KB in one piece).
namespace mem_stats {

    struct Stats {
        uint32_t heap_free;  // now (8-bit)
        uint32_t heap_min_free;  // lowest since boot
        uint32_t largest_block;  // now
        uint32_t largest_block_min;  // lowest since boot
        uint32_t lvgl_total;  // LVGL's pool (LV_MEM_SIZE)
        uint32_t lvgl_used;  // now
        uint32_t lvgl_max_used;  // highest since boot
    };

    void update();  // main loop; samples every 5 s
    const Stats& get();

    // Stack headroom per task (bytes never used, lowest seen), to size the stacks from evidence.
    enum StackSlot : uint8_t {
        STK_APRS,
        STK_HAMALERT,
        STK_OTA_CHECK,
        STK_SOLAR,
        STK_POTA,
        STK_ASYNC_TCP,  // the web server's network task (AsyncTCP)
        STK_LOOP,  // Arduino's loopTask (LVGL, UI)
        STK_COUNT
    };
    // Long-lived tasks: sampled by update() through their handle.
    void watch_stack(StackSlot slot, void* task_handle);
    // Short-lived tasks call this just before vTaskDelete(NULL).
    void note_stack(StackSlot slot);
    uint32_t stack_free(StackSlot slot);  // 0 = not measured yet
    const char* stack_name(StackSlot slot);

}  // namespace mem_stats
