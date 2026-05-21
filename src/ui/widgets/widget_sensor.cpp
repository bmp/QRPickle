#include "widget_sensor.h"
#include "../fonts.h"
#include "../theme.h" // <-- ADD THIS
#include "../../hw/sensor.h"
#include <stdio.h>

namespace ui {

    static void sensor_timer_cb(lv_timer_t* timer) {
        if (!timer) return;

        lv_obj_t* widget = (lv_obj_t*)lv_timer_get_user_data(timer);
        if (!widget) return;

        lv_obj_t* lbl_th = lv_obj_get_child(widget, 0);
        lv_obj_t* lbl_pres = lv_obj_get_child(widget, 1);

        if (lbl_th) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.1f C  |  %.1f%%", sensor_get_temp(), sensor_get_humidity());
            lv_label_set_text(lbl_th, buf);
        }

        if (lbl_pres) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.1f hPa", sensor_get_pressure());
            lv_label_set_text(lbl_pres, buf);
        }
    }

    lv_obj_t* widget_sensor_create(lv_obj_t* parent, WidgetSize size) {
        lv_obj_t* widget = lv_obj_create(parent);
        lv_obj_clear_flag(widget, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_set_style_bg_color(widget, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_color(widget, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(widget, 1, 0);
        lv_obj_set_style_radius(widget, 6, 0);
        lv_obj_set_style_pad_all(widget, 0, 0);

        lv_obj_t* lbl_th = lv_label_create(widget);
        lv_obj_t* lbl_pres = lv_label_create(widget);

        if (size == WIDGET_SIZE_QUARTER) {
            lv_obj_set_size(widget, 154, 104);

            lv_obj_set_style_text_font(lbl_th, &font_jetbrains_14, 0);
            lv_obj_set_style_text_color(lbl_th, theme_color(COLOR_SENSOR_TEMP), 0);
            lv_label_set_text(lbl_th, "--.- C  |  --.-%");
            lv_obj_align(lbl_th, LV_ALIGN_TOP_MID, 0, 22);

            lv_obj_set_style_text_font(lbl_pres, &font_jetbrains_14, 0);
            lv_obj_set_style_text_color(lbl_pres, theme_color(COLOR_SENSOR_PRES), 0);
            lv_label_set_text(lbl_pres, "----.- hPa");
            lv_obj_align(lbl_pres, LV_ALIGN_BOTTOM_MID, 0, -22);
        }

        lv_timer_t* timer = lv_timer_create(sensor_timer_cb, 2000, widget);
        sensor_timer_cb(timer);

        lv_obj_add_event_cb(widget, [](lv_event_t* e) {
            lv_timer_t* t = (lv_timer_t*)lv_event_get_user_data(e);
            if (t) lv_timer_delete(t);
        }, LV_EVENT_DELETE, timer);

            return widget;
    }

} // namespace ui
