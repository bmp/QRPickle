#include "weather.h"
#include "../theme.h"
#include "../fonts.h"
#include "../status_bar.h"
#include "../../hw/sensor.h"
#include <cstdio>

namespace ui {

    static lv_obj_t* tab_view = nullptr;

    void draw_weather_page(lv_obj_t* parent) {
        // FIXED: The weather console now binds directly to the active view_container pipeline
        // preventing rogue overlays from trapping the UI or hiding the global home button.

        tab_view = lv_tabview_create(parent);
        lv_obj_set_size(tab_view, 320, 216);
        lv_obj_align(tab_view, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(tab_view, theme_color(COLOR_BG_APP), 0);

        // Styled tab selector dimensions and color profile matching status bar standard properties
        lv_obj_t* tab_bar = lv_tabview_get_tab_bar(tab_view);
        lv_obj_set_style_bg_color(tab_bar, theme_color(COLOR_BG_BAR), 0);
        lv_obj_set_style_height(tab_bar, 24, 0);
        lv_obj_set_style_pad_top(tab_bar, 0, 0);
        lv_obj_set_style_pad_bottom(tab_bar, 0, 0);
        lv_obj_set_style_text_color(tab_bar, theme_color(COLOR_TEXT_MAIN), 0);
        lv_obj_set_style_text_font(tab_bar, &font_atkinson_14, 0);

        lv_obj_t* t_sensor = lv_tabview_add_tab(tab_view, "Sensor");
        lv_obj_t* t_owm    = lv_tabview_add_tab(tab_view, "OpenWeather");
        lv_obj_t* t_fore   = lv_tabview_add_tab(tab_view, "Forecast");

        char buf[128];
        lv_obj_t* lbl_sens = lv_label_create(t_sensor);
        lv_obj_set_style_text_font(lbl_sens, &font_jetbrains_14, 0);
        lv_obj_set_style_text_color(lbl_sens, theme_color(COLOR_TEXT_MAIN), 0);
        snprintf(buf, sizeof(buf), "Ambient Temp:  %.1f °C\nHumidity:      %.1f %%\nBaro Pressure: %.1f hPa",
                 sensor_get_temp(), sensor_get_humidity(), sensor_get_pressure());
        lv_label_set_text(lbl_sens, buf);
        lv_obj_align(lbl_sens, LV_ALIGN_TOP_LEFT, 10, 15);

        lv_obj_t* lbl_owm = lv_label_create(t_owm);
        lv_obj_set_style_text_font(lbl_owm, &font_jetbrains_14, 0);
        lv_obj_set_style_text_color(lbl_owm, theme_color(COLOR_TEXT_MAIN), 0);
        snprintf(buf, sizeof(buf), "Outdoor Temp:  28.1 °C\nHumidity:      52.0 %%\nBaro Pressure: 1009.8 hPa\nCondition:     Scattered Clouds");
        lv_label_set_text(lbl_owm, buf);
        lv_obj_align(lbl_owm, LV_ALIGN_TOP_LEFT, 10, 15);

        lv_obj_t* lbl_fore = lv_label_create(t_fore);
        lv_obj_set_style_text_font(lbl_fore, &font_jetbrains_14, 0);
        lv_obj_set_style_text_color(lbl_fore, theme_color(COLOR_TEXT_MAIN), 0);
        snprintf(buf, sizeof(buf), "Fri: 29C | Scattered Clouds\nSat: 31C | Clear Skies\nSun: 30C | Light Rain Shower\nMon: 28C | Heavy Overcast");
        lv_label_set_text(lbl_fore, buf);
        lv_obj_align(lbl_fore, LV_ALIGN_TOP_LEFT, 10, 10);
    }

    // Deprecated standalone creator functions no longer utilized by the central routing matrix
    lv_obj_t* weather_create() { return nullptr; }
    void weather_destroy() {}
}
