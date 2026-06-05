#include "xota.h"
#include "../theme.h"
#include "../fonts.h"
#include "../ui.h"
#include "../../services/pota_manager.h"
#include "../../services/sota_manager.h"
#include "../../services/dx_manager.h"       
#include "../../services/hamalert_manager.h" 
#include "../../services/aprs_manager.h"    // NEW: Include to reclaim the 10KB stack
#include <cstdio>
#include <cstring>
#include <strings.h>
#include <Arduino.h>

namespace ui {

    enum XotaTab { TAB_POTA, TAB_SOTA };
    static XotaTab active_tab = TAB_POTA;

    static lv_obj_t* scr = nullptr;
    static lv_obj_t* btn_tab_pota = nullptr;
    static lv_obj_t* btn_tab_sota = nullptr;
    static lv_obj_t* lbl_tab_pota = nullptr;
    static lv_obj_t* lbl_tab_sota = nullptr;
    static lv_obj_t* lbl_hdr_ref = nullptr;
    static lv_obj_t* btn_freq = nullptr;
    static lv_obj_t* btn_mode = nullptr;
    static lv_obj_t* btn_qrp = nullptr;
    static lv_obj_t* list_container = nullptr;
    static lv_obj_t* lbl_comment = nullptr;
    static lv_obj_t* lbl_loading = nullptr; 
    static lv_obj_t* status_dot = nullptr;
    static lv_timer_t* ui_timer = nullptr;
    static lv_timer_t* delayed_fetch_timer = nullptr;
    
    static uint32_t last_auto_refresh_millis = 0;

    static int active_band = 0; 
    static const char* BANDS[] = {"FRQ", "160", "80", "40", "20", "15", "10", "6"};
    static int active_mode = 0; 
    static const char* MODES[] = {"MOD", "CW", "SSB", "FT8", "FT4"};
    static int active_qrp = 0; 
    static const char* QRPS[] = {"Q", "•", "-"};

    #define MAX_UI_ROWS 15 

    struct RowX {
        lv_obj_t* base;
        lv_obj_t* l_time;
        lv_obj_t* l_ref;
        lv_obj_t* l_freq;
        lv_obj_t* l_mode;
        lv_obj_t* l_act;
        lv_obj_t* l_q;
    };
    
    static RowX* rows = nullptr;

    static bool match_b(float f, int b) {
        if(b==0) return true;
        switch(b) {
            case 1: return f>=1.8f && f<=2.0f;
            case 2: return f>=3.5f && f<=4.0f;
            case 3: return f>=7.0f && f<=7.3f;
            case 4: return f>=14.0f && f<=14.35f;
            case 5: return f>=21.0f && f<=21.45f;
            case 6: return f>=28.0f && f<=29.7f;
            case 7: return f>=50.0f && f<=54.0f;
            default: return false;
        }
    }

    static void update_ui(lv_timer_t* t);

    static void execute_delayed_fetch(lv_timer_t* t) {
        if (active_tab == TAB_POTA) {
            services::PotaManager::fetch_async(); 
        } else {
            services::SotaManager::fetch_async();
        }

        if (lbl_comment) {
            lv_label_set_text(lbl_comment, "COMMENT: Tap a row to read.");
        }
        services::PotaManager::clear_dirty();
        services::SotaManager::clear_dirty();
        
        update_ui(nullptr); 
        lv_timer_delete(t); 
        delayed_fetch_timer = nullptr;
    }

    static void set_tab(XotaTab t) {
        if (delayed_fetch_timer) return; 

        active_tab = t;
        last_auto_refresh_millis = millis(); 

        if (t == TAB_POTA) {
            lv_obj_set_style_text_color(lbl_tab_pota, theme_color(COLOR_ACCENT_PRIMARY), 0);
            lv_obj_set_style_border_width(btn_tab_pota, 2, 0);
            lv_obj_set_style_border_color(btn_tab_pota, theme_color(COLOR_ACCENT_PRIMARY), 0);
            
            lv_obj_set_style_text_color(lbl_tab_sota, theme_color(COLOR_TEXT_MUTED), 0);
            lv_obj_set_style_border_width(btn_tab_sota, 0, 0);
            lv_label_set_text(lbl_hdr_ref, "PARK");
        } else {
            lv_obj_set_style_text_color(lbl_tab_pota, theme_color(COLOR_TEXT_MUTED), 0);
            lv_obj_set_style_border_width(btn_tab_pota, 0, 0);
            
            lv_obj_set_style_text_color(lbl_tab_sota, theme_color(COLOR_ACCENT_PRIMARY), 0);
            lv_obj_set_style_border_width(btn_tab_sota, 2, 0);
            lv_obj_set_style_border_color(btn_tab_sota, theme_color(COLOR_ACCENT_PRIMARY), 0);
            lv_label_set_text(lbl_hdr_ref, "SUMMIT");
        }

        bool needs_fetch = false;
        if (t == TAB_POTA && (millis() - services::PotaManager::get_last_fetch_time() > 30000 || services::PotaManager::get_last_fetch_time() == 0)) {
            needs_fetch = true;
        } else if (t == TAB_SOTA && (millis() - services::SotaManager::get_last_fetch_time() > 30000 || services::SotaManager::get_last_fetch_time() == 0)) {
            needs_fetch = true;
        }

        if (needs_fetch) {
            if (list_container) lv_obj_add_flag(list_container, LV_OBJ_FLAG_HIDDEN);
            if (lbl_loading) lv_obj_clear_flag(lbl_loading, LV_OBJ_FLAG_HIDDEN);
            
            if (lbl_comment) {
                lv_label_set_text(lbl_comment, "Please wait...");
            }
            
            delayed_fetch_timer = lv_timer_create(execute_delayed_fetch, 100, NULL);
        } else {
            services::PotaManager::clear_dirty();
            services::SotaManager::clear_dirty();
            update_ui(nullptr);
        }
    }

    static void cycle_cb(lv_event_t* e) {
        if (delayed_fetch_timer) return; 

        int id = (intptr_t)lv_event_get_user_data(e);
        if(id == 1) { active_band = (active_band + 1) % 8; lv_label_set_text(lv_obj_get_child(btn_freq, 0), BANDS[active_band]); }
        if(id == 2) { active_mode = (active_mode + 1) % 5; lv_label_set_text(lv_obj_get_child(btn_mode, 0), MODES[active_mode]); }
        if(id == 3) { active_qrp = (active_qrp + 1) % 3; lv_label_set_text(lv_obj_get_child(btn_qrp, 0), QRPS[active_qrp]); }
        
        services::PotaManager::clear_dirty(); 
        services::SotaManager::clear_dirty(); 
        update_ui(nullptr);
    }

    static void row_click(lv_event_t* e) {
        int idx = (intptr_t)lv_event_get_user_data(e);
        char b[64] = "COMMENT: ";
        
        if (active_tab == TAB_POTA) {
            const auto* sp = services::PotaManager::get_spots();
            if(idx < services::PotaManager::get_spot_count()) strncat(b, sp[idx].comment, sizeof(b)-10);
        } else {
            const auto* sp = services::SotaManager::get_spots();
            if(idx < services::SotaManager::get_spot_count()) strncat(b, sp[idx].comment, sizeof(b)-10);
        }
        if(lbl_comment) lv_label_set_text(lbl_comment, b);
    }

    static void update_ui(lv_timer_t* t) {
        if(!scr || !rows) return;

        bool fetching = (delayed_fetch_timer != nullptr);
        bool dirty = (active_tab == TAB_POTA) ? services::PotaManager::is_dirty() : services::SotaManager::is_dirty();

        if (status_dot) {
            lv_obj_set_style_bg_color(status_dot, fetching ? lv_color_hex(0xFF9900) : lv_color_hex(0x00FF00), 0);
        }

        if (dirty || t == nullptr) { 
            if (lbl_loading) lv_obj_add_flag(lbl_loading, LV_OBJ_FLAG_HIDDEN);
            
            lv_obj_add_flag(list_container, LV_OBJ_FLAG_HIDDEN);
            int vis = 0;

            if (active_tab == TAB_POTA) {
                const auto* spots = services::PotaManager::get_spots();
                size_t c = services::PotaManager::get_spot_count();
                for(size_t i=0; i<c && vis < MAX_UI_ROWS; i++) {
                    bool show = match_b(spots[i].freq, active_band) &&
                    (active_mode == 0 || strcasecmp(spots[i].mode, MODES[active_mode])==0) &&
                    (active_qrp == 0 || (active_qrp==1 && spots[i].is_qrp) || (active_qrp==2 && !spots[i].is_qrp));
                    if(show) {
                        lv_label_set_text(rows[vis].l_time, spots[i].time);
                        lv_label_set_text(rows[vis].l_ref, spots[i].reference);
                        char fb[8]; snprintf(fb, sizeof(fb), "%.3f", spots[i].freq);
                        lv_label_set_text(rows[vis].l_freq, fb);
                        lv_label_set_text(rows[vis].l_mode, spots[i].mode);
                        lv_label_set_text(rows[vis].l_act, spots[i].activator);
                        lv_label_set_text(rows[vis].l_q, spots[i].is_qrp ? "•" : "-");
                        lv_obj_set_style_text_color(rows[vis].l_q, spots[i].is_qrp ? theme_color(COLOR_ACCENT_PRIMARY) : theme_color(COLOR_TEXT_MAIN), 0);
                        lv_obj_clear_flag(rows[vis].base, LV_OBJ_FLAG_HIDDEN);
                        vis++;
                    }
                }
                services::PotaManager::clear_dirty();
            } else {
                const auto* spots = services::SotaManager::get_spots();
                size_t c = services::SotaManager::get_spot_count();
                for(size_t i=0; i<c && vis < MAX_UI_ROWS; i++) {
                    bool show = match_b(spots[i].freq, active_band) &&
                    (active_mode == 0 || strcasecmp(spots[i].mode, MODES[active_mode])==0) &&
                    (active_qrp == 0 || (active_qrp==1 && spots[i].is_qrp) || (active_qrp==2 && !spots[i].is_qrp));
                    if(show) {
                        lv_label_set_text(rows[vis].l_time, spots[i].time);
                        lv_label_set_text(rows[vis].l_ref, spots[i].summit);
                        char fb[8]; snprintf(fb, sizeof(fb), "%.3f", spots[i].freq);
                        lv_label_set_text(rows[vis].l_freq, fb);
                        lv_label_set_text(rows[vis].l_mode, spots[i].mode);
                        lv_label_set_text(rows[vis].l_act, spots[i].activator);
                        lv_label_set_text(rows[vis].l_q, spots[i].is_qrp ? "•" : "-");
                        lv_obj_set_style_text_color(rows[vis].l_q, spots[i].is_qrp ? theme_color(COLOR_ACCENT_PRIMARY) : theme_color(COLOR_TEXT_MAIN), 0);
                        lv_obj_clear_flag(rows[vis].base, LV_OBJ_FLAG_HIDDEN);
                        vis++;
                    }
                }
                services::SotaManager::clear_dirty();
            }

            for(int i=vis; i<MAX_UI_ROWS; i++) lv_obj_add_flag(rows[i].base, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(list_container, LV_OBJ_FLAG_HIDDEN);
        }
    }

    static void async_resume_task(void* param) {
        vTaskDelay(pdMS_TO_TICKS(2000)); 
        Serial.println("[xOTA] Quiet period ended. Re-establishing core TCP sockets...");
        services::DxManager::start();
        services::HamAlertManager::start();
        services::AprsManager::start(); // RESTORED: APRS monitoring loop resumes cleanly
        vTaskDelete(NULL);
    }

    void draw_xota_page(lv_obj_t* parent) {
        Serial.println("[xOTA] Entry. Suspending core monitoring sockets to free RAM...");
        services::DxManager::stop();
        services::HamAlertManager::stop();
        services::AprsManager::stop(); // NEW: Halts 10KB APRS task loop immediately on entry

        if (!rows) {
            rows = (RowX*)calloc(MAX_UI_ROWS, sizeof(RowX));
            if (!rows) return; 
        }

        scr = lv_obj_create(parent);
        lv_obj_set_size(scr, 320, 240);
        lv_obj_set_style_bg_color(scr, theme_color(COLOR_BG_APP), 0);
        lv_obj_set_style_border_width(scr, 0, 0);
        lv_obj_set_style_pad_all(scr, 0, 0);

        status_dot = lv_obj_create(lv_layer_top());
        lv_obj_set_size(status_dot, 6, 6);
        lv_obj_set_pos(status_dot, 150, 9);
        lv_obj_set_style_radius(status_dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(status_dot, 0, 0);

        lv_obj_t* tabs = lv_obj_create(scr);
        lv_obj_set_size(tabs, 320, 24);
        lv_obj_set_style_bg_color(tabs, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_width(tabs, 0, 0);
        lv_obj_set_style_pad_all(tabs, 0, 0);
        lv_obj_clear_flag(tabs, LV_OBJ_FLAG_SCROLLABLE);
        
        btn_tab_pota = lv_button_create(tabs);
        lv_obj_set_size(btn_tab_pota, 160, 24);
        lv_obj_align(btn_tab_pota, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_style_radius(btn_tab_pota, 0, 0);
        lv_obj_set_style_bg_color(btn_tab_pota, theme_color(COLOR_BG_PANEL), 0); 
        lv_obj_set_style_border_side(btn_tab_pota, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_add_event_cb(btn_tab_pota, [](lv_event_t*){ set_tab(TAB_POTA); }, LV_EVENT_CLICKED, nullptr);
        lbl_tab_pota = lv_label_create(btn_tab_pota);
        lv_label_set_text(lbl_tab_pota, "POTA SPOTS");
        lv_obj_set_style_text_font(lbl_tab_pota, &font_atkinson_14, 0); 
        lv_obj_center(lbl_tab_pota);

        btn_tab_sota = lv_button_create(tabs);
        lv_obj_set_size(btn_tab_sota, 160, 24);
        lv_obj_align(btn_tab_sota, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_style_radius(btn_tab_sota, 0, 0);
        lv_obj_set_style_bg_color(btn_tab_sota, theme_color(COLOR_BG_PANEL), 0); 
        lv_obj_set_style_border_side(btn_tab_sota, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_add_event_cb(btn_tab_sota, [](lv_event_t*){ set_tab(TAB_SOTA); }, LV_EVENT_CLICKED, nullptr);
        lbl_tab_sota = lv_label_create(btn_tab_sota);
        lv_label_set_text(lbl_tab_sota, "SOTA SPOTS");
        lv_obj_set_style_text_font(lbl_tab_sota, &font_atkinson_14, 0); 
        lv_obj_center(lbl_tab_sota);

        lv_obj_t* head = lv_obj_create(scr);
        lv_obj_set_size(head, 320, 26);
        lv_obj_align(head, LV_ALIGN_TOP_MID, 0, 24);
        lv_obj_set_style_bg_color(head, theme_color(COLOR_BG_PANEL), 0); 
        lv_obj_set_style_border_side(head, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(head, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(head, 1, 0);
        lv_obj_set_style_pad_all(head, 0, 0);
        lv_obj_clear_flag(head, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* l1 = lv_label_create(head); lv_label_set_text(l1, "TIME"); lv_obj_set_style_text_font(l1, &font_jetbrains_10, 0); lv_obj_set_style_text_color(l1, theme_color(COLOR_TEXT_MUTED), 0); lv_obj_align(l1, LV_ALIGN_LEFT_MID, 4, 0);
        lbl_hdr_ref = lv_label_create(head); lv_label_set_text(lbl_hdr_ref, "PARK"); lv_obj_set_style_text_font(lbl_hdr_ref, &font_jetbrains_10, 0); lv_obj_set_style_text_color(lbl_hdr_ref, theme_color(COLOR_TEXT_MUTED), 0); lv_obj_align(lbl_hdr_ref, LV_ALIGN_LEFT_MID, 40, 0);
        
        btn_freq = lv_button_create(head); lv_obj_set_size(btn_freq, 36, 20); lv_obj_align(btn_freq, LV_ALIGN_LEFT_MID, 110, 0); 
        lv_obj_set_style_bg_color(btn_freq, theme_color(COLOR_BG_APP), 0); lv_obj_set_style_border_color(btn_freq, theme_color(COLOR_BORDER), 0); lv_obj_set_style_border_width(btn_freq, 1, 0); lv_obj_set_style_radius(btn_freq, 3, 0); lv_obj_set_style_pad_all(btn_freq, 0, 0);
        lv_obj_add_event_cb(btn_freq, cycle_cb, LV_EVENT_CLICKED, (void*)1);
        lv_obj_t* l3 = lv_label_create(btn_freq); lv_label_set_text(l3, BANDS[active_band]); lv_obj_set_style_text_font(l3, &font_jetbrains_10, 0); lv_obj_set_style_text_color(l3, theme_color(COLOR_TEXT_MAIN), 0); lv_obj_center(l3);
        
        btn_mode = lv_button_create(head); lv_obj_set_size(btn_mode, 35, 20); lv_obj_align(btn_mode, LV_ALIGN_LEFT_MID, 155, 0); 
        lv_obj_set_style_bg_color(btn_mode, theme_color(COLOR_BG_APP), 0); lv_obj_set_style_border_color(btn_mode, theme_color(COLOR_BORDER), 0); lv_obj_set_style_border_width(btn_mode, 1, 0); lv_obj_set_style_radius(btn_mode, 3, 0); lv_obj_set_style_pad_all(btn_mode, 0, 0);
        lv_obj_add_event_cb(btn_mode, cycle_cb, LV_EVENT_CLICKED, (void*)2);
        lv_obj_t* l4 = lv_label_create(btn_mode); lv_label_set_text(l4, MODES[active_mode]); lv_obj_set_style_text_font(l4, &font_jetbrains_10, 0); lv_obj_set_style_text_color(l4, theme_color(COLOR_TEXT_MAIN), 0); lv_obj_center(l4);

        lv_obj_t* l5 = lv_label_create(head); lv_label_set_text(l5, "ACT"); lv_obj_set_style_text_font(l5, &font_jetbrains_10, 0); lv_obj_set_style_text_color(l5, theme_color(COLOR_TEXT_MUTED), 0); lv_obj_align(l5, LV_ALIGN_LEFT_MID, 205, 0);
        
        btn_qrp = lv_button_create(head); lv_obj_set_size(btn_qrp, 20, 20); lv_obj_align(btn_qrp, LV_ALIGN_LEFT_MID, 290, 0); 
        lv_obj_set_style_bg_color(btn_qrp, theme_color(COLOR_BG_APP), 0); lv_obj_set_style_border_color(btn_qrp, theme_color(COLOR_BORDER), 0); lv_obj_set_style_border_width(btn_qrp, 1, 0); lv_obj_set_style_radius(btn_qrp, 3, 0); lv_obj_set_style_pad_all(btn_qrp, 0, 0);
        lv_obj_add_event_cb(btn_qrp, cycle_cb, LV_EVENT_CLICKED, (void*)3);
        lv_obj_t* l6 = lv_label_create(btn_qrp); lv_label_set_text(l6, QRPS[active_qrp]); lv_obj_set_style_text_font(l6, &font_jetbrains_10, 0); lv_obj_set_style_text_color(l6, theme_color(COLOR_TEXT_MAIN), 0); lv_obj_center(l6);

        lv_obj_t* foot = lv_obj_create(scr);
        lv_obj_set_size(foot, 320, 24);
        lv_obj_align(foot, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(foot, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_side(foot, LV_BORDER_SIDE_TOP, 0);
        lv_obj_set_style_border_color(foot, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(foot, 1, 0);
        lv_obj_clear_flag(foot, LV_OBJ_FLAG_SCROLLABLE);
        
        lbl_comment = lv_label_create(foot);
        lv_label_set_text(lbl_comment, "INITIALIZING...");
        lv_obj_set_style_text_font(lbl_comment, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_comment, theme_color(COLOR_TEXT_MUTED), 0);
        lv_obj_align(lbl_comment, LV_ALIGN_LEFT_MID, 8, 0);

        lv_obj_t* btn_refresh = lv_button_create(foot);
        lv_obj_set_size(btn_refresh, 24, 20);
        lv_obj_align(btn_refresh, LV_ALIGN_RIGHT_MID, -2, 0);
        lv_obj_set_style_bg_color(btn_refresh, theme_color(COLOR_BG_APP), 0);
        lv_obj_set_style_border_color(btn_refresh, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(btn_refresh, 1, 0);
        lv_obj_set_style_radius(btn_refresh, 3, 0);
        lv_obj_set_style_pad_all(btn_refresh, 0, 0);
        lv_obj_add_event_cb(btn_refresh, [](lv_event_t*){
            if (delayed_fetch_timer) return; 
            
            if (active_tab == TAB_POTA) services::PotaManager::expire_timer();
            else services::SotaManager::expire_timer();
            
            set_tab(active_tab);
        }, LV_EVENT_CLICKED, nullptr);
        lv_obj_t* lbl_ref = lv_label_create(btn_refresh);
        lv_label_set_text(lbl_ref, LV_SYMBOL_REFRESH);
        lv_obj_set_style_text_color(lbl_ref, theme_color(COLOR_TEXT_MAIN), 0);
        lv_obj_center(lbl_ref);

        list_container = lv_obj_create(scr);
        lv_obj_set_size(list_container, 320, 166); 
        lv_obj_align(list_container, LV_ALIGN_TOP_MID, 0, 50);
        lv_obj_set_style_bg_opa(list_container, 0, 0);
        lv_obj_set_style_border_width(list_container, 0, 0);
        lv_obj_set_style_pad_all(list_container, 0, 0);
        lv_obj_set_flex_flow(list_container, LV_FLEX_FLOW_COLUMN);
        
        lv_obj_add_flag(list_container, LV_OBJ_FLAG_HIDDEN);

        lbl_loading = lv_label_create(scr);
        lv_obj_set_style_text_font(lbl_loading, &font_atkinson_14, 0); // Crisp 14px text element
        lv_obj_set_style_text_color(lbl_loading, theme_color(COLOR_ACCENT_PRIMARY), 0);
        lv_obj_set_style_text_align(lbl_loading, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(lbl_loading, "SYNCING...\nSecuring network stream connection.");
        lv_obj_align(lbl_loading, LV_ALIGN_CENTER, 0, 10);
        lv_obj_add_flag(lbl_loading, LV_OBJ_FLAG_HIDDEN);

        for(int i=0; i<MAX_UI_ROWS; i++) {
            lv_obj_t* r = lv_obj_create(list_container);
            lv_obj_set_size(r, 320, 20);
            
            lv_color_t bg_c = (i % 2 == 0) ? theme_color(COLOR_BG_PANEL) : theme_color(COLOR_BG_APP);
            lv_obj_set_style_bg_color(r, bg_c, 0);
            
            lv_obj_set_style_border_width(r, 0, 0);
            lv_obj_set_style_radius(r, 0, 0);
            lv_obj_set_style_pad_all(r, 0, 0);
            lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(r, LV_OBJ_FLAG_HIDDEN);

            lv_obj_add_event_cb(r, row_click, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            
            rows[i].base = r;
            
            rows[i].l_time = lv_label_create(r); 
            lv_obj_set_style_text_font(rows[i].l_time, &font_jetbrains_10, 0); 
            lv_obj_set_style_text_color(rows[i].l_time, theme_color(COLOR_TEXT_MAIN), 0);
            lv_obj_align(rows[i].l_time, LV_ALIGN_LEFT_MID, 4, 0);
            
            rows[i].l_ref = lv_label_create(r); 
            lv_obj_set_style_text_font(rows[i].l_ref, &font_jetbrains_10, 0); 
            lv_obj_set_style_text_color(rows[i].l_ref, theme_color(COLOR_ACCENT_PRIMARY), 0); 
            lv_obj_align(rows[i].l_ref, LV_ALIGN_LEFT_MID, 40, 0);
            
            rows[i].l_freq = lv_label_create(r); 
            lv_obj_set_style_text_font(rows[i].l_freq, &font_jetbrains_10, 0); 
            lv_obj_set_style_text_color(rows[i].l_freq, theme_color(COLOR_TEXT_MAIN), 0);
            lv_obj_align(rows[i].l_freq, LV_ALIGN_LEFT_MID, 110, 0);
            
            rows[i].l_mode = lv_label_create(r); 
            lv_obj_set_style_text_font(rows[i].l_mode, &font_jetbrains_10, 0); 
            lv_obj_set_style_text_color(rows[i].l_mode, theme_color(COLOR_TEXT_MAIN), 0);
            lv_obj_align(rows[i].l_mode, LV_ALIGN_LEFT_MID, 160, 0);
            
            rows[i].l_act = lv_label_create(r); 
            lv_obj_set_style_text_font(rows[i].l_act, &font_jetbrains_10, 0); 
            lv_obj_set_style_text_color(rows[i].l_act, theme_color(COLOR_TEXT_MAIN), 0);
            lv_obj_align(rows[i].l_act, LV_ALIGN_LEFT_MID, 205, 0);
            
            rows[i].l_q = lv_label_create(r); 
            lv_obj_set_style_text_font(rows[i].l_q, &font_jetbrains_10, 0); 
            lv_obj_align(rows[i].l_q, LV_ALIGN_LEFT_MID, 290, 0);
        }

        lv_obj_add_event_cb(scr, [](lv_event_t*){
            if(ui_timer) { lv_timer_delete(ui_timer); ui_timer = nullptr; }
            if(delayed_fetch_timer) { lv_timer_delete(delayed_fetch_timer); delayed_fetch_timer = nullptr; }
            if(status_dot) { lv_obj_delete(status_dot); status_dot = nullptr; }
            scr = list_container = lbl_comment = lbl_loading = btn_freq = btn_mode = btn_qrp = lbl_tab_pota = lbl_tab_sota = lbl_hdr_ref = btn_tab_pota = btn_tab_sota = nullptr;
            
            if (rows) {
                free(rows);
                rows = nullptr;
            }

            xTaskCreate(async_resume_task, "resume_task", 2048, NULL, 1, NULL);
            
        }, LV_EVENT_DELETE, NULL);

        // Force LVGL to physically draw the initial canvas and the big loading label
        lv_timer_handler();

        // Queue the initial dynamic fetch sequence
        delayed_fetch_timer = lv_timer_create(execute_delayed_fetch, 100, NULL);

        ui_timer = lv_timer_create(update_ui, 300, NULL);
    }
}