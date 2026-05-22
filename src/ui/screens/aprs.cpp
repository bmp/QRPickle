#include "aprs.h"
#include "../theme.h"
#include "../fonts.h"
#include "../ui.h"
#include "../../services/aprs_manager.h"
#include <cstdio>
#include <cstring>
#include <Arduino.h>

namespace ui {

    static lv_obj_t* scr = nullptr;
    static lv_obj_t* tabview = nullptr;
    static lv_obj_t* list_container = nullptr;
    static lv_obj_t* lbl_comment = nullptr;
    static lv_timer_t* ui_timer = nullptr;

    struct AprsRow {
        lv_obj_t* base;
        lv_obj_t* l_call;
        lv_obj_t* l_type;
        lv_obj_t* l_dist;
        lv_obj_t* l_brg;
    };
    
    static AprsRow* rows = nullptr; // FIXED: Dynamic allocation

    static void row_click(lv_event_t* e) {
        int idx = (intptr_t)lv_event_get_user_data(e);
        const auto* st = services::AprsManager::get_stations();
        if(idx < services::AprsManager::get_station_count() && lbl_comment) {
            char b[80];
            snprintf(b, sizeof(b), "CMT: %s", st[idx].comment);
            lv_label_set_text(lbl_comment, b);
        }
    }

    static void radar_btn_click(lv_event_t* e) {
        if(lbl_comment) lv_label_set_text(lbl_comment, "RADAR: Opening scope view...");
    }

    static void update_ui(lv_timer_t* t) {
        if(!scr || !rows) return;

        if (services::AprsManager::is_dirty()) {
            lv_obj_add_flag(list_container, LV_OBJ_FLAG_HIDDEN);
            
            const auto* st = services::AprsManager::get_stations();
            size_t c = services::AprsManager::get_station_count();
            
            int vis = 0;
            for(size_t i = 0; i < c && vis < 15; i++) {
                lv_label_set_text(rows[vis].l_call, st[i].callsign);
                lv_label_set_text(rows[vis].l_type, st[i].type);
                
                char dbuf[16];
                snprintf(dbuf, sizeof(dbuf), "%.1f k", st[i].distance_km);
                lv_label_set_text(rows[vis].l_dist, dbuf);
                
                char bbuf[8];
                const char* dirs[] = {"N","NNE","NE","ENE","E","ESE","SE","SSE","S","SSW","SW","WSW","W","WNW","NW","NNW"};
                strcpy(bbuf, dirs[((st[i].bearing_deg + 11) % 360) / 22]);
                lv_label_set_text(rows[vis].l_brg, bbuf);

                lv_obj_clear_flag(rows[vis].base, LV_OBJ_FLAG_HIDDEN);
                vis++;
            }

            for(int i = vis; i < 15; i++) {
                lv_obj_add_flag(rows[i].base, LV_OBJ_FLAG_HIDDEN);
            }
            
            lv_obj_clear_flag(list_container, LV_OBJ_FLAG_HIDDEN);
            services::AprsManager::clear_dirty();
        }
    }

    void draw_aprs_page(lv_obj_t* parent) {
        if (!rows) {
            rows = (AprsRow*)calloc(15, sizeof(AprsRow));
            if (!rows) return;
        }

        scr = lv_obj_create(parent);
        lv_obj_set_size(scr, 320, 240);
        lv_obj_set_style_bg_color(scr, theme_color(COLOR_BG_APP), 0);
        lv_obj_set_style_border_width(scr, 0, 0);
        lv_obj_set_style_pad_all(scr, 0, 0);
        lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

        tabview = lv_tabview_create(scr);
        lv_tabview_set_tab_bar_position(tabview, LV_DIR_TOP);
        lv_tabview_set_tab_bar_size(tabview, 26);

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

        lv_obj_t* t1 = lv_tabview_add_tab(tabview, "TRAFFIC");
        lv_obj_t* t2 = lv_tabview_add_tab(tabview, "MESSAGES");
        lv_obj_t* t3 = lv_tabview_add_tab(tabview, "MY BEACON");

        lv_obj_set_style_bg_color(t1, theme_color(COLOR_BG_APP), 0);
        lv_obj_set_style_pad_all(t1, 0, 0);

        lv_obj_t* head = lv_obj_create(t1);
        lv_obj_set_size(head, 320, 20);
        lv_obj_align(head, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(head, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_side(head, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(head, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(head, 1, 0);
        lv_obj_set_style_pad_all(head, 0, 0);
        lv_obj_clear_flag(head, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* l1 = lv_label_create(head); lv_label_set_text(l1, "CALLSIGN"); lv_obj_set_style_text_font(l1, &font_jetbrains_10, 0); lv_obj_set_style_text_color(l1, theme_color(COLOR_TEXT_MUTED), 0); lv_obj_align(l1, LV_ALIGN_LEFT_MID, 4, 0);
        lv_obj_t* l2 = lv_label_create(head); lv_label_set_text(l2, "TYPE"); lv_obj_set_style_text_font(l2, &font_jetbrains_10, 0); lv_obj_set_style_text_color(l2, theme_color(COLOR_TEXT_MUTED), 0); lv_obj_align(l2, LV_ALIGN_LEFT_MID, 90, 0);
        lv_obj_t* l3 = lv_label_create(head); lv_label_set_text(l3, "DIST"); lv_obj_set_style_text_font(l3, &font_jetbrains_10, 0); lv_obj_set_style_text_color(l3, theme_color(COLOR_TEXT_MUTED), 0); lv_obj_align(l3, LV_ALIGN_LEFT_MID, 180, 0);
        lv_obj_t* l4 = lv_label_create(head); lv_label_set_text(l4, "BRG"); lv_obj_set_style_text_font(l4, &font_jetbrains_10, 0); lv_obj_set_style_text_color(l4, theme_color(COLOR_TEXT_MUTED), 0); lv_obj_align(l4, LV_ALIGN_LEFT_MID, 250, 0);

        lv_obj_t* foot = lv_obj_create(t1);
        lv_obj_set_size(foot, 320, 26);
        lv_obj_align(foot, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(foot, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_side(foot, LV_BORDER_SIDE_TOP, 0);
        lv_obj_set_style_border_color(foot, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(foot, 1, 0);
        lv_obj_clear_flag(foot, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* btn_radar = lv_button_create(foot);
        lv_obj_set_size(btn_radar, 80, 20);
        lv_obj_align(btn_radar, LV_ALIGN_LEFT_MID, 4, 0);
        lv_obj_set_style_bg_color(btn_radar, theme_color(COLOR_ACCENT_PRIMARY), 0);
        lv_obj_set_style_radius(btn_radar, 4, 0);
        lv_obj_add_event_cb(btn_radar, radar_btn_click, LV_EVENT_CLICKED, nullptr);
        lv_obj_t* lbl_r = lv_label_create(btn_radar);
        lv_label_set_text(lbl_r, "(O) RADAR");
        lv_obj_set_style_text_font(lbl_r, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_r, lv_color_hex(0x000000), 0);
        lv_obj_center(lbl_r);

        lbl_comment = lv_label_create(foot);
        lv_label_set_text(lbl_comment, "CMT: Tap row to view");
        lv_obj_set_style_text_font(lbl_comment, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_comment, theme_color(COLOR_TEXT_MAIN), 0);
        lv_obj_align(lbl_comment, LV_ALIGN_LEFT_MID, 95, 0);

        list_container = lv_obj_create(t1);
        lv_obj_set_size(list_container, 320, 142); 
        lv_obj_align(list_container, LV_ALIGN_TOP_MID, 0, 20);
        lv_obj_set_style_bg_opa(list_container, 0, 0);
        lv_obj_set_style_border_width(list_container, 0, 0);
        lv_obj_set_style_pad_all(list_container, 0, 0);
        lv_obj_set_flex_flow(list_container, LV_FLEX_FLOW_COLUMN);
        
        lv_obj_add_flag(list_container, LV_OBJ_FLAG_HIDDEN);

        for(int i=0; i<15; i++) {
            lv_obj_t* r = lv_obj_create(list_container);
            lv_obj_set_size(r, 320, 22);
            lv_obj_set_style_bg_color(r, (i % 2 == 0) ? theme_color(COLOR_BG_PANEL) : theme_color(COLOR_BG_APP), 0);
            lv_obj_set_style_border_width(r, 0, 0);
            lv_obj_set_style_radius(r, 0, 0);
            lv_obj_set_style_pad_all(r, 0, 0);
            lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(r, row_click, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            
            rows[i].base = r;
            
            rows[i].l_call = lv_label_create(r); 
            lv_obj_set_style_text_font(rows[i].l_call, &font_jetbrains_10, 0); 
            lv_obj_set_style_text_color(rows[i].l_call, theme_color(COLOR_ACCENT_PRIMARY), 0);
            lv_obj_align(rows[i].l_call, LV_ALIGN_LEFT_MID, 4, 0);
            
            rows[i].l_type = lv_label_create(r); 
            lv_obj_set_style_text_font(rows[i].l_type, &font_jetbrains_10, 0); 
            lv_obj_set_style_text_color(rows[i].l_type, theme_color(COLOR_TEXT_MAIN), 0); 
            lv_obj_align(rows[i].l_type, LV_ALIGN_LEFT_MID, 90, 0);
            
            rows[i].l_dist = lv_label_create(r); 
            lv_obj_set_style_text_font(rows[i].l_dist, &font_jetbrains_10, 0); 
            lv_obj_set_style_text_color(rows[i].l_dist, theme_color(COLOR_TEXT_MAIN), 0);
            lv_obj_align(rows[i].l_dist, LV_ALIGN_LEFT_MID, 180, 0);
            
            rows[i].l_brg = lv_label_create(r); 
            lv_obj_set_style_text_font(rows[i].l_brg, &font_jetbrains_10, 0); 
            lv_obj_set_style_text_color(rows[i].l_brg, theme_color(COLOR_TEXT_MAIN), 0);
            lv_obj_align(rows[i].l_brg, LV_ALIGN_LEFT_MID, 250, 0);
        }

        lv_obj_clear_flag(list_container, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_event_cb(scr, [](lv_event_t*){
            if(ui_timer) { lv_timer_delete(ui_timer); ui_timer = nullptr; }
            scr = list_container = lbl_comment = tabview = nullptr;
            
            if (rows) {
                free(rows);
                rows = nullptr;
            }
        }, LV_EVENT_DELETE, NULL);

        services::AprsManager::clear_dirty();
        ui_timer = lv_timer_create(update_ui, 500, NULL);
    }
}