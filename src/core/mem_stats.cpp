#include "mem_stats.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

#include <string.h>
#include <freertos/timers.h>

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

    static JobRecord jobs[JOBS] = {};
    static int job_n = 0, job_next = 0;
    static JobRecord running = {};
    static uint32_t job_start_ms = 0;
    static TimerHandle_t job_timer = nullptr;
    static uint8_t active_page = 0;
    static uint32_t page_peak[PAGES] = {};

    static void job_sample(TimerHandle_t) {
        const uint32_t f = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        const uint32_t l = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        if (f < running.free_min) running.free_min = f;
        if (l < running.largest_min) running.largest_min = l;
    }

    void job_begin(const char* name) {
        running = {};
        strncpy(running.name, name, sizeof(running.name) - 1);
        running.free_before = running.free_min = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        running.largest_before = running.largest_min = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        job_start_ms = millis();
        if (!job_timer) job_timer = xTimerCreate("job_mem", pdMS_TO_TICKS(50), pdTRUE, nullptr, job_sample);
        if (job_timer) xTimerStart(job_timer, 0);
    }

    void job_end() {
        if (job_timer) xTimerStop(job_timer, 0);
        job_sample(nullptr);
        running.free_after = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        running.largest_after = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        running.ms = millis() - job_start_ms;
        jobs[job_next] = running;
        job_next = (job_next + 1) % JOBS;
        if (job_n < JOBS) job_n++;
        Serial.printf("[Mem] job %s: %lu ms, free %lu -> min %lu -> %lu, largest %lu -> min %lu -> %lu\n", running.name,
                      (unsigned long)running.ms, (unsigned long)running.free_before, (unsigned long)running.free_min,
                      (unsigned long)running.free_after, (unsigned long)running.largest_before,
                      (unsigned long)running.largest_min, (unsigned long)running.largest_after);
    }

    int job_count() { return job_n; }
    const JobRecord& job(int i) { return jobs[(job_next - job_n + i + JOBS) % JOBS]; }

    void note_page(uint8_t page) { active_page = page < PAGES ? page : 0; }
    uint32_t lvgl_page_peak(uint8_t page) { return page < PAGES ? page_peak[page] : 0; }

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
        if (stats.lvgl_used > page_peak[active_page]) page_peak[active_page] = stats.lvgl_used;
    }

    const Stats& get() { return stats; }

}  // namespace mem_stats
