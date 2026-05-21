#include "display.h"
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>

// Instantiate the TFT_eSPI driver wrapper to interact with the physical display controller
static TFT_eSPI tft = TFT_eSPI();

// Allocate display draw buffer inside internal SRAM (~15KB)
// Holds 24 rows of 320 pixels; 2 bytes per pixel for 16-bit RGB565 color depth
#define DRAW_BUF_SIZE (320 * 24)
static uint8_t draw_buf[DRAW_BUF_SIZE * 2];

// LVGL v9 Flush Callback function
// Transfers the rendered VDB (Video Display Buffer) pixels from RAM over SPI to the display hardware
static void flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);

    // Assert Chip Select (CS) line and open write transaction context
    tft.startWrite();

    // Set the drawing coordinate window boundaries on the controller chip
    tft.setAddrWindow(area->x1, area->y1, w, h);

    // THE FIX: Change false back to true! This triggers high-speed byte swapping
    // to align the ESP32's little-endian RAM data with the big-endian display panel.
    tft.pushColors((uint16_t *)px_map, w * h, true);

    // Release Chip Select (CS) line and close SPI transaction context
    tft.endWrite();

    // Notify LVGL engine that the frame transmission block is complete and ready for next cycle
    lv_display_flush_ready(disp);
}

void display_init() {
    // 1. Initialize physical Backlight Pin (GPIO 21)
    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH);

    // 2. Drive the screen hardware orientation and inversion settings
    tft.begin();
    tft.setRotation(1);        // Landscape view mode
    tft.invertDisplay(true);   // Resolves inverted colors common to standard CYD display clone batches

    // 3. Fire up the core LVGL subsystem
    lv_init();

    // 4. Create and bind the LVGL display object
    // Defines standard landscape bounds (320x240) and links allocated memory blocks
    lv_display_t * disp = lv_display_create(320, 240);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Use the standard public native format
    // Tells the LVGL engine to use the standard 16-bit system depth color schema
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_NATIVE);

    // Bind the hardware transmission flush callback subroutine to the screen object
    lv_display_set_flush_cb(disp, flush_cb);
}

void display_update() {
    // Keep track of total elapsed execution cycles
    static uint32_t last_tick = 0;
    uint32_t current = millis();

    // Pass the delta time difference to inform LVGL how many milliseconds passed since the last poll
    lv_tick_inc(current - last_tick);
    last_tick = current;

    // Direct the core graphics subsystem to execute animations, redraws, and user events
    lv_timer_handler();
}
