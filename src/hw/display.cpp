#include "display.h"
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>

static TFT_eSPI tft = TFT_eSPI();

// Allocate display draw buffer inside internal SRAM (~15KB)
#define DRAW_BUF_SIZE (320 * 24)
static uint8_t draw_buf[DRAW_BUF_SIZE * 2];

// LVGL v9 Flush Callback function
static void flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);

    // THE FIX: Change false back to true! This triggers high-speed byte swapping
    // to align the ESP32's little-endian RAM data with the big-endian display panel.
    tft.pushColors((uint16_t *)px_map, w * h, true);

    tft.endWrite();

    lv_display_flush_ready(disp);
}

void display_init() {
    // 1. Initialize physical Backlight Pin (GPIO 21)
    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH);

    // 2. Drive the screen hardware orientation and inversion settings
    tft.begin();
    tft.setRotation(1);
    tft.invertDisplay(true);

    // 3. Fire up the core LVGL subsystem
    lv_init();

    // 4. Create and bind the LVGL display object
    lv_display_t * disp = lv_display_create(320, 240);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Use the standard public native format
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_NATIVE);

    lv_display_set_flush_cb(disp, flush_cb);
}

void display_update() {
    static uint32_t last_tick = 0;
    uint32_t current = millis();
    lv_tick_inc(current - last_tick);
    last_tick = current;

    lv_timer_handler();
}
