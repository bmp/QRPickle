#include "weather.h"
#include "../status_bar.h"
#include "../theme.h"
#include "../fonts.h"
#include "../../hw/sensor.h"
#include "../../services/weather_manager.h"
#include "../../config/config.h"
#include <lvgl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

namespace ui {

    static lv_obj_t* scr = nullptr;
    static lv_obj_t* tabview = nullptr;
    static lv_timer_t* wx_timer = nullptr;

    void weather_update_data(); 

    static const char* deg_to_cardinal(uint16_t deg) {
        const char* directions[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
        return directions[((deg + 11) % 360) / 22];
    }

    static void format_local_time(uint32_t utc_ts, char* buf, size_t sz) {
        if (utc_ts == 0) { snprintf(buf, sz, "--:--"); return; }
        time_t local_ts = utc_ts + (config::get().tz_offset_hh * 1800);
        struct tm* tm_info = gmtime(&local_ts); 
        if (tm_info) snprintf(buf, sz, "%02d:%02d", tm_info->tm_hour, tm_info->tm_min);
        else snprintf(buf, sz, "--:--");
    }

    // --- Core Grid Card Generator ---
    // FIXED: Added font parameter so we can shrink the text for tighter boxes
    static lv_obj_t* create_grid_card(lv_obj_t* parent, const char* title, lv_obj_t** out_val, const lv_font_t* val_font, lv_obj_t** out_unit = nullptr) {
        lv_obj_t* card = lv_obj_create(parent);
        lv_obj_set_style_bg_color(card, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_color(card, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_radius(card, 4, 0);
        lv_obj_set_style_pad_all(card, 4, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl_title = lv_label_create(card);
        lv_label_set_text(lbl_title, title);
        lv_obj_set_style_text_font(lbl_title, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_title, theme_color(COLOR_TEXT_MUTED), 0);
        lv_obj_align(lbl_title, LV_ALIGN_TOP_LEFT, 2, 2);

        lv_obj_t* lbl_val = lv_label_create(card);
        lv_label_set_text(lbl_val, "--");
        lv_obj_set_style_text_font(lbl_val, val_font, 0);
        lv_obj_set_style_text_color(lbl_val, theme_color(COLOR_TEXT_MAIN), 0);
        
        // Slightly offset center if a unit is attached
        if (out_unit) {
            lv_obj_align(lbl_val, LV_ALIGN_BOTTOM_MID, -10, -2);
        } else {
            lv_obj_align(lbl_val, LV_ALIGN_BOTTOM_MID, 0, -2);
        }

        if (out_unit) {
            lv_obj_t* lbl_u = lv_label_create(card);
            lv_label_set_text(lbl_u, "");
            lv_obj_set_style_text_font(lbl_u, &font_jetbrains_14, 0); // Always use 14pt for units (contains degree symbol)
            lv_obj_set_style_text_color(lbl_u, theme_color(COLOR_TEXT_MUTED), 0);
            
            // If the font is large (24pt), attach superscript style, otherwise inline it
            if (val_font == &font_jetbrains_24) {
                lv_obj_align_to(lbl_u, lbl_val, LV_ALIGN_OUT_RIGHT_TOP, 2, 2);
            } else {
                lv_obj_align_to(lbl_u, lbl_val, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, 0);
            }
            *out_unit = lbl_u;
        }

        if (out_val) *out_val = lbl_val;
        return card;
    }

    // --- Tab 1 Layout (1B - Grid) ---
    static lv_obj_t* lbl_loc_temp = nullptr; static lv_obj_t* lbl_unit_temp = nullptr;
    static lv_obj_t* lbl_loc_hum  = nullptr; static lv_obj_t* lbl_unit_hum  = nullptr;
    static lv_obj_t* lbl_loc_pres = nullptr; static lv_obj_t* lbl_unit_pres = nullptr;
    static lv_obj_t* lbl_loc_dew  = nullptr; static lv_obj_t* lbl_unit_dew  = nullptr;
    static lv_obj_t* lbl_loc_alt  = nullptr; 

    static void build_tab_sensors(lv_obj_t* parent) {
        lv_obj_set_style_pad_all(parent, 4, 0);

        int card_w = (320 - 12) / 2;
        int card_h = 76; 

        lv_obj_t* c1 = create_grid_card(parent, "TEMPERATURE", &lbl_loc_temp, &font_jetbrains_24, &lbl_unit_temp);
        lv_obj_set_size(c1, card_w, card_h); lv_obj_align(c1, LV_ALIGN_TOP_LEFT, 2, 0);

        lv_obj_t* c2 = create_grid_card(parent, "HUMIDITY", &lbl_loc_hum, &font_jetbrains_24, &lbl_unit_hum);
        lv_obj_set_size(c2, card_w, card_h); lv_obj_align(c2, LV_ALIGN_TOP_RIGHT, -2, 0);

        lv_obj_t* c3 = create_grid_card(parent, "PRESSURE", &lbl_loc_pres, &font_jetbrains_24, &lbl_unit_pres);
        lv_obj_set_size(c3, card_w, card_h); lv_obj_align(c3, LV_ALIGN_TOP_LEFT, 2, 80);

        lv_obj_t* c4 = create_grid_card(parent, "DEW POINT", &lbl_loc_dew, &font_jetbrains_24, &lbl_unit_dew);
        lv_obj_set_size(c4, card_w, card_h); lv_obj_align(c4, LV_ALIGN_TOP_RIGHT, -2, 80);

        lv_obj_t* alt_bar = lv_obj_create(parent);
        lv_obj_set_size(alt_bar, 312, 24);
        lv_obj_align(alt_bar, LV_ALIGN_BOTTOM_MID, 0, -2);
        lv_obj_set_style_bg_color(alt_bar, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_color(alt_bar, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(alt_bar, 1, 0);
        lv_obj_clear_flag(alt_bar, LV_OBJ_FLAG_SCROLLABLE);

        lbl_loc_alt = lv_label_create(alt_bar);
        lv_label_set_text(lbl_loc_alt, "EST. ALTITUDE: -- m / -- ft"); 
        lv_obj_set_style_text_font(lbl_loc_alt, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_loc_alt, theme_color(COLOR_TEXT_MAIN), 0);
        lv_obj_center(lbl_loc_alt);
    }

    // --- Tab 2 Layout (2B - Matrix) ---
    static lv_obj_t* img_cur_icon = nullptr;
    static lv_obj_t* lbl_cur_temp = nullptr; static lv_obj_t* lbl_cur_tunit = nullptr;
    static lv_obj_t* lbl_cur_desc = nullptr;
    static lv_obj_t* lbl_cur_loc  = nullptr;
    
    static lv_obj_t* lbl_cur_sr = nullptr;   static lv_obj_t* lbl_cur_ss = nullptr;
    static lv_obj_t* lbl_cur_wind = nullptr; static lv_obj_t* lbl_cur_cloud = nullptr;
    static lv_obj_t* lbl_cur_vis = nullptr;  static lv_obj_t* lbl_cur_pres = nullptr;

    static void build_tab_current(lv_obj_t* parent) {
        lv_obj_set_style_pad_all(parent, 2, 0);

        lv_obj_t* top_deck = lv_obj_create(parent);
        lv_obj_set_size(top_deck, lv_pct(100), 50); 
        lv_obj_align(top_deck, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(top_deck, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_width(top_deck, 0, 0);
        lv_obj_clear_flag(top_deck, LV_OBJ_FLAG_SCROLLABLE);

        // FIXED: Icon and Temp moved further to the right
        img_cur_icon = lv_image_create(top_deck);
        lv_obj_set_size(img_cur_icon, 40, 40);
        lv_obj_align(img_cur_icon, LV_ALIGN_LEFT_MID, 25, 0); 

        lbl_cur_temp = lv_label_create(top_deck);
        lv_obj_set_style_text_font(lbl_cur_temp, &font_jetbrains_24, 0);
        lv_obj_set_style_text_color(lbl_cur_temp, theme_color(COLOR_TEXT_MAIN), 0); 
        lv_label_set_text(lbl_cur_temp, "--");
        lv_obj_align(lbl_cur_temp, LV_ALIGN_LEFT_MID, 75, 0); 

        // FIXED: Superscript degree utilizing the robust 14pt font
        lbl_cur_tunit = lv_label_create(top_deck);
        lv_label_set_text(lbl_cur_tunit, "°C");
        lv_obj_set_style_text_font(lbl_cur_tunit, &font_jetbrains_14, 0);
        lv_obj_set_style_text_color(lbl_cur_tunit, theme_color(COLOR_TEXT_MAIN), 0);
        lv_obj_align_to(lbl_cur_tunit, lbl_cur_temp, LV_ALIGN_OUT_RIGHT_TOP, 2, 2);

        lv_obj_t* text_stack = lv_obj_create(top_deck);
        lv_obj_set_size(text_stack, 180, 46);
        lv_obj_align(text_stack, LV_ALIGN_RIGHT_MID, 0, 0); 
        lv_obj_set_style_bg_opa(text_stack, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(text_stack, 0, 0);
        lv_obj_set_style_pad_all(text_stack, 0, 0);
        lv_obj_set_flex_flow(text_stack, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(text_stack, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END); 
        lv_obj_clear_flag(text_stack, LV_OBJ_FLAG_SCROLLABLE);

        lbl_cur_desc = lv_label_create(text_stack);
        lv_obj_set_style_text_font(lbl_cur_desc, &font_atkinson_14, 0);
        lv_obj_set_style_text_color(lbl_cur_desc, theme_color(COLOR_ACCENT_PRIMARY), 0);
        lv_label_set_text(lbl_cur_desc, "Connecting...");

        lbl_cur_loc = lv_label_create(text_stack);
        lv_obj_set_style_text_font(lbl_cur_loc, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_cur_loc, theme_color(COLOR_TEXT_MUTED), 0);
        lv_label_set_text(lbl_cur_loc, "---");

        int q_w = (320 - 12) / 2; int q_h = 42;
        
        // FIXED: Grid Values now use 14pt so they fit beautifully inside the boxes
        lv_obj_t* q1 = create_grid_card(parent, "SUNRISE", &lbl_cur_sr, &font_jetbrains_14);
        lv_obj_set_size(q1, q_w, q_h); lv_obj_align(q1, LV_ALIGN_TOP_LEFT, 2, 54);

        lv_obj_t* q2 = create_grid_card(parent, "SUNSET", &lbl_cur_ss, &font_jetbrains_14);
        lv_obj_set_size(q2, q_w, q_h); lv_obj_align(q2, LV_ALIGN_TOP_RIGHT, -2, 54);

        lv_obj_t* q3 = create_grid_card(parent, "WIND", &lbl_cur_wind, &font_jetbrains_14); 
        lv_obj_set_size(q3, q_w, q_h); lv_obj_align(q3, LV_ALIGN_TOP_LEFT, 2, 100);

        lv_obj_t* q4 = create_grid_card(parent, "CLOUD COVER", &lbl_cur_cloud, &font_jetbrains_14);
        lv_obj_set_size(q4, q_w, q_h); lv_obj_align(q4, LV_ALIGN_TOP_RIGHT, -2, 100);

        lv_obj_t* q5 = create_grid_card(parent, "VISIBILITY", &lbl_cur_vis, &font_jetbrains_14);
        lv_obj_set_size(q5, q_w, q_h); lv_obj_align(q5, LV_ALIGN_TOP_LEFT, 2, 146);

        lv_obj_t* q6 = create_grid_card(parent, "PRESSURE", &lbl_cur_pres, &font_jetbrains_14);
        lv_obj_set_size(q6, q_w, q_h); lv_obj_align(q6, LV_ALIGN_TOP_RIGHT, -2, 146);
    }

    // --- Tab 3 Layout (3A - Fast Zebra List) ---
    static lv_obj_t* forecast_scroll_container = nullptr;
    
    struct DynForecastRow {
        lv_obj_t* base_row = nullptr;
        lv_obj_t* lbl_time;
        lv_obj_t* lbl_temp;
        lv_obj_t* lbl_rain;
        lv_obj_t* lbl_wind;
    };
    static DynForecastRow dyn_rows[8];

    static void build_tab_forecast(lv_obj_t* parent) {
        lv_obj_set_style_pad_all(parent, 2, 0);

        forecast_scroll_container = lv_obj_create(parent);
        lv_obj_set_size(forecast_scroll_container, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_opa(forecast_scroll_container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(forecast_scroll_container, 0, 0);
        lv_obj_set_style_pad_all(forecast_scroll_container, 2, 0);
        lv_obj_set_flex_flow(forecast_scroll_container, LV_FLEX_FLOW_COLUMN); 
        lv_obj_set_style_pad_row(forecast_scroll_container, 4, 0); 
        lv_obj_add_flag(forecast_scroll_container, LV_OBJ_FLAG_SCROLLABLE);

        for (int i = 0; i < 8; i++) {
            lv_obj_t* row = lv_obj_create(forecast_scroll_container);
            lv_obj_set_size(row, lv_pct(100), 34); 
            lv_obj_set_style_bg_color(row, i % 2 == 0 ? theme_color(COLOR_BG_PANEL) : lv_color_hex(0x050505), 0);
            lv_obj_set_style_border_color(row, theme_color(COLOR_BORDER), 0);
            lv_obj_set_style_border_width(row, 1, 0);
            lv_obj_set_style_radius(row, 6, 0); 
            lv_obj_set_style_pad_all(row, 0, 0);
            lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
            dyn_rows[i].base_row = row;

            dyn_rows[i].lbl_time = lv_label_create(row);
            lv_obj_set_style_text_font(dyn_rows[i].lbl_time, &font_jetbrains_14, 0);
            lv_obj_set_style_text_color(dyn_rows[i].lbl_time, theme_color(COLOR_ACCENT_PRIMARY), 0);
            lv_obj_align(dyn_rows[i].lbl_time, LV_ALIGN_LEFT_MID, 10, 0);

            dyn_rows[i].lbl_temp = lv_label_create(row);
            lv_obj_set_style_text_font(dyn_rows[i].lbl_temp, &font_jetbrains_14, 0);
            lv_obj_align(dyn_rows[i].lbl_temp, LV_ALIGN_LEFT_MID, 75, 0); // Temp position

            dyn_rows[i].lbl_rain = lv_label_create(row);
            lv_obj_set_style_text_font(dyn_rows[i].lbl_rain, &font_jetbrains_10, 0);
            lv_obj_set_style_text_color(dyn_rows[i].lbl_rain, theme_color(COLOR_TEXT_MUTED), 0);
            lv_obj_align(dyn_rows[i].lbl_rain, LV_ALIGN_LEFT_MID, 135, 0); // FIXED: Shifted Left

            dyn_rows[i].lbl_wind = lv_label_create(row);
            lv_obj_set_style_text_font(dyn_rows[i].lbl_wind, &font_jetbrains_10, 0);
            lv_obj_set_style_text_color(dyn_rows[i].lbl_wind, theme_color(COLOR_TEXT_MUTED), 0);
            lv_obj_align(dyn_rows[i].lbl_wind, LV_ALIGN_LEFT_MID, 215, 0); // FIXED: Shifted Left & anchored left
        }
    }

    void draw_weather_page(lv_obj_t* parent) {
        scr = lv_obj_create(parent);
        lv_obj_set_size(scr, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_color(scr, theme_color(COLOR_BG_APP), 0);
        lv_obj_set_style_border_width(scr, 0, 0);
        lv_obj_set_style_pad_all(scr, 0, 0);
        lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

        tabview = lv_tabview_create(scr);
        lv_tabview_set_tab_bar_position(tabview, LV_DIR_TOP);
        lv_tabview_set_tab_bar_size(tabview, 24);

        // FIXED: Force all tabview backgrounds to match the theme color seamlessly
        lv_obj_set_style_bg_color(tabview, theme_color(COLOR_BG_APP), 0);
        lv_obj_t* tab_cont = lv_tabview_get_content(tabview);
        if (tab_cont) lv_obj_set_style_bg_color(tab_cont, theme_color(COLOR_BG_APP), 0);

        lv_obj_t* tab_btns = lv_tabview_get_tab_bar(tabview);
        lv_obj_set_style_bg_color(tab_btns, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_text_font(tab_btns, &font_atkinson_14, 0);
        lv_obj_set_style_text_color(tab_btns, theme_color(COLOR_TEXT_MUTED), 0);
        lv_obj_set_style_text_color(tab_btns, theme_color(COLOR_ACCENT_PRIMARY), LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_border_color(tab_btns, theme_color(COLOR_ACCENT_PRIMARY), LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_border_width(tab_btns, 2, LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_border_side(tab_btns, LV_BORDER_SIDE_BOTTOM, LV_PART_ITEMS | LV_STATE_CHECKED);

        lv_obj_t* t1 = lv_tabview_add_tab(tabview, "LOCAL");
        lv_obj_t* t2 = lv_tabview_add_tab(tabview, "OPENWEATHER");
        lv_obj_t* t3 = lv_tabview_add_tab(tabview, "FORECAST");
        
        // Force the page containers to also be seamless
        lv_obj_set_style_bg_color(t1, theme_color(COLOR_BG_APP), 0);
        lv_obj_set_style_bg_color(t2, theme_color(COLOR_BG_APP), 0);
        lv_obj_set_style_bg_color(t3, theme_color(COLOR_BG_APP), 0);

        build_tab_sensors(t1);
        build_tab_current(t2);
        build_tab_forecast(t3);

        lv_obj_add_event_cb(scr, [](lv_event_t* e) {
            if (wx_timer) { lv_timer_delete(wx_timer); wx_timer = nullptr; }
            scr = nullptr; forecast_scroll_container = nullptr;
            lbl_loc_temp = lbl_unit_temp = nullptr;
            lbl_loc_hum = lbl_unit_hum = nullptr;
            lbl_loc_pres = lbl_unit_pres = nullptr;
            lbl_loc_dew = lbl_unit_dew = lbl_loc_alt = nullptr;
            img_cur_icon = lbl_cur_temp = lbl_cur_tunit = lbl_cur_desc = lbl_cur_loc = nullptr;
            lbl_cur_sr = lbl_cur_ss = lbl_cur_wind = lbl_cur_cloud = lbl_cur_vis = lbl_cur_pres = nullptr;
            for(int i=0; i<8; i++) dyn_rows[i].base_row = nullptr;
        }, LV_EVENT_DELETE, NULL);

        wx_timer = lv_timer_create([](lv_timer_t* t){ weather_update_data(); }, 1500, NULL);
        weather_update_data(); 
    }

    void weather_update_data() {
        if (!scr) return;
        char buf[64];

        // 1. Tab 1: Local Calculations
        float t = sensor_get_temp(); float h = sensor_get_humidity(); float p = sensor_get_pressure();

        if (lbl_loc_temp) { snprintf(buf, sizeof(buf), "%.1f", t); lv_label_set_text(lbl_loc_temp, buf); lv_label_set_text(lbl_unit_temp, "°C"); }
        if (lbl_loc_hum)  { snprintf(buf, sizeof(buf), "%.0f", h);  lv_label_set_text(lbl_loc_hum, buf);  lv_label_set_text(lbl_unit_hum, "%"); }
        if (lbl_loc_pres) { snprintf(buf, sizeof(buf), "%.0f", p);  lv_label_set_text(lbl_loc_pres, buf); lv_label_set_text(lbl_unit_pres, "hPa"); }
        if (lbl_loc_dew)  { snprintf(buf, sizeof(buf), "%.1f", t - ((100.0f - h) / 5.0f)); lv_label_set_text(lbl_loc_dew, buf); lv_label_set_text(lbl_unit_dew, "°C"); }
        
        if (lbl_loc_alt && p > 100.0f) {
            float alt_m = 44330.0f * (1.0f - powf((p / 1013.25f), 0.1903f));
            float alt_ft = alt_m * 3.28084f;
            snprintf(buf, sizeof(buf), "EST. ALTITUDE: %.0f m / %.0f ft", alt_m, alt_ft);
            lv_label_set_text(lbl_loc_alt, buf);
        }

        // 2. Tab 2: Live Regional 
        const auto& cur = services::weather_manager::get_current();
        if (cur.valid) {
            if (lbl_cur_temp) { snprintf(buf, sizeof(buf), "%.0f", cur.temp); lv_label_set_text(lbl_cur_temp, buf); }
            if (lbl_cur_desc) lv_label_set_text(lbl_cur_desc, cur.description);
            if (lbl_cur_loc)  lv_label_set_text(lbl_cur_loc, cur.location);

            if (lbl_cur_sr) { format_local_time(cur.sunrise, buf, sizeof(buf)); lv_label_set_text(lbl_cur_sr, buf); }
            if (lbl_cur_ss) { format_local_time(cur.sunset, buf, sizeof(buf)); lv_label_set_text(lbl_cur_ss, buf); }

            if (lbl_cur_wind) { snprintf(buf, sizeof(buf), "%.1f m/s %s", cur.wind_speed, deg_to_cardinal(cur.wind_deg)); lv_label_set_text(lbl_cur_wind, buf); }
            if (lbl_cur_cloud) { snprintf(buf, sizeof(buf), "%d%%", cur.clouds); lv_label_set_text(lbl_cur_cloud, buf); }
            if (lbl_cur_vis) { snprintf(buf, sizeof(buf), "%.1f km", (float)cur.visibility / 1000.0f); lv_label_set_text(lbl_cur_vis, buf); }
            if (lbl_cur_pres) { snprintf(buf, sizeof(buf), "%.0f hPa", cur.pressure); lv_label_set_text(lbl_cur_pres, buf); }

            if (img_cur_icon && lv_image_get_src(img_cur_icon) == nullptr) {
                snprintf(buf, sizeof(buf), "L:/img/%s_40x40.bin", cur.icon);
                lv_image_set_src(img_cur_icon, buf);
            }
        }

        // 3. Tab 3: Forecast List 
        const auto& fore = services::weather_manager::get_forecast();
        uint8_t enabled_mask = config::get().forecast_slots;

        if (fore.valid && forecast_scroll_container) {
            for (int i = 0; i < 8; i++) {
                bool is_enabled = (enabled_mask & (1 << i));
                if (dyn_rows[i].base_row) {
                    if (!is_enabled) {
                        lv_obj_add_flag(dyn_rows[i].base_row, LV_OBJ_FLAG_HIDDEN);
                        continue;
                    }
                    lv_obj_clear_flag(dyn_rows[i].base_row, LV_OBJ_FLAG_HIDDEN);

                    snprintf(buf, sizeof(buf), "%.0f°C", fore.blocks[i].temp);
                    lv_label_set_text(dyn_rows[i].lbl_temp, buf);
                    
                    snprintf(buf, sizeof(buf), "%s %d%%", LV_SYMBOL_TINT, fore.blocks[i].pop);
                    lv_label_set_text(dyn_rows[i].lbl_rain, buf);

                    snprintf(buf, sizeof(buf), "~ %.0f m/s", fore.blocks[i].wind_speed); 
                    lv_label_set_text(dyn_rows[i].lbl_wind, buf);

                    format_local_time(fore.blocks[i].dt, buf, sizeof(buf));
                    lv_label_set_text(dyn_rows[i].lbl_time, buf);
                }
            }
        }
    }

} // namespace ui