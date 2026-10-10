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
        COLOR_BG_APP,
        COLOR_BG_PANEL,
        COLOR_BG_BAR,
        COLOR_BORDER,
        COLOR_TEXT_MAIN,
        COLOR_TEXT_MUTED,
        COLOR_ACCENT_PRIMARY,
        COLOR_ACCENT_SECONDARY,
        COLOR_SENSOR_TEMP,
        COLOR_SENSOR_PRES,
        COLOR_BAND_GOOD,
        COLOR_BAND_FAIR,
        COLOR_BAND_POOR,
        COLOR_BAND_DOWN,
        // Semantic tokens (review 4.4): in Classic they are the exact colours these places used
        // before; other themes map them onto their own palette (theme.cpp, SEMANTIC).
        COLOR_TEXT_ON_ACCENT,  // text on accent buttons, band badges, pressed menu items
        COLOR_TEXT_ON_ALERT,  // text on the red "Sleep" button
        COLOR_STATUS_OK,  // connection / status dots
        COLOR_STATUS_ERROR,
        COLOR_STATUS_WARN,
        COLOR_STATUS_BUSY,  // fetching
        COLOR_BG_SUNKEN,  // alternate list rows, message body
        COLOR_BG_DEEP,  // release-notes box
        COLOR_BG_INPUT,  // text fields
        COLOR_BG_BUTTON,  // secondary buttons
        COLOR_SUCCESS,  // "SAVED!" button
        COLOR_TEXT_ON_SUCCESS,
        COLOR_PENDING,  // flashing in progress
        COLOR_INFO  // informational text (splash WiFi status)
    };

    lv_color_t theme_color(ThemeToken token);
    const char* theme_get_name(uint8_t theme_id);

} // namespace ui