#include "aprs_radar.h"
#include "../theme.h"
#include "../fonts.h"
#include "../ui.h"
#include "../../services/aprs_manager.h"
#include <cstdio>
#include <cstring>
#include <math.h>
#include <Arduino.h>

namespace ui {

    static lv_obj_t* scr = nullptr;
    static lv_obj_t* radar_scope_panel = nullptr;
    static lv_obj_t* lbl_r_call = nullptr;
    static lv_obj_t* lbl_r_type = nullptr;
    static lv_obj_t* lbl_r_dist = nullptr;
    static lv_obj_t* lbl_r_brg = nullptr;
    static lv_obj_t* lbl_r_cmt = nullptr;
    static lv_obj_t* lbl_r_range = nullptr;
    static lv_obj_t* radar_dots[30];
    static lv_timer_t* ui_timer = nullptr;

    static void radar_dot_click_cb(lv_event_t* e) {
        int idx = (intptr_t)lv_event_get_user_data(e);
        const auto* st = services::AprsManager::get_stations();
        if (idx < services::AprsManager::get_station_count()) {
            lv_label_set_text(lbl_r_call, st[idx].callsign);
            lv_label_set_text(lbl_r_type, st[idx].type);
            
            char buf[32];
            snprintf(buf, sizeof(buf), "Dist: %.1f km", st[idx].distance_km);
            lv_label_set_text(lbl_r_dist, buf);
            
            snprintf(buf, sizeof(buf), "Azim: %d\xC2\xB0", st[idx].bearing_deg);
            lv_label_set_text(lbl_r_brg, buf);
            
            snprintf(buf, sizeof(buf), "Cmt:\n%s", st[idx].comment[0] ? st[idx].comment : "(None)");
            lv_label_set_text(lbl_r_cmt, buf);
        }
    }

    static void update_radar_ui(lv_timer_t* t) {
        if (!scr || !radar_scope_panel) return;

        const auto* st = services::AprsManager::get_stations();
        size_t c = services::AprsManager::get_station_count();
        
        float max_visible_dist = 10.0f; 
        for (size_t i = 0; i < c; i++) {
            if (st[i].distance_km > 0.1f && st[i].distance_km <= 100.0f) {
                if (st[i].distance_km > max_visible_dist) {
                    max_visible_dist = st[i].distance_km;
                }
            }
        }

        char r_buf[16]; 
        snprintf(r_buf, sizeof(r_buf), "Rng: %.0fkm", max_visible_dist);
        lv_label_set_text(lbl_r_range, r_buf);

        float radar_scale = 102.0f / max_visible_dist;

        for (size_t i = 0; i < 30; i++) {
            if (i < c && st[i].distance_km <= 100.0f) {
                float rad = (90.0f - (float)st[i].bearing_deg) * M_PI / 180.0f;
                
                int x_offset = (int)(st[i].distance_km * radar_scale * cos(rad));
                int y_offset = (int)(st[i].distance_km * radar_scale * sin(rad));
                
                int target_x = 110 + x_offset - 8; 
                int target_y = 110 - y_offset - 8;

                lv_obj_set_pos(radar_dots[i], target_x, target_y);
                lv_obj_clear_flag(radar_dots[i], LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(radar_dots[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
        services::AprsManager::clear_dirty();
    }

    void draw_aprs_radar_page(lv_obj_t* parent) {
        scr = lv_obj_create(parent);
        lv_obj_set_size(scr, 320, 240);
        lv_obj_set_style_bg_color(scr, theme_color(COLOR_BG_APP), 0);
        lv_obj_set_style_border_width(scr, 0, 0);
        lv_obj_set_style_pad_all(scr, 0, 0);
        lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

        radar_scope_panel = lv_obj_create(scr);
        lv_obj_set_size(radar_scope_panel, 220, 220);
        lv_obj_align(radar_scope_panel, LV_ALIGN_TOP_LEFT, 5, 10);
        lv_obj_set_style_bg_color(radar_scope_panel, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_color(radar_scope_panel, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(radar_scope_panel, 1, 0);
        lv_obj_set_style_radius(radar_scope_panel, LV_RADIUS_CIRCLE, 0);
        lv_obj_clear_flag(radar_scope_panel, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* ring50 = lv_obj_create(radar_scope_panel);
        lv_obj_set_size(ring50, 110, 110); lv_obj_center(ring50);
        lv_obj_set_style_bg_opa(ring50, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(ring50, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(ring50, 1, 0);
        lv_obj_set_style_radius(ring50, LV_RADIUS_CIRCLE, 0);

        lv_obj_t* center_cross = lv_obj_create(radar_scope_panel);
        lv_obj_set_size(center_cross, 4, 4); lv_obj_center(center_cross);
        lv_obj_set_style_bg_color(center_cross, theme_color(COLOR_ACCENT_PRIMARY), 0);
        lv_obj_set_style_radius(center_cross, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(center_cross, 0, 0);

        lv_obj_t* lbl_n = lv_label_create(radar_scope_panel); lv_label_set_text(lbl_n, "N"); lv_obj_set_style_text_font(lbl_n, &font_jetbrains_10, 0); lv_obj_set_style_text_color(lbl_n, theme_color(COLOR_TEXT_MUTED), 0); lv_obj_align(lbl_n, LV_ALIGN_TOP_MID, 0, 2);
        lv_obj_t* lbl_s = lv_label_create(radar_scope_panel); lv_label_set_text(lbl_s, "S"); lv_obj_set_style_text_font(lbl_s, &font_jetbrains_10, 0); lv_obj_set_style_text_color(lbl_s, theme_color(COLOR_TEXT_MUTED), 0); lv_obj_align(lbl_s, LV_ALIGN_BOTTOM_MID, 0, -2);
        lv_obj_t* lbl_w = lv_label_create(radar_scope_panel); lv_label_set_text(lbl_w, "W"); lv_obj_set_style_text_font(lbl_w, &font_jetbrains_10, 0); lv_obj_set_style_text_color(lbl_w, theme_color(COLOR_TEXT_MUTED), 0); lv_obj_align(lbl_w, LV_ALIGN_LEFT_MID, 2, 0);
        lv_obj_t* lbl_e = lv_label_create(radar_scope_panel); lv_label_set_text(lbl_e, "E"); lv_obj_set_style_text_font(lbl_e, &font_jetbrains_10, 0); lv_obj_set_style_text_color(lbl_e, theme_color(COLOR_TEXT_MUTED), 0); lv_obj_align(lbl_e, LV_ALIGN_RIGHT_MID, -2, 0);

        lv_obj_t* r_card = lv_obj_create(scr);
        lv_obj_set_size(r_card, 85, 230);
        lv_obj_align(r_card, LV_ALIGN_TOP_RIGHT, -5, 5);
        lv_obj_set_style_bg_color(r_card, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_color(r_card, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(r_card, 1, 0);
        lv_obj_set_style_pad_all(r_card, 3, 0);
        lv_obj_clear_flag(r_card, LV_OBJ_FLAG_SCROLLABLE);

        lbl_r_call = lv_label_create(r_card); lv_label_set_text(lbl_r_call, "NO TARGET"); lv_obj_set_style_text_font(lbl_r_call, &font_atkinson_14, 0); lv_obj_set_style_text_color(lbl_r_call, theme_color(COLOR_ACCENT_PRIMARY), 0); lv_obj_align(lbl_r_call, LV_ALIGN_TOP_LEFT, 1, 2);
        lbl_r_type = lv_label_create(r_card); lv_label_set_text(lbl_r_type, "--"); lv_obj_set_style_text_font(lbl_r_type, &font_jetbrains_10, 0); lv_obj_set_style_text_color(lbl_r_type, theme_color(COLOR_TEXT_MUTED), 0); lv_obj_align(lbl_r_type, LV_ALIGN_TOP_LEFT, 1, 16);
        lbl_r_dist = lv_label_create(r_card); lv_label_set_text(lbl_r_dist, "Dist: --"); lv_obj_set_style_text_font(lbl_r_dist, &font_jetbrains_10, 0); lv_obj_align(lbl_r_dist, LV_ALIGN_TOP_LEFT, 1, 30);
        lbl_r_brg = lv_label_create(r_card);  lv_label_set_text(lbl_r_brg, "Azim: --");  lv_obj_set_style_text_font(lbl_r_brg, &font_jetbrains_10, 0);  lv_obj_align(lbl_r_brg, LV_ALIGN_TOP_LEFT, 1, 42);
        
        lbl_r_cmt = lv_label_create(r_card);
        lv_label_set_text(lbl_r_cmt, "Cmt:\nTap node.");
        lv_obj_set_style_text_font(lbl_r_cmt, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_r_cmt, theme_color(COLOR_TEXT_MUTED), 0);
        lv_obj_align(lbl_r_cmt, LV_ALIGN_TOP_LEFT, 1, 56);
        lv_label_set_long_mode(lbl_r_cmt, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(lbl_r_cmt, 79);

        lbl_r_range = lv_label_create(r_card);
        lv_label_set_text(lbl_r_range, "Rng: --km");
        lv_obj_set_style_text_font(lbl_r_range, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_r_range, theme_color(COLOR_TEXT_MUTED), 0);
        lv_obj_align(lbl_r_range, LV_ALIGN_TOP_LEFT, 1, 175);

        for (int i = 0; i < 30; i++) {
            radar_dots[i] = lv_button_create(radar_scope_panel);
            lv_obj_set_size(radar_dots[i], 16, 16);
            lv_obj_set_style_bg_opa(radar_dots[i], LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(radar_dots[i], 0, 0);
            lv_obj_set_style_shadow_width(radar_dots[i], 0, 0);
            lv_obj_set_style_pad_all(radar_dots[i], 0, 0);
            lv_obj_add_event_cb(radar_dots[i], radar_dot_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            lv_obj_add_flag(radar_dots[i], LV_OBJ_FLAG_HIDDEN);

            lv_obj_t* visual_dot = lv_obj_create(radar_dots[i]);
            lv_obj_set_size(visual_dot, 6, 6);
            lv_obj_center(visual_dot);
            lv_obj_set_style_bg_color(visual_dot, theme_color(COLOR_ACCENT_PRIMARY), 0);
            lv_obj_set_style_border_width(visual_dot, 0, 0);
            lv_obj_set_style_radius(visual_dot, LV_RADIUS_CIRCLE, 0);
            lv_obj_remove_flag(visual_dot, LV_OBJ_FLAG_CLICKABLE); 
        }

        lv_obj_t* btn_back = lv_button_create(r_card);
        lv_obj_set_size(btn_back, 38, 20);
        lv_obj_align(btn_back, LV_ALIGN_BOTTOM_LEFT, 0, -2);
        lv_obj_set_style_bg_color(btn_back, theme_color(COLOR_BG_APP), 0);
        lv_obj_set_style_border_color(btn_back, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(btn_back, 1, 0);
        lv_obj_set_style_radius(btn_back, 3, 0);
        lv_obj_add_event_cb(btn_back, [](lv_event_t*){
            ui_navigate_local(PAGE_APRS); 
        }, LV_EVENT_CLICKED, nullptr);
        lv_obj_t* lbl_kb = lv_label_create(btn_back); lv_label_set_text(lbl_kb, LV_SYMBOL_LEFT); lv_obj_set_style_text_font(lbl_kb, &font_jetbrains_10, 0); lv_obj_center(lbl_kb);

        lv_obj_t* btn_home = lv_button_create(r_card);
        lv_obj_set_size(btn_home, 38, 20);
        lv_obj_align(btn_home, LV_ALIGN_BOTTOM_RIGHT, 0, -2);
        lv_obj_set_style_bg_color(btn_home, theme_color(COLOR_BG_APP), 0);
        lv_obj_set_style_border_color(btn_home, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(btn_home, 1, 0);
        lv_obj_set_style_radius(btn_home, 3, 0);
        lv_obj_add_event_cb(btn_home, [](lv_event_t*){
            ui_navigate_local(PAGE_DASHBOARD); 
        }, LV_EVENT_CLICKED, nullptr);
        lv_obj_t* lbl_kh = lv_label_create(btn_home); lv_label_set_text(lbl_kh, "HM"); lv_obj_set_style_text_font(lbl_kh, &font_jetbrains_10, 0); lv_obj_center(lbl_kh);

        lv_obj_add_event_cb(scr, [](lv_event_t*){
            if (ui_timer) { lv_timer_delete(ui_timer); ui_timer = nullptr; }
            scr = radar_scope_panel = lbl_r_call = lbl_r_type = lbl_r_dist = lbl_r_brg = lbl_r_cmt = lbl_r_range = nullptr;
        }, LV_EVENT_DELETE, NULL);

        update_radar_ui(nullptr);
        ui_timer = lv_timer_create(update_radar_ui, 300, NULL);
    }
}