#include "touch.h"
#include "../services/display_manager.h" // FIXED: Linked to dedicated touch-block logic
#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>

#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK  25
#define TOUCH_CS   33

static SPIClass touchSpi = SPIClass(HSPI);
static XPT2046_Touchscreen ts(TOUCH_CS, 255);

#define TS_MIN_X 300
#define TS_MAX_X 3800
#define TS_MIN_Y 200
#define TS_MAX_Y 3800

static void touch_read_cb(lv_indev_t * indev, lv_indev_data_t * data) {
    // Determine the baseline physical hardware contact state
    bool is_contacting = ts.touched();

    // FIXED: Trap the raw physical interrupt to process wake logic safely
    if (is_contacting) {
        if (services::display_manager::is_sleeping()) {
            services::display_manager::wake();
            // Critical Return: Even though they are pressing, we abort reporting the coordinate to LVGL
            data->state = LV_INDEV_STATE_RELEASED;
            return; 
        }

        // Apply ghost tap lockout window immediately following a wake event
        if (services::display_manager::should_block_touch()) {
            data->state = LV_INDEV_STATE_RELEASED;
            return;
        }

        // Standard coordinate generation pipeline for active displays
        TS_Point p = ts.getPoint();
        int16_t x = map(p.x, TS_MIN_X, TS_MAX_X, 0, 320);
        int16_t y = map(p.y, TS_MIN_Y, TS_MAX_Y, 0, 240);

        if (x < 0) x = 0;
        if (x > 319) x = 319;
        if (y < 0) y = 0;
        if (y > 239) y = 239;

        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void touch_init() {
    touchSpi.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    ts.begin(touchSpi);
    ts.setRotation(1);

    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
}