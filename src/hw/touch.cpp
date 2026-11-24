// Touch input for LVGL from the active board (hw::board): XPT2046 on its own SPI bus (the 2.8" boards),
// XPT2046 on the screen's bus (2.4"/3.2"/3.5" R boards), or GT911 over I2C (3.2"/3.5" C boards). Raw
// points become panel pixels (landscape), then UI coordinates (the UI may be centred on a bigger panel).
// The LVGL input device always exists: the screenshot build's taps come through here on every board.
#include "touch.h"
#include "board.h"
#include "display.h"
#include "touch_gt911.h"
#include "../services/display_manager.h"
#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>
#ifdef QRP_SCREEN_TOOLS
#include "../core/screen_tools.h"
#endif

static SPIClass touchSpi = SPIClass(HSPI);
static XPT2046_Touchscreen* xpt = nullptr;
static bool use_gt911 = false;

// Raw touch -> panel pixels (landscape). False when nothing is pressed.
static bool read_panel(int16_t& px, int16_t& py) {
    const hw::BoardDef& b = hw::board::active();
    if (xpt) {
        if (!xpt->touched()) return false;
        TS_Point p = xpt->getPoint();
        px = (int16_t)map(p.x, b.cal[0], b.cal[1], 0, b.width);
        py = (int16_t)map(p.y, b.cal[2], b.cal[3], 0, b.height);
        return true;
    }
    if (use_gt911) return hw::gt911::read(px, py);
    return false;
}

static void touch_read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    (void)indev;
#ifdef QRP_SCREEN_TOOLS
    // Taps requested by tools/device_screens.py (screenshot builds only).
    int16_t tx, ty;
    bool pressed;
    if (screen_tools::tap_read(tx, ty, pressed)) {
        if (services::display_manager::is_sleeping()) services::display_manager::wake();
        data->point.x = tx;
        data->point.y = ty;
        data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
        return;
    }
#endif
    int16_t px, py;
    if (!read_panel(px, py)) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    // A touch on a dark screen only wakes it; the point is not passed on.
    if (services::display_manager::is_sleeping()) {
        services::display_manager::wake();
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    // Ghost-tap lockout right after waking.
    if (services::display_manager::should_block_touch()) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    const hw::BoardDef& b = hw::board::active();
    if (px < 0) px = 0;
    if (py < 0) py = 0;
    if (px >= b.width) px = b.width - 1;
    if (py >= b.height) py = b.height - 1;
    int ux, uy;
    if (!hw::rules::map_touch(b, px, py, &ux, &uy)) {  // in the border around a centred UI
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    data->point.x = ux;
    data->point.y = uy;
    data->state = LV_INDEV_STATE_PRESSED;
}

void touch_init() {
    const hw::BoardDef& b = hw::board::active();
    if (hw::board::selection().touch_on) {
        switch (b.touch) {
            case hw::TouchType::XPT2046Own:
                touchSpi.begin(b.t_sclk, b.t_miso, b.t_mosi, b.t_cs);
                xpt = new XPT2046_Touchscreen(b.t_cs, 255);
                xpt->begin(touchSpi);
                xpt->setRotation(1);
                break;
            case hw::TouchType::XPT2046Shared:  // the screen's SPI object: no second driver on the bus
                xpt = new XPT2046_Touchscreen(b.t_cs, 255);
                xpt->begin(display_spi());
                xpt->setRotation(1);
                break;
            case hw::TouchType::GT911:
                use_gt911 = hw::gt911::begin(b);
                break;
            default:
                break;
        }
    }
    Serial.printf("[Touch] %s\n", xpt ? "XPT2046" : use_gt911 ? "GT911" : "off (no supported touch chip)");

    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
}

bool touch_available() { return xpt || use_gt911; }
