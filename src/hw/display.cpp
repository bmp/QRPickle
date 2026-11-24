// The screen: LVGL draws; this file starts the panel the active board's way (src/hw/board.h,
// display_init_seq.h) and sends LVGL's pixels over SPI with Arduino's writePixels (swaps each RGB565
// pixel while filling the FIFO: measured 235 ms per full redraw on the owner's board vs 253 ms with
// TFT_eSPI, 2026-11-24). The LVGL display is always 320x240; bigger panels show it centred.
#include "led_rgb.h"
#include "display.h"
#include "board.h"
#include "display_init_seq.h"
#include "../services/display_manager.h"
#include <Arduino.h>
#include <SPI.h>
#include <lvgl.h>
#ifdef QRP_SCREEN_TOOLS
#include "../core/screen_tools.h"
#endif

static SPIClass lcd_spi(VSPI);
static const SPISettings LCD_SPI(40000000, MSBFIRST, SPI_MODE0);
static int8_t pin_cs = -1, pin_dc = -1;
static int16_t off_x = 0, off_y = 0;  // where the 320x240 UI sits on the panel

#define DRAW_BUF_SIZE (320 * 10)
alignas(4) static uint8_t draw_buf[DRAW_BUF_SIZE * 2];

static void begin_write() {
    lcd_spi.beginTransaction(LCD_SPI);
    digitalWrite(pin_cs, LOW);
}

static void end_write() {
    digitalWrite(pin_cs, HIGH);
    lcd_spi.endTransaction();
}

// Inside a transaction: a command byte (DC low), then data follows with DC high.
static void cmd(uint8_t c) {
    digitalWrite(pin_dc, LOW);
    lcd_spi.write(c);
    digitalWrite(pin_dc, HIGH);
}

static void command(uint8_t c, const uint8_t* data = nullptr, size_t n = 0) {
    begin_write();
    cmd(c);
    if (n) lcd_spi.writeBytes(data, n);
    end_write();
}

static void send_list(const uint8_t* l) {
    for (;;) {
        const uint8_t c = *l++, n = *l++;
        if (c == hw::init_seq::DELAY) {
            if (n == hw::init_seq::END) return;
            delay(n * 10u);
            continue;
        }
        command(c, l, n);
        l += n;
    }
}

static void window(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    const uint8_t ca[4] = {(uint8_t)(x1 >> 8), (uint8_t)x1, (uint8_t)(x2 >> 8), (uint8_t)x2};
    const uint8_t ra[4] = {(uint8_t)(y1 >> 8), (uint8_t)y1, (uint8_t)(y2 >> 8), (uint8_t)y2};
    cmd(0x2A);
    lcd_spi.writeBytes(ca, 4);
    cmd(0x2B);
    lcd_spi.writeBytes(ra, 4);
}

static void flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
#ifdef QRP_SCREEN_TOOLS
    screen_tools::on_flush(area->x1, area->y1, area->x2, area->y2, px_map);
#endif
    const uint32_t n = (uint32_t)lv_area_get_width(area) * (uint32_t)lv_area_get_height(area);
    begin_write();
    window(area->x1 + off_x, area->y1 + off_y, area->x2 + off_x, area->y2 + off_y);
    cmd(0x2C);
    lcd_spi.writePixels(px_map, n * 2);
    end_write();
    lv_display_flush_ready(disp);
}

static const uint8_t* start_list(hw::PanelInit init) {
    switch (init) {
        case hw::PanelInit::ILI9341: return hw::init_seq::ILI9341;
        case hw::PanelInit::ST7789:  return hw::init_seq::ST7789;
        case hw::PanelInit::ST7796:  return hw::init_seq::ST7796;
        default:                     return hw::init_seq::ILI9341_2;
    }
}

// The panel shows whatever its memory holds once switched on: black it out first. The backlight is
// still off here (display_manager::init), so nothing of the start-up is visible.
static void clear_panel(uint16_t w, uint16_t h) {
    memset(draw_buf, 0, sizeof(draw_buf));
    begin_write();
    window(0, 0, w - 1, h - 1);
    cmd(0x2C);
    for (uint32_t left = (uint32_t)w * h * 2; left;) {
        const uint32_t n = left < sizeof(draw_buf) ? left : sizeof(draw_buf);
        lcd_spi.writeBytes(draw_buf, n);
        left -= n;
    }
    end_write();
}

void display_init() {
    services::display_manager::init();  // backlight off until the panel is ready
    const hw::BoardDef& b = hw::board::active();
    pin_cs = b.cs;
    pin_dc = b.dc;
    off_x = b.ui_x;
    off_y = b.ui_y;
    pinMode(pin_cs, OUTPUT);
    pinMode(pin_dc, OUTPUT);
    digitalWrite(pin_cs, HIGH);
    lcd_spi.begin(b.sclk, b.miso, b.mosi, -1);

    Serial.printf("  [Display Sub-Check] Starting the %s panel...\n", b.model);
    command(0x01);  // software reset (TFT_RST is tied high on these boards)
    delay(150);
    command(0x11);  // sleep out
    delay(120);
    command(0x28);  // display off until cleared
    send_list(start_list(b.init));
    const uint8_t colmod = 0x55;  // 16 bits per pixel
    command(0x3A, &colmod, 1);
    const uint8_t madctl = (b.mirror_y ? 0x80 : 0) | (b.mirror_x ? 0x40 : 0) | (b.swap_xy ? 0x20 : 0) | (b.bgr ? 0x08 : 0);
    command(0x36, &madctl, 1);
    command(b.invert ? 0x21 : 0x20);
    clear_panel(b.width, b.height);
    command(0x29);  // display on
    delay(20);

    Serial.println("  [Display Sub-Check] Initializing LVGL core subsystem structures (lv_init)..."); Serial.flush();
    lv_init();

    lv_display_t* disp = lv_display_create(320, 240);
    if (disp == nullptr) {
        Serial.println("\n[CRITICAL MEMORY FAULT] lv_display_create returned NULL!");
        hw::led_rgb::set_state(hw::led_rgb::STATE_FAULT);  // visible without a serial console (review 5.5)
        while (1) { delay(100); }
    }
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_NATIVE);
    lv_display_set_flush_cb(disp, flush_cb);

    services::display_manager::backlight_ready();
#ifdef QRP_BENCH
    display_bench_start();
#endif
    Serial.println("  [Display Sub-Check] Display initialization successfully completed."); Serial.flush();
}

void display_wait_idle() {}

SPIClass& display_spi() { return lcd_spi; }
