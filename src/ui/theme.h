#pragma once
#include <lvgl.h>

namespace ui {

    // The compiler automatically numbers these 0, 1, 2, 3, 4, 5, 6.
    // THEME_COUNT automatically becomes 7!
    enum ThemeId : uint8_t {
        THEME_CLASSIC,
        THEME_FIELD_RED,
        THEME_SLATE_DARK,
        THEME_LIGHT,
        THEME_TERMINAL_GREEN,
        THEME_EINK_LIGHT,
        THEME_EINK_DARK,
        THEME_COUNT
    };

    enum ThemeToken {
        COLOR_BG_APP, COLOR_BG_PANEL, COLOR_BG_BAR, COLOR_BORDER,
        COLOR_TEXT_MAIN, COLOR_TEXT_MUTED, COLOR_ACCENT_PRIMARY, COLOR_ACCENT_SECONDARY,
        COLOR_SENSOR_TEMP, COLOR_SENSOR_PRES,
        COLOR_BAND_GOOD, COLOR_BAND_FAIR, COLOR_BAND_POOR, COLOR_BAND_DOWN
    };

    lv_color_t theme_color(ThemeToken token);
    const char* theme_get_name(uint8_t theme_id);

} // namespace ui