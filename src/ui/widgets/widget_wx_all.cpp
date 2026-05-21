#include "widget_wx_all.h"
#include "../theme.h"
#include "../fonts.h"
#include "../ui.h"
#include "../../hw/sensor.h"
#include <cstdio>

namespace ui {

    static lv_obj_t* base_card = nullptr;
    static lv_obj_t* btn_sensor = nullptr;
    static lv_obj_t* btn_owm = nullptr;
    static lv_obj_t* lbl_data = nullptr;
    static bool use_owm_source = false;

    static void repaint_widget_metrics() {
        if (!lbl_data) return;
        char buf[128];

        if (!use_owm_source) {
            snprintf(buf, sizeof(buf), "Temp:  %.1f °C\nHumid: %.0f %%\nBaro:  %.0f hPa",
                     sensor_get_temp(), sensor_get_humidity(), sensor_get_pressure());
            lv_obj_set_style_text_color(btn_sensor, theme_color(COLOR_ACCENT_PRIMARY), 0);
            lv_obj_set_style_text_color(btn_owm, theme_color(COLOR_TEXT_MUTED), 0);
        } else {
            snprintf(buf, sizeof(buf), "Temp:  28.1 °C\nHumid: 52 %%\nBaro:  1010 hPa");
            lv_obj_set_style_text_color(btn_owm, theme_color(COLOR_ACCENT_PRIMARY), 0);
            lv_obj_set_style_text_color(btn_sensor, theme_color(COLOR_TEXT_MUTED), 0);
        }
        lv_label_set_text(lbl_data, buf);
    }

    // FIXED: Asynchronous telemetry poller replicates your old widget's automatic updates
    static void widget_timer_cb(lv_timer_t* timer) {
        repaint_widget_metrics();
    }

    static void click_handler(lv_event_t* e) {
        lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
        if (target == btn_sensor) {
            use_owm_source = false;
            repaint_widget_metrics();
        } else if (target == btn_owm) {
            use_owm_source = true;
            repaint_widget_metrics();
        } else {
            ui_navigate_local(PAGE_WEATHER);
        }
    }

    lv_obj_t* widget_wx_all_create(lv_obj_t* parent, uint8_t size_type) {
        base_card = lv_obj_create(parent);

        // FIXED: Maps exactly to the 154x104 footprint of your original component
        lv_obj_set_size(base_card, 154, 104);
        lv_obj_set_style_bg_color(base_card, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_color(base_card, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(base_card, 1, 0);
        lv_obj_set_style_radius(base_card, 4, 0);
        lv_obj_set_style_pad_all(base_card, 2, 0);
        lv_obj_clear_flag(base_card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_add_flag(base_card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(base_card, click_handler, LV_EVENT_CLICKED, nullptr);

        lv_obj_t* side_bar = lv_obj_create(base_card);
        lv_obj_set_size(side_bar, 24, 96);
        lv_obj_align(side_bar, LV_ALIGN_LEFT_MID, 2, 0);
        lv_obj_set_style_bg_opa(side_bar, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(side_bar, 0, 0);
        lv_obj_set_style_pad_all(side_bar, 0, 0);
        lv_obj_set_flex_flow(side_bar, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(side_bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        btn_sensor = lv_label_create(side_bar);
        lv_obj_set_style_text_font(btn_sensor, &font_atkinson_14, 0);
        lv_label_set_text(btn_sensor, "S");
        lv_obj_add_flag(btn_sensor, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(btn_sensor, click_handler, LV_EVENT_CLICKED, nullptr);

        btn_owm = lv_label_create(side_bar);
        lv_obj_set_style_text_font(btn_owm, &font_atkinson_14, 0);
        lv_label_set_text(btn_owm, "W");
        lv_obj_add_flag(btn_owm, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(btn_owm, click_handler, LV_EVENT_CLICKED, nullptr);

        lbl_data = lv_label_create(base_card);
        lv_obj_set_style_text_font(lbl_data, &font_jetbrains_14, 0);
        lv_obj_set_style_text_color(lbl_data, theme_color(COLOR_TEXT_MAIN), 0);
        lv_obj_set_style_text_line_space(lbl_data, 5, 0);
        lv_obj_align(lbl_data, LV_ALIGN_LEFT_MID, 32, 0); // Pushed right slightly to account for the wider 154px container

        repaint_widget_metrics();

        // FIXED: Spawns the background 2-second background refresh timer block
        lv_timer_t* timer = lv_timer_create(widget_timer_cb, 2000, base_card);
        lv_obj_add_event_cb(base_card, [](lv_event_t* e) {
            lv_timer_t* t = (lv_timer_t*)lv_event_get_user_data(e);
            if (t) lv_timer_delete(t);
        }, LV_EVENT_DELETE, timer);

            return base_card;
    }

    void widget_wx_all_refresh() {
        if (base_card) {
            lv_obj_set_style_bg_color(base_card, theme_color(COLOR_BG_PANEL), 0);
            lv_obj_set_style_border_color(base_card, theme_color(COLOR_BORDER), 0);
            repaint_widget_metrics();
        }
    }
}
