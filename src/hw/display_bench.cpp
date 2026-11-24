// Full-screen redraw benchmark (QRP_BENCH test builds only; never released), from the v0.3.2 spike.
// 20 s after boot (dashboard up), then twice more 20 s apart: 10 forced full redraws of the active
// screen, timed until the last pixel has left (display_wait_idle), plus heap and LVGL memory.
#ifdef QRP_BENCH
#include "display.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

static void bench_cb(lv_timer_t *) {
    lv_display_t * d = lv_display_get_default();
    const int N = 10;
    uint32_t sum = 0, best = UINT32_MAX, worst = 0;
    for (int i = 0; i < N; i++) {
        lv_obj_invalidate(lv_screen_active());
        uint32_t t0 = micros();
        lv_refr_now(d);
        display_wait_idle();
        uint32_t dt = micros() - t0;
        sum += dt;
        if (dt < best) best = dt;
        if (dt > worst) worst = dt;
    }
    lv_mem_monitor_t m;
    lv_mem_monitor(&m);
    Serial.printf("[BENCH] full redraw x%d: avg %lu us, min %lu, max %lu | heap free %u, largest 8bit %u | lvgl used %u peak %u\n",
                  N, (unsigned long)(sum / N), (unsigned long)best, (unsigned long)worst,
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
                  (unsigned)(m.total_size - m.free_size), (unsigned)m.max_used);
}

void display_bench_start() {
    lv_timer_t * t = lv_timer_create(bench_cb, 20000, NULL);
    lv_timer_set_repeat_count(t, 3);
}
#endif
