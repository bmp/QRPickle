#include "touch.h"
#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>

// The CYD physically wires the touch controller to these specific pins
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK  25
#define TOUCH_CS   33

// Create a dedicated secondary SPI bus (HSPI) just for the touch controller
static SPIClass touchSpi = SPIClass(HSPI);

// CS=33, IRQ=255 to enforce strict software polling
static XPT2046_Touchscreen ts(TOUCH_CS, 255);

// CYD Resistive calibration boundaries
#define TS_MIN_X 300
#define TS_MAX_X 3800
#define TS_MIN_Y 200
#define TS_MAX_Y 3800

static void touch_read_cb(lv_indev_t * indev, lv_indev_data_t * data) {
    if (ts.touched()) {
        TS_Point p = ts.getPoint();

        // Map the raw hardware values to the 320x240 landscape layout
        int16_t x = map(p.x, TS_MIN_X, TS_MAX_X, 0, 320);
        int16_t y = map(p.y, TS_MIN_Y, TS_MAX_Y, 0, 240);

        // Clamping constraints
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
    // 1. Initialize the dedicated touch SPI bus
    touchSpi.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);

    // 2. Bind the touch library to this dedicated bus
    ts.begin(touchSpi);
    ts.setRotation(1);

    // 3. Register the pointer device with the LVGL engine
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
}
