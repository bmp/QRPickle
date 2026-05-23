#include "aprs_hub.h"
#include "../theme.h"
#include "../fonts.h"
#include "../ui.h"
#include "../status_bar.h"
#include "../../services/aprs_manager.h"
#include "../../config/config.h"
#include <cstdio>
#include <cstring>
#include <Arduino.h>

namespace ui {

    static lv_obj_t* scr = nullptr;
    static lv_obj_t* tabview = nullptr;
    static lv_obj_t* list_container = nullptr;
    static lv_obj_t* msg_container = nullptr;
    
    static int active_type_idx = 0; 
    static const char* FILTER_TYPES[] = {"TYPE: ALL", "PORTABLE", "RUNNER", "RADIOS", "FIXED"};
    static bool sort_ascending = true;
    static bool force_ui_refresh = false; 

    static lv_obj_t* lbl_b_status = nullptr;
    static lv_obj_t* lbl_b_txcount = nullptr;
    static lv_obj_t* lbl_b_last = nullptr;
    static lv_obj_t* lbl_b_next = nullptr;
    static lv_obj_t* lbl_b_loc = nullptr;
    static lv_obj_t* lbl_b_sym = nullptr;
    static lv_obj_t* lbl_b_payload = nullptr;

    static lv_obj_t* btn_hdr_type = nullptr;
    static lv_obj_t* btn_hdr_dist = nullptr;
    static lv_obj_t* lbl_comment = nullptr;
    static lv_timer_t* ui_timer = nullptr;

    struct AprsRow {
        lv_obj_t* base;
        lv_obj_t* l_call;
        lv_obj_t* l_type;
        lv_obj_t* l_dist;
        lv_obj_t* l_brg;
    };
    static AprsRow* rows = nullptr;

    static void row_click(lv_event_t* e) {
        int idx = (intptr_t)lv_event_get_user_data(e);
        const auto* st = services::AprsManager::get_stations();
        if(idx < services::AprsManager::get_station_count() && lbl_comment) {
            char b[96];
            snprintf(b, sizeof(b), "CMT: %s", st[idx].comment[0] ? st[idx].comment : "(No comment info)");
            lv_label_set_text(lbl_comment, b);
        }
    }

    static void toggle_type_filter_cb(lv_event_t* e) {
        active_type_idx = (active_type_idx + 1) % 5;
        lv_label_set_text(lv_obj_get_child(btn_hdr_type, 0), FILTER_TYPES[active_type_idx]);
        force_ui_refresh = true; 
    }

    static void toggle_dist_sort_cb(lv_event_t* e) {
        sort_ascending = !sort_ascending;
        lv_label_set_text(lv_obj_get_child(btn_hdr_dist, 0), sort_ascending ? "DIST [ASC]" : "DIST [DSC]");
        services::AprsManager::sort_stations(sort_ascending);
        force_ui_refresh = true; 
    }

    static void force_tx_click_cb(lv_event_t* e) {
        services::AprsManager::trigger_manual_beacon();
    }

    static void update_hub_ui(lv_timer_t* t) {
        if(!scr || !rows) return;

        if (services::AprsManager::is_dirty() || force_ui_refresh) {
            force_ui_refresh = false;
            const auto* st = services::AprsManager::get_stations();
            size_t c = services::AprsManager::get_station_count();
            
            if (list_container && !lv_obj_has_flag(list_container, LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_add_flag(list_container, LV_OBJ_FLAG_HIDDEN);
                int vis = 0;
                for(size_t i = 0; i < c && vis < 15; i++) {
                    if (active_type_idx == 1 && strcmp(st[i].type, "Portable") != 0) continue;
                    if (active_type_idx == 2 && strcmp(st[i].type, "Runner") != 0) continue;
                    if (active_type_idx == 3 && strcmp(st[i].type, "Radios") != 0) continue;
                    if (active_type_idx == 4 && strcmp(st[i].type, "Fixed Base") != 0) continue;

                    lv_label_set_text(rows[vis].l_call, st[i].callsign);
                    lv_label_set_text(rows[vis].l_type, st[i].type);
                    
                    char dbuf[16]; snprintf(dbuf, sizeof(dbuf), "%.1f km", st[i].distance_km);
                    lv_label_set_text(rows[vis].l_dist, dbuf);
                    
                    int brg_val = st[i].bearing_deg;
                    if (brg_val < 0 || brg_val >= 360) brg_val = 0;
                    const char* dirs[] = {"N","NNE","NE","ENE","E","ESE","SE","SSE","S","SSW","SW","WSW","W","WNW","NW","NNW"};
                    int dir_idx = ((brg_val + 11) % 360) / 22;
                    if (dir_idx < 0 || dir_idx >= 16) dir_idx = 0;
                    
                    char bbuf[16]; snprintf(bbuf, sizeof(bbuf), "%d\xC2\xB0 %s", brg_val, dirs[dir_idx]);
                    lv_label_set_text(rows[vis].l_brg, bbuf);

                    lv_obj_clear_flag(rows[vis].base, LV_OBJ_FLAG_HIDDEN);
                    vis++;
                }
                for(int i = vis; i < 15; i++) lv_obj_add_flag(rows[i].base, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(list_container, LV_OBJ_FLAG_HIDDEN);
            }
            services::AprsManager::clear_dirty();
        }

        if (services::AprsManager::is_msg_dirty() && msg_container) {
            lv_obj_clean(msg_container);
            const auto* msgs = services::AprsManager::get_messages();
            size_t m_count = services::AprsManager::get_message_count();
            
            for(size_t i=0; i<m_count; i++) {
                lv_obj_t* m_row = lv_obj_create(msg_container);
                lv_obj_set_size(m_row, 310, 24);
                lv_obj_set_style_bg_color(m_row, theme_color(COLOR_BG_PANEL), 0);
                lv_obj_set_style_border_width(m_row, 0, 0);
                lv_obj_set_style_pad_all(m_row, 2, 0);
                
                lv_obj_t* lbl = lv_label_create(m_row);
                lv_label_set_long_mode(lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
                lv_obj_set_width(lbl, 300);
                lv_obj_set_style_text_font(lbl, &font_jetbrains_10, 0);
                
                char m_buf[96]; snprintf(m_buf, sizeof(m_buf), "%s: %s", msgs[i].from, msgs[i].text);
                lv_label_set_text(lbl, m_buf);
            }
            services::AprsManager::clear_msg_dirty();
        }

        if (lbl_b_status && tabview && lv_tabview_get_tab_active(tabview) == 2) {
            auto& cfg = config::get();
            bool is_conn = services::AprsManager::is_connected();
            lv_label_set_text(lbl_b_status, is_conn ? "STATUS: ACTIVE (APRS-IS Secure Link)" : "STATUS: OFFLINE (Reconnecting)");
            lv_obj_set_style_text_color(lbl_b_status, is_conn ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000), 0);
            
            char s_buf[64];
            snprintf(s_buf, sizeof(s_buf), "TX COUNT: %u times", services::AprsManager::get_tx_count());
            lv_label_set_text(lbl_b_txcount, s_buf);
            
            uint32_t last = services::AprsManager::get_last_tx_time();
            if (last == 0) {
                lv_label_set_text(lbl_b_last, "LAST TX: Never transmitted yet.");
                lv_label_set_text(lbl_b_next, "NEXT TX: Awaiting radio handshake...");
            } else {
                uint32_t elapsed_sec = (millis() - last) / 1000;
                snprintf(s_buf, sizeof(s_buf), "LAST TX: %u mins %u secs ago", elapsed_sec / 60, elapsed_sec % 60);
                lv_label_set_text(lbl_b_last, s_buf);
                
                int remaining = (30 * 60) - elapsed_sec; if(remaining < 0) remaining = 0;
                snprintf(s_buf, sizeof(s_buf), "NEXT TX: scheduled in %u mins %u secs", remaining / 60, remaining % 60);
                lv_label_set_text(lbl_b_next, s_buf);
            }
            
            snprintf(s_buf, sizeof(s_buf), "LOCATION: %.4f N / %.4f E [Grid: %s]", cfg.lat, cfg.lon, cfg.grid);
            lv_label_set_text(lbl_b_loc, s_buf);
            
            const char* human_sym = "Observer";
            if (strcmp(cfg.aprs_icon, "/[") == 0) human_sym = "Runner / Tactical Observer";
            else if (strcmp(cfg.aprs_icon, "/-") == 0) human_sym = "House / Fixed Base Station";
            else if (strcmp(cfg.aprs_icon, "I&") == 0) human_sym = "Internet Gateway (IGate)";
            else if (strcmp(cfg.aprs_icon, "\\Y") == 0) human_sym = "APRS/Radios Handheld Matrix";
            else if (strcmp(cfg.aprs_icon, "/;") == 0) human_sym = "Portable Operation Tent";
            else if (strcmp(cfg.aprs_icon, "\\F") == 0) human_sym = "Field Day Operations";
            else if (strcmp(cfg.aprs_icon, "\\;") == 0) human_sym = "Park/Picnic Station Vector";
            snprintf(s_buf, sizeof(s_buf), "MAP ICON: %s (\x25%s)", human_sym, cfg.aprs_icon);
            lv_label_set_text(lbl_b_sym, s_buf);

            char p_buf[96]; services::AprsManager::get_current_payload(p_buf, sizeof(p_buf));
            snprintf(s_buf, sizeof(s_buf), "PAYLOAD: %s-%d>APRS: %s", cfg.callsign, (int)cfg.aprs_ssid, p_buf);
            lv_label_set_text(lbl_b_payload, s_buf);
        }
    }

    void draw_aprs_hub_page(lv_obj_t* parent) {
        if (!rows) {
            rows = (AprsRow*)calloc(15, sizeof(AprsRow));
            if (!rows) return;
        }

        status_bar_set_title("APRS-IS Hub");

        scr = lv_obj_create(parent);
        lv_obj_set_size(scr, 320, 216); 
        lv_obj_set_style_bg_color(scr, theme_color(COLOR_BG_APP), 0);
        lv_obj_set_style_border_width(scr, 0, 0);
        lv_obj_set_style_pad_all(scr, 0, 0);
        lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* foot = lv_obj_create(scr);
        lv_obj_set_size(foot, 320, 36); 
        lv_obj_align(foot, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(foot, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_side(foot, LV_BORDER_SIDE_TOP, 0);
        lv_obj_set_style_border_color(foot, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(foot, 1, 0);
        lv_obj_set_style_pad_all(foot, 0, 0);
        lv_obj_clear_flag(foot, LV_OBJ_FLAG_SCROLLABLE);

        lbl_comment = lv_label_create(foot);
        lv_label_set_text(lbl_comment, "COMMENT: Touch any station row to inspect details.");
        lv_obj_set_style_text_font(lbl_comment, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_comment, theme_color(COLOR_TEXT_MUTED), 0);
        lv_obj_align(lbl_comment, LV_ALIGN_TOP_LEFT, 6, 2);

        lv_obj_t* btn_radar = lv_button_create(foot);
        lv_obj_set_size(btn_radar, 36, 16);
        lv_obj_align(btn_radar, LV_ALIGN_BOTTOM_LEFT, 6, -2);
        lv_obj_set_style_bg_color(btn_radar, theme_color(COLOR_ACCENT_PRIMARY), 0);
        lv_obj_set_style_radius(btn_radar, 3, 0);
        lv_obj_set_style_pad_all(btn_radar, 0, 0);
        lv_obj_add_event_cb(btn_radar, [](lv_event_t*){
            ui_navigate_local(PAGE_APRS_RADAR); 
        }, LV_EVENT_CLICKED, nullptr);
        lv_obj_t* lbl_br = lv_label_create(btn_radar);
        lv_label_set_text(lbl_br, LV_SYMBOL_GPS);
        lv_obj_set_style_text_font(lbl_br, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_br, lv_color_hex(0x000000), 0);
        lv_obj_center(lbl_br);

        tabview = lv_tabview_create(scr);
        lv_tabview_set_tab_bar_position(tabview, LV_DIR_TOP);
        lv_tabview_set_tab_bar_size(tabview, 24);
        lv_obj_set_size(tabview, 320, 180); 
        lv_obj_align(tabview, LV_ALIGN_TOP_MID, 0, 0);

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

        lv_obj_set_style_bg_color(t1, theme_color(COLOR_BG_APP), 0); lv_obj_set_style_pad_all(t1, 0, 0); lv_obj_clear_flag(t1, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(t2, theme_color(COLOR_BG_APP), 0); lv_obj_set_style_pad_all(t2, 4, 0); lv_obj_clear_flag(t2, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(t3, theme_color(COLOR_BG_APP), 0); lv_obj_set_style_pad_all(t3, 4, 0); lv_obj_clear_flag(t3, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* head = lv_obj_create(t1);
        lv_obj_set_size(head, 320, 22);
        lv_obj_align(head, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(head, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_side(head, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(head, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(head, 1, 0);
        lv_obj_set_style_pad_all(head, 0, 0);
        lv_obj_clear_flag(head, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* hl1 = lv_label_create(head); lv_label_set_text(hl1, "CALL"); lv_obj_set_style_text_font(hl1, &font_jetbrains_10, 0); lv_obj_set_style_text_color(hl1, theme_color(COLOR_TEXT_MUTED), 0); lv_obj_align(hl1, LV_ALIGN_LEFT_MID, 6, 0);
        
        btn_hdr_type = lv_button_create(head); lv_obj_set_size(btn_hdr_type, 80, 18); lv_obj_align(btn_hdr_type, LV_ALIGN_LEFT_MID, 62, 0);
        lv_obj_set_style_bg_color(btn_hdr_type, theme_color(COLOR_BG_APP), 0); lv_obj_set_style_border_color(btn_hdr_type, theme_color(COLOR_BORDER), 0); lv_obj_set_style_border_width(btn_hdr_type, 1, 0); lv_obj_set_style_radius(btn_hdr_type, 3, 0); lv_obj_set_style_pad_all(btn_hdr_type, 0, 0);
        lv_obj_add_event_cb(btn_hdr_type, toggle_type_filter_cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_t* lbl_ht = lv_label_create(btn_hdr_type); lv_label_set_text(lbl_ht, FILTER_TYPES[active_type_idx]); lv_obj_set_style_text_font(lbl_ht, &font_jetbrains_10, 0); lv_obj_set_style_text_color(lbl_ht, theme_color(COLOR_TEXT_MAIN), 0); lv_obj_center(lbl_ht);

        btn_hdr_dist = lv_button_create(head); lv_obj_set_size(btn_hdr_dist, 72, 18); lv_obj_align(btn_hdr_dist, LV_ALIGN_LEFT_MID, 148, 0);
        lv_obj_set_style_bg_color(btn_hdr_dist, theme_color(COLOR_BG_APP), 0); lv_obj_set_style_border_color(btn_hdr_dist, theme_color(COLOR_BORDER), 0); lv_obj_set_style_border_width(btn_hdr_dist, 1, 0); lv_obj_set_style_radius(btn_hdr_dist, 3, 0); lv_obj_set_style_pad_all(btn_hdr_dist, 0, 0);
        lv_obj_add_event_cb(btn_hdr_dist, toggle_dist_sort_cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_t* lbl_hd = lv_label_create(btn_hdr_dist); lv_label_set_text(lbl_hd, "DIST [ASC]"); lv_obj_set_style_text_font(lbl_hd, &font_jetbrains_10, 0); lv_obj_set_style_text_color(lbl_hd, theme_color(COLOR_TEXT_MAIN), 0); lv_obj_center(lbl_hd);

        lv_obj_t* hl4 = lv_label_create(head); lv_label_set_text(hl4, "BRG / AZM"); lv_obj_set_style_text_font(hl4, &font_jetbrains_10, 0); lv_obj_set_style_text_color(hl4, theme_color(COLOR_TEXT_MUTED), 0); lv_obj_align(hl4, LV_ALIGN_LEFT_MID, 226, 0);

        list_container = lv_obj_create(t1);
        lv_obj_set_size(list_container, 320, 142); 
        lv_obj_align(list_container, LV_ALIGN_TOP_MID, 0, 22);
        lv_obj_set_style_bg_opa(list_container, 0, 0);
        lv_obj_set_style_border_width(list_container, 0, 0);
        lv_obj_set_style_pad_all(list_container, 0, 0);
        lv_obj_set_flex_flow(list_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_add_flag(list_container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(list_container, LV_OBJ_FLAG_HIDDEN);

        for(int i=0; i<15; i++) {
            lv_obj_t* r = lv_obj_create(list_container);
            lv_obj_set_size(r, 320, 21);
            lv_obj_set_style_bg_color(r, (i % 2 == 0) ? theme_color(COLOR_BG_PANEL) : theme_color(COLOR_BG_APP), 0);
            lv_obj_set_style_border_width(r, 0, 0);
            lv_obj_set_style_radius(r, 0, 0);
            lv_obj_set_style_pad_all(r, 0, 0);
            lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(r, row_click, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            
            rows[i].base = r;
            rows[i].l_call = lv_label_create(r); lv_obj_set_style_text_font(rows[i].l_call, &font_jetbrains_10, 0); lv_obj_set_style_text_color(rows[i].l_call, theme_color(COLOR_TEXT_MAIN), 0); lv_obj_align(rows[i].l_call, LV_ALIGN_LEFT_MID, 6, 0);
            rows[i].l_type = lv_label_create(r); lv_obj_set_style_text_font(rows[i].l_type, &font_jetbrains_10, 0); lv_obj_set_style_text_color(rows[i].l_type, theme_color(COLOR_TEXT_MUTED), 0); lv_obj_align(rows[i].l_type, LV_ALIGN_LEFT_MID, 62, 0);
            rows[i].l_dist = lv_label_create(r); lv_obj_set_style_text_font(rows[i].l_dist, &font_jetbrains_10, 0); lv_obj_set_style_text_color(rows[i].l_dist, theme_color(COLOR_ACCENT_PRIMARY), 0); lv_obj_align(rows[i].l_dist, LV_ALIGN_LEFT_MID, 148, 0);
            rows[i].l_brg = lv_label_create(r);  lv_obj_set_style_text_font(rows[i].l_brg, &font_jetbrains_10, 0);  lv_obj_set_style_text_color(rows[i].l_brg, theme_color(COLOR_TEXT_MAIN), 0); lv_obj_align(rows[i].l_brg, LV_ALIGN_LEFT_MID, 226, 0);
        }
        lv_obj_clear_flag(list_container, LV_OBJ_FLAG_HIDDEN);

        msg_container = lv_obj_create(t2);
        lv_obj_set_size(msg_container, 312, 156);
        lv_obj_align(msg_container, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_opa(msg_container, 0, 0);
        lv_obj_set_style_border_width(msg_container, 0, 0);
        lv_obj_set_style_pad_all(msg_container, 2, 0);
        lv_obj_set_flex_flow(msg_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(msg_container, 4, 0);
        lv_obj_add_flag(msg_container, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* b_card = lv_obj_create(t3);
        lv_obj_set_size(b_card, 312, 130);
        lv_obj_align(b_card, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(b_card, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_color(b_card, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(b_card, 1, 0);
        lv_obj_set_style_pad_all(b_card, 4, 0);
        lv_obj_clear_flag(b_card, LV_OBJ_FLAG_SCROLLABLE);

        lbl_b_status = lv_label_create(b_card);  lv_obj_set_style_text_font(lbl_b_status, &font_jetbrains_10, 0); lv_obj_align(lbl_b_status, LV_ALIGN_TOP_LEFT, 2, 2);
        lbl_b_txcount = lv_label_create(b_card); lv_obj_set_style_text_font(lbl_b_txcount, &font_jetbrains_10, 0); lv_obj_set_style_text_color(lbl_b_txcount, theme_color(COLOR_TEXT_MAIN), 0); lv_obj_align(lbl_b_txcount, LV_ALIGN_TOP_RIGHT, -2, 2);
        lbl_b_last = lv_label_create(b_card);    lv_obj_set_style_text_font(lbl_b_last, &font_jetbrains_10, 0); lv_obj_set_style_text_color(lbl_b_last, theme_color(COLOR_TEXT_MAIN), 0); lv_obj_align(lbl_b_last, LV_ALIGN_TOP_LEFT, 2, 16);
        lbl_b_next = lv_label_create(b_card);    lv_obj_set_style_text_font(lbl_b_next, &font_jetbrains_10, 0); lv_obj_set_style_text_color(lbl_b_next, theme_color(COLOR_TEXT_MUTED), 0); lv_obj_align(lbl_b_next, LV_ALIGN_TOP_LEFT, 2, 30);
        lbl_b_loc = lv_label_create(b_card);     lv_obj_set_style_text_font(lbl_b_loc, &font_jetbrains_10, 0); lv_obj_set_style_text_color(lbl_b_loc, theme_color(COLOR_TEXT_MAIN), 0); lv_obj_align(lbl_b_loc, LV_ALIGN_TOP_LEFT, 2, 48);
        lbl_b_sym = lv_label_create(b_card);     lv_obj_set_style_text_font(lbl_b_sym, &font_jetbrains_10, 0); lv_obj_set_style_text_color(lbl_b_sym, theme_color(COLOR_TEXT_MAIN), 0); lv_obj_align(lbl_b_sym, LV_ALIGN_TOP_LEFT, 2, 62);
        
        lbl_b_payload = lv_label_create(b_card);
        lv_label_set_long_mode(lbl_b_payload, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_width(lbl_b_payload, 295);
        lv_obj_set_style_text_font(lbl_b_payload, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_b_payload, theme_color(COLOR_ACCENT_PRIMARY), 0);
        lv_obj_align(lbl_b_payload, LV_ALIGN_TOP_LEFT, 2, 84);

        lv_obj_t* btn_tx = lv_button_create(t3);
        lv_obj_set_size(btn_tx, 120, 20);
        lv_obj_align(btn_tx, LV_ALIGN_TOP_LEFT, 4, 114); 
        lv_obj_set_style_bg_color(btn_tx, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_color(btn_tx, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(btn_tx, 1, 0);
        lv_obj_set_style_radius(btn_tx, 3, 0);
        lv_obj_add_event_cb(btn_tx, force_tx_click_cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_t* lbl_btx = lv_label_create(btn_tx);
        lv_label_set_text(lbl_btx, "FORCE BEACON NOW");
        lv_obj_set_style_text_font(lbl_btx, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_btx, theme_color(COLOR_TEXT_MAIN), 0);
        lv_obj_center(lbl_btx);

        lv_obj_add_event_cb(scr, [](lv_event_t*){
            if(ui_timer) { lv_timer_delete(ui_timer); ui_timer = nullptr; }
            scr = list_container = msg_container = lbl_comment = tabview = btn_hdr_type = btn_hdr_dist = nullptr;
            lbl_b_status = lbl_b_txcount = lbl_b_last = lbl_b_next = lbl_b_loc = lbl_b_sym = lbl_b_payload = nullptr;
            if (rows) { free(rows); rows = nullptr; }
        }, LV_EVENT_DELETE, NULL);

        services::AprsManager::clear_dirty();
        services::AprsManager::clear_msg_dirty();
        services::AprsManager::sort_stations(sort_ascending);
        
        ui_timer = lv_timer_create(update_hub_ui, 300, NULL);
    }
}