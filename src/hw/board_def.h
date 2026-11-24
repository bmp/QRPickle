#pragma once
// One CYD board as the firmware sees it (host-safe: no Arduino). The list itself is generated from
// boards/boards.toml into boards_gen.h (scripts/gen_boards.py); never edit boards_gen.h by hand.
#include <stddef.h>
#include <stdint.h>

namespace hw {

    enum class BoardStatus : uint8_t { Tested, Reported, Mapped };
    enum class PanelInit : uint8_t { ILI9341_2, ILI9341, ST7789, ST7796 };
    enum class TouchType : uint8_t { None, XPT2046Own, XPT2046Shared, GT911 };

    constexpr int8_t NO_PIN = -1;
    constexpr uint32_t NO_ID = 0xFFFFFFFFu;  // the board's list entry has no such ID

    struct BoardDef {
        const char* id;     // stable key (NVS "board", reports)
        const char* model;  // printed on the back
        const char* name;   // "ESP32-2432S028R (2.8", USB-C + micro-USB)"
        BoardStatus status;
        bool complete;      // every required value present; else never selectable
        bool addons_v04;    // v0.4 add-ons tested on this board

        // Screen (landscape, as TFT_eSPI rotation 1)
        PanelInit init;
        uint32_t rddid;  // 0x04, 24 bits after one dummy clock; NO_ID if unknown
        uint32_t rdid4;  // 0xD3 (ST7796 direct, ILI9341 via the 0xD9 index); NO_ID if unknown
        uint16_t width, height;
        bool swap_xy, mirror_x, mirror_y, bgr, invert;
        int16_t ui_x, ui_y;  // where the 320x240 UI sits on the panel
        int8_t sclk, mosi, miso, cs, dc;

        int8_t bl_pin;
        bool bl_active_high;

        int8_t led_r, led_g, led_b;
        bool led_active_low;

        TouchType touch;
        int8_t t_sclk, t_mosi, t_miso, t_cs, t_irq;  // XPT2046 (shared bus: only cs and irq)
        int8_t t_sda, t_scl, t_rst;                  // I2C touch chips (also for probing)
        uint8_t t_addr;                              // I2C address, 0 if none
        int16_t cal[4];                              // XPT2046 x_min, x_max, y_min, y_max
        const char* touch_note;                      // "" or e.g. "touch not supported yet (CST816S)"

        int8_t i2c_sda, i2c_scl;  // module connector; NO_PIN = sensors off
        int8_t ldr_gpio;
        int8_t sd_cs;

        uint64_t chip_outputs;  // bit n: GPIO n is driven by a chip on this board
    };

}  // namespace hw
