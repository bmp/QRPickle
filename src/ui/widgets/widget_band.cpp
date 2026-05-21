#include "widget_band.h"
#include "../fonts.h"
#include "../theme.h" // <-- ADD THIS

namespace ui {

    lv_obj_t* widget_band_create(lv_obj_t* parent, WidgetSize size) {
        lv_obj_t* widget = lv_obj_create(parent);
        lv_obj_clear_flag(widget, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_set_style_bg_color(widget, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_color(widget, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(widget, 1, 0);
        lv_obj_set_style_radius(widget, 6, 0);
        lv_obj_set_style_pad_all(widget, 10, 0);
        lv_obj_set_flex_flow(widget, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(widget, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

        if (size == WIDGET_SIZE_HALF_VERT) {
            lv_obj_set_size(widget, 154, 212);

            const char* bands[] = {"10m: GOOD", "20m: FAIR", "40m: POOR", "80m: DOWN"};
            lv_color_t colors[] = {
                theme_color(COLOR_BAND_GOOD),
                theme_color(COLOR_BAND_FAIR),
                theme_color(COLOR_BAND_POOR),
                theme_color(COLOR_BAND_DOWN)
            };

            for (int i = 0; i < 4; i++) {
                lv_obj_t* row = lv_label_create(widget);
                lv_label_set_text(row, bands[i]);
                lv_obj_set_style_text_font(row, &font_jetbrains_14, 0);
                lv_obj_set_style_text_color(row, colors[i], 0);
            }
        }

        return widget;
    }

} // namespace ui
