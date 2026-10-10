#include "led_rgb.h"
#include "display.h"
#include "../services/display_manager.h" // Linked to dedicated power logic
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#ifdef QRP_SCREEN_TOOLS
#include "../core/screen_tools.h"
#endif

static TFT_eSPI tft = TFT_eSPI();

#define DRAW_BUF_SIZE (320 * 10)
alignas(4) static uint8_t draw_buf[DRAW_BUF_SIZE * 2];

static void flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);
#ifdef QRP_SCREEN_TOOLS
    screen_tools::on_flush(area->x1, area->y1, area->x2, area->y2, px_map);
#endif

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)px_map, w * h, true);
    tft.endWrite();
    lv_display_flush_ready(disp);
}

void display_init() {
    // Offloaded raw backlight GPIO init to the dedicated manager
    services::display_manager::init();

    Serial.println("  [Display Sub-Check] Initializing TFT_eSPI driver bus (tft.begin)..."); Serial.flush();
    tft.begin();

    Serial.println("  [Display Sub-Check] Applying rotation and screen inversion parameters..."); Serial.flush();
    tft.setRotation(1);
    tft.invertDisplay(true);

    Serial.println("  [Display Sub-Check] Executing hardware screen clear (fillScreen)..."); Serial.flush();
    tft.fillScreen(0x0000);

    Serial.println("  [Display Sub-Check] Initializing LVGL core subsystem structures (lv_init)..."); Serial.flush();
    lv_init();

    Serial.println("  [Display Sub-Check] Allocating virtual display canvas (lv_display_create)..."); Serial.flush();
    lv_display_t * disp = lv_display_create(320, 240);

    if (disp == nullptr) {
        Serial.println("\n[CRITICAL MEMORY FAULT] lv_display_create returned NULL!");
        hw::led_rgb::set_state(hw::led_rgb::STATE_FAULT);  // visible without a serial console (review 5.5)
        while (1) { delay(100); } 
    }

    Serial.println("  [Display Sub-Check] Assigning static drawing buffers..."); Serial.flush();
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    Serial.println("  [Display Sub-Check] Selecting native color formatting rules..."); Serial.flush();
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_NATIVE);

    Serial.println("  [Display Sub-Check] Registering low-level flush callback hooks..."); Serial.flush();
    lv_display_set_flush_cb(disp, flush_cb);

    Serial.println("  [Display Sub-Check] Display initialization successfully completed."); Serial.flush();
}
