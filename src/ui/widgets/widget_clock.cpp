#include "widget_clock.h"
#include "../fonts.h"
#include "../theme.h" // <-- ADD THIS
#include "../../core/timekeeper.h"
#include <string.h>

namespace ui {

    static void clock_timer_cb(lv_timer_t* timer) {
        if (!timer) return;

        lv_obj_t* widget = (lv_obj_t*)lv_timer_get_user_data(timer);
        if (!widget) return;

        lv_obj_t* loc_lbl = lv_obj_get_child(widget, 0);
        lv_obj_t* utc_lbl = lv_obj_get_child(widget, 1);

        const char* raw_local = timekeeper_get_local_string();
        const char* raw_utc = timekeeper_get_utc_string();

        if (loc_lbl && raw_local != nullptr && strlen(raw_local) >= 5) {
            char buf_loc[6];
            strncpy(buf_loc, raw_local, 5);
            buf_loc[5] = '\0';
            lv_label_set_text(loc_lbl, buf_loc);
        }

        if (utc_lbl && raw_utc != nullptr && strlen(raw_utc) >= 5) {
            char buf_utc[12];
            strncpy(buf_utc, raw_utc, 5);
            buf_utc[5] = '\0';
            strcat(buf_utc, " Z");
            lv_label_set_text(utc_lbl, buf_utc);
        }
    }

    lv_obj_t* widget_clock_create(lv_obj_t* parent, WidgetSize size) {
        lv_obj_t* widget = lv_obj_create(parent);
        lv_obj_clear_flag(widget, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_set_style_bg_color(widget, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_color(widget, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(widget, 1, 0);
        lv_obj_set_style_radius(widget, 6, 0);
        lv_obj_set_style_pad_all(widget, 0, 0);

        lv_obj_t* loc_lbl = lv_label_create(widget);
        lv_obj_t* utc_lbl = lv_label_create(widget);

        if (size == WIDGET_SIZE_QUARTER) {
            lv_obj_set_size(widget, 154, 104);

            lv_obj_set_style_text_font(loc_lbl, &font_jetbrains_24, 0);
            lv_obj_set_style_text_color(loc_lbl, theme_color(COLOR_ACCENT_PRIMARY), 0);
            lv_label_set_text(loc_lbl, "--:--");
            lv_obj_align(loc_lbl, LV_ALIGN_TOP_MID, 0, 16);

            lv_obj_set_style_text_font(utc_lbl, &font_jetbrains_14, 0);
            lv_obj_set_style_text_color(utc_lbl, theme_color(COLOR_TEXT_MUTED), 0);
            lv_label_set_text(utc_lbl, "--:-- Z");
            lv_obj_align(utc_lbl, LV_ALIGN_BOTTOM_MID, 0, -16);
        }

        lv_timer_t* timer = lv_timer_create(clock_timer_cb, 1000, widget);
        clock_timer_cb(timer);

        lv_obj_add_event_cb(widget, [](lv_event_t* e) {
            lv_timer_t* t = (lv_timer_t*)lv_event_get_user_data(e);
            if (t) lv_timer_delete(t);
        }, LV_EVENT_DELETE, timer);

            return widget;
    }

} // namespace ui
