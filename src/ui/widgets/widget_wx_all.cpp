#include "widget_wx_all.h"
#include "../theme.h"
#include "../fonts.h"
#include "../ui.h" 
#include "../../hw/sensor.h"
#include "../../config/config.h"
#include "../../services/weather_manager.h" 
#include <cstdio>
#include <cstring>
#include <Arduino.h>
#include <LittleFS.h> // FIXED: Included to read files directly to RAM

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

    // --- High-Speed RAM Caching Engine ---
    static lv_image_dsc_t dsc_sensor = {0};
    static lv_image_dsc_t dsc_web = {0};
    static bool icons_in_ram = false;

    static void cache_bin_to_ram(const char* path, lv_image_dsc_t* dsc) {
        if (dsc->data != nullptr) return; // Already cached

        // Strip "L:" prefix so standard LittleFS can find the path (e.g., L:/img/... -> /img/...)
        const char* native_path = path;
        if (strncmp(path, "L:", 2) == 0) native_path += 2;

        File f = LittleFS.open(native_path, "r");
        if (!f) return;

        size_t sz = f.size();
        if (sz <= 12) { f.close(); return; }

        // Allocate a permanent block of RAM to hold the icon
        uint8_t* raw_buf = (uint8_t*)malloc(sz);
        if (!raw_buf) { f.close(); return; }

        f.read(raw_buf, sz);
        f.close();

        // Trick LVGL by mapping the binary header directly into the native C-struct
        memcpy(&dsc->header, raw_buf, 12);
        dsc->data_size = sz - 12;
        dsc->data = raw_buf + 12; // Point directly to the payload
    }
    // -------------------------------------

    static void repaint_widget_metrics() {
        if (!lbl_data) return;
        char buf[128];

        // Instantly assign the RAM-backed struct instead of the slow LittleFS path
        if (dsc_sensor.data && lv_image_get_src(img_sensor) != &dsc_sensor) {
            lv_image_set_src(img_sensor, &dsc_sensor);
        }
        if (dsc_web.data && lv_image_get_src(img_owm) != &dsc_web) {
            lv_image_set_src(img_owm, &dsc_web);
        }

        if (!use_owm_source) {
            if (dsc_sensor.data) lv_obj_set_style_image_opa(img_sensor, LV_OPA_COVER, 0); 
            if (dsc_web.data)    lv_obj_set_style_image_opa(img_owm, LV_OPA_30, 0); 

            float p = sensor_get_pressure();
            if (p > 200.0f) {
                float t = sensor_get_temp();
                float h = sensor_get_humidity();
                lv_obj_set_style_text_font(lbl_data, &font_jetbrains_14, 0);
                snprintf(buf, sizeof(buf), "%.1f °C\n%.0f %%\n%.0f hPa", t, h, p);
            } else {
                lv_obj_set_style_text_font(lbl_data, &font_atkinson_10, 0);
                snprintf(buf, sizeof(buf), "SENSOR OFFLINE\n\nNo local environmental\ndata detected.");
            }
        } else {
            if (dsc_web.data)    lv_obj_set_style_image_opa(img_owm, LV_OPA_COVER, 0);
            if (dsc_sensor.data) lv_obj_set_style_image_opa(img_sensor, LV_OPA_30, 0);

            if (strlen(config::get().openweather_api_key) == 0) {
                lv_obj_set_style_text_font(lbl_data, &font_atkinson_10, 0);
                snprintf(buf, sizeof(buf), "API KEY MISSING\n\nConfigure OpenWeather\ntoken in Web UI.");
            } else {
                lv_obj_set_style_text_font(lbl_data, &font_jetbrains_14, 0);
                
                const auto& cur = services::weather_manager::get_current();
                if (cur.valid) {
                    snprintf(buf, sizeof(buf), "%.1f °C\n%d %%\n%.0f hPa", cur.temp, cur.humidity, cur.pressure);
                } else {
                    snprintf(buf, sizeof(buf), "-- °C\n-- %\n-- hPa");
                }
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
        // FIXED: Cache the icons immediately to RAM on the very first load
        if (!icons_in_ram) {
            cache_bin_to_ram(PATH_ICON_SENSOR, &dsc_sensor);
            cache_bin_to_ram(PATH_ICON_WEB, &dsc_web);
            icons_in_ram = true;
        }

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
        lv_obj_center(img_owm);
        lv_obj_add_flag(img_owm, LV_OBJ_FLAG_EVENT_BUBBLE); 

        lbl_data = lv_label_create(base_card);
        lv_obj_set_style_text_color(lbl_data, theme_color(COLOR_TEXT_MAIN), 0);
        lv_obj_set_width(lbl_data, 120);
        lv_obj_set_style_text_align(lbl_data, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_line_space(lbl_data, 8, 0); 
        lv_obj_align(lbl_data, LV_ALIGN_RIGHT_MID, 0, 0); 

        // FIXED: Since everything is safely in RAM, we paint instantly with zero lag!
        repaint_widget_metrics();

        // FIXED: Revert to standard 2-second polling interval. No delay needed.
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