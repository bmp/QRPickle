#include "widget_wx_all.h"
#include "../theme.h"
#include "../fonts.h"
#include "../ui.h" 
#include "../../hw/sensor.h"
#include "../../config/config.h"
#include <cstdio>
#include <cstring>
#include <Arduino.h>

namespace ui {

    static lv_obj_t* base_card = nullptr;
    static lv_obj_t* area_s = nullptr;    
    static lv_obj_t* area_w = nullptr;    
    static lv_obj_t* img_sensor = nullptr; 
    static lv_obj_t* img_owm = nullptr;    
    static lv_obj_t* lbl_data = nullptr;
    static bool use_owm_source = false;

    static const char* PATH_ICON_SENSOR = "L:/img/icon_sensor_20x20.bin";
    static const char* PATH_ICON_WEB    = "L:/img/icon_web_20x20.bin";

    static void repaint_widget_metrics() {
        if (!lbl_data) return;
        char buf[128];

        lv_color_t c_active = theme_color(COLOR_ACCENT_PRIMARY);
        lv_color_t c_inactive = theme_color(COLOR_TEXT_MUTED);

        if (!use_owm_source) {
            if (img_sensor) {
                lv_obj_set_style_image_recolor(img_sensor, c_active, 0);
                lv_obj_set_style_image_recolor_opa(img_sensor, LV_OPA_COVER, 0); 
            }
            if (img_owm) {
                lv_obj_set_style_image_recolor(img_owm, c_inactive, 0);
                lv_obj_set_style_image_recolor_opa(img_owm, LV_OPA_COVER, 0); 
            }

            if (sensor_get_pressure() > 200.0f) {
                lv_obj_set_style_text_font(lbl_data, &font_jetbrains_14, 0);
                // FIXED: Removed labels, added "humid" suffix, and centered formatting
                snprintf(buf, sizeof(buf), "%.1f °C\n%.0f %% humid\n%.0f hPa", 
                         sensor_get_temp(), sensor_get_humidity(), sensor_get_pressure());
            } else {
                lv_obj_set_style_text_font(lbl_data, &font_atkinson_10, 0);
                snprintf(buf, sizeof(buf), "SENSOR OFFLINE\n\nNo local environmental\ndata detected.");
            }
        } else {
            if (img_owm) {
                lv_obj_set_style_image_recolor(img_owm, c_active, 0);
                lv_obj_set_style_image_recolor_opa(img_owm, LV_OPA_COVER, 0);
            }
            if (img_sensor) {
                lv_obj_set_style_image_recolor(img_sensor, c_inactive, 0);
                lv_obj_set_style_image_recolor_opa(img_sensor, LV_OPA_COVER, 0);
            }

            if (strlen(config::get().openweather_api_key) == 0) {
                lv_obj_set_style_text_font(lbl_data, &font_atkinson_10, 0);
                snprintf(buf, sizeof(buf), "API KEY MISSING\n\nConfigure OpenWeather\ntoken in Web UI.");
            } else {
                lv_obj_set_style_text_font(lbl_data, &font_jetbrains_14, 0);
                // FIXED: Removed labels, added "humid" suffix
                snprintf(buf, sizeof(buf), "28.1 °C\n52 %% humid\n1010 hPa");
            }
        }
        lv_label_set_text(lbl_data, buf);
    }

    static void widget_timer_cb(lv_timer_t* timer) {
        repaint_widget_metrics();
    }

    static void click_handler(lv_event_t* e) {
        lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
        if (target == area_s) {
            use_owm_source = false;
            repaint_widget_metrics();
        } else if (target == area_w) {
            use_owm_source = true;
            repaint_widget_metrics();
        } else {
            ui_navigate_local(PAGE_WEATHER);
        }
    }

    lv_obj_t* widget_wx_all_create(lv_obj_t* parent, uint8_t size_type) {
        base_card = lv_obj_create(parent);
        
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
        lv_obj_set_size(side_bar, 30, 96); 
        lv_obj_align(side_bar, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_style_bg_opa(side_bar, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(side_bar, 0, 0);
        lv_obj_set_style_pad_all(side_bar, 0, 0);
        lv_obj_clear_flag(side_bar, LV_OBJ_FLAG_SCROLLABLE); 
        
        lv_obj_set_flex_flow(side_bar, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(side_bar, 4, 0); 
        lv_obj_set_flex_align(side_bar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        area_s = lv_obj_create(side_bar);
        lv_obj_set_size(area_s, 30, 38);
        lv_obj_set_style_bg_opa(area_s, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(area_s, 0, 0);
        lv_obj_clear_flag(area_s, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(area_s, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(area_s, click_handler, LV_EVENT_CLICKED, nullptr);

        img_sensor = lv_image_create(area_s); 
        lv_image_set_src(img_sensor, PATH_ICON_SENSOR); 
        lv_obj_center(img_sensor);
        lv_obj_add_flag(img_sensor, LV_OBJ_FLAG_EVENT_BUBBLE); 

        area_w = lv_obj_create(side_bar);
        lv_obj_set_size(area_w, 30, 38);
        lv_obj_set_style_bg_opa(area_w, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(area_w, 0, 0);
        lv_obj_clear_flag(area_w, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(area_w, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(area_w, click_handler, LV_EVENT_CLICKED, nullptr);

        img_owm = lv_image_create(area_w); 
        lv_image_set_src(img_owm, PATH_ICON_WEB); 
        lv_obj_center(img_owm);
        lv_obj_add_flag(img_owm, LV_OBJ_FLAG_EVENT_BUBBLE); 

        lbl_data = lv_label_create(base_card);
        lv_obj_set_style_text_color(lbl_data, theme_color(COLOR_TEXT_MAIN), 0);
        // FIXED: Center the text inside a 120px width block, aligned vertically and to the right
        lv_obj_set_width(lbl_data, 120);
        lv_obj_set_style_text_align(lbl_data, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_line_space(lbl_data, 8, 0); // Breathes better without labels
        lv_obj_align(lbl_data, LV_ALIGN_RIGHT_MID, 0, 0); 

        repaint_widget_metrics();

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