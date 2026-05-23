#include "aprs_msg.h"
#include "../theme.h"
#include "../fonts.h"
#include "../ui.h"
#include "../../services/aprs_manager.h"
#include "../../config/config.h"
#include <cstdio>
#include <cstring>
#include <Arduino.h>

namespace ui {

    static lv_obj_t* page_root = nullptr;
    static lv_obj_t* content_body = nullptr;
    static lv_obj_t* ta_target = nullptr;
    static lv_obj_t* ta_body = nullptr;
    static lv_obj_t* kb_input = nullptr;

    static lv_obj_t* panel_macros = nullptr;
    static lv_obj_t* panel_wizard = nullptr;
    static lv_obj_t* panel_qwerty = nullptr;

    static lv_obj_t* dd_wiz_net = nullptr;
    static lv_obj_t* ta_wiz_ref = nullptr;
    static lv_obj_t* ta_wiz_freq = nullptr;
    static lv_obj_t* dd_wiz_mode = nullptr;
    static lv_obj_t* ta_wiz_cmt = nullptr;

    static lv_obj_t* btn_tab_mac = nullptr;
    static lv_obj_t* btn_tab_wiz = nullptr;
    static lv_obj_t* btn_tab_qwr = nullptr;

    static void switch_messaging_view(int mode_idx) {
        if (!panel_macros || !panel_wizard || !panel_qwerty) return;

        lv_obj_add_flag(panel_macros, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(panel_wizard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(panel_qwerty, LV_OBJ_FLAG_HIDDEN);
        if (kb_input) lv_obj_add_flag(kb_input, LV_OBJ_FLAG_HIDDEN);

        lv_obj_set_style_bg_color(btn_tab_mac, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_bg_color(btn_tab_wiz, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_bg_color(btn_tab_qwr, theme_color(COLOR_BG_PANEL), 0);

        switch (mode_idx) {
            case 0:
                lv_obj_clear_flag(panel_macros, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_style_bg_color(btn_tab_mac, theme_color(COLOR_BG_APP), 0);
                break;
            case 1:
                lv_obj_clear_flag(panel_wizard, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_style_bg_color(btn_tab_wiz, theme_color(COLOR_BG_APP), 0);
                // FIXED: Use the industry standard live target gateway callsigns
                if (dd_wiz_net && ta_target) {
                    uint16_t opt = lv_dropdown_get_selected(dd_wiz_net);
                    lv_textarea_set_text(ta_target, (opt == 0) ? "APSPOT" : "SOTA");
                }
                break;
            case 2:
                lv_obj_clear_flag(panel_qwerty, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_style_bg_color(btn_tab_qwr, theme_color(COLOR_BG_APP), 0);
                if (kb_input && ta_body) {
                    lv_obj_clear_flag(kb_input, LV_OBJ_FLAG_HIDDEN);
                    lv_keyboard_set_textarea(kb_input, ta_body);
                }
                break;
        }
    }

    static void cb_back_click(lv_event_t* e) {
        ui_navigate_local(PAGE_APRS);
    }

    static void cb_send_click(lv_event_t* e) {
        const char* target = lv_textarea_get_text(ta_target);
        
        if (!lv_obj_has_flag(panel_wizard, LV_OBJ_FLAG_HIDDEN)) {
            char wiz_payload[80];
            char mode_str[12];
            lv_dropdown_get_selected_str(dd_wiz_mode, mode_str, sizeof(mode_str));
            uint16_t opt = lv_dropdown_get_selected(dd_wiz_net);

            // FIXED: Structure rigid syntax patterns based on official gateway specs
            if (opt == 0) {
                // POTA via APSPOT target expects: ! POTA [REF] [FREQ] [MODE] [COMMENT]
                snprintf(wiz_payload, sizeof(wiz_payload), "! POTA %s %s %s %s",
                         lv_textarea_get_text(ta_wiz_ref),
                         lv_textarea_get_text(ta_wiz_freq),
                         mode_str,
                         lv_textarea_get_text(ta_wiz_cmt));
            } else {
                // SOTA via SOTA target expects: [REF] [FREQ] [MODE] [COMMENT]
                snprintf(wiz_payload, sizeof(wiz_payload), "%s %s %s %s",
                         lv_textarea_get_text(ta_wiz_ref),
                         lv_textarea_get_text(ta_wiz_freq),
                         mode_str,
                         lv_textarea_get_text(ta_wiz_cmt));
            }
                     
            services::AprsManager::send_message(target, wiz_payload);
        } else {
            const char* msg = lv_textarea_get_text(ta_body);
            if (strlen(target) > 0 && strlen(msg) > 0) {
                services::AprsManager::send_message(target, msg);
            }
        }
        
        ui_navigate_local(PAGE_APRS);
    }

    static void cb_macro_select(lv_event_t* e) {
        const char* mac_text = (const char*)lv_event_get_user_data(e);
        if (ta_body) {
            lv_textarea_set_text(ta_body, mac_text);
            switch_messaging_view(2); 
        }
    }

    static void cb_wizard_net_changed(lv_event_t* e) {
        uint16_t opt = lv_dropdown_get_selected(dd_wiz_net);
        lv_textarea_set_text(ta_target, (opt == 0) ? "APSPOT" : "SOTA");
    }

    void draw_aprs_msg_page(lv_obj_t* parent) {
        auto& cfg = config::get();

        page_root = lv_obj_create(parent);
        lv_obj_set_size(page_root, 320, 240);
        lv_obj_set_style_bg_color(page_root, theme_color(COLOR_BG_APP), 0);
        lv_obj_set_style_border_width(page_root, 0, 0);
        lv_obj_set_style_pad_all(page_root, 0, 0);
        lv_obj_clear_flag(page_root, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* header = lv_obj_create(page_root);
        lv_obj_set_size(header, 320, 35);
        lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(header, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(header, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(header, 1, 0);
        lv_obj_set_style_pad_all(header, 3, 0);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* btn_back = lv_button_create(header);
        lv_obj_set_size(btn_back, 50, 28);
        lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 2, 0);
        lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x222222), 0);
        lv_obj_set_style_radius(btn_back, 3, 0);
        lv_obj_add_event_cb(btn_back, cb_back_click, LV_EVENT_CLICKED, nullptr);
        
        lv_obj_t* lbl_back = lv_label_create(btn_back);
        lv_label_set_text(lbl_back, "BACK");
        lv_obj_set_style_text_font(lbl_back, &font_jetbrains_10, 0);
        lv_obj_center(lbl_back);

        lv_obj_t* lbl_to = lv_label_create(header);
        lv_label_set_text(lbl_to, "TO:");
        lv_obj_set_style_text_font(lbl_to, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_to, theme_color(COLOR_TEXT_MUTED), 0);
        lv_obj_align(lbl_to, LV_ALIGN_LEFT_MID, 60, 0);

        ta_target = lv_textarea_create(header);
        lv_obj_set_size(ta_target, 95, 26);
        lv_obj_align(ta_target, LV_ALIGN_LEFT_MID, 82, 0);
        lv_textarea_set_one_line(ta_target, true);
        lv_textarea_set_max_length(ta_target, 9);
        lv_textarea_set_text(ta_target, "APSPOT");
        lv_obj_set_style_text_font(ta_target, &font_jetbrains_10, 0);
        lv_obj_set_style_bg_color(ta_target, lv_color_hex(0x111111), 0);
        lv_obj_set_style_border_color(ta_target, theme_color(COLOR_BORDER), 0);
        lv_obj_add_event_cb(ta_target, [](lv_event_t* e){
            if(kb_input && ta_target) {
                lv_obj_clear_flag(kb_input, LV_OBJ_FLAG_HIDDEN);
                lv_keyboard_set_textarea(kb_input, ta_target);
            }
        }, LV_EVENT_FOCUSED, nullptr);

        lv_obj_t* btn_send = lv_button_create(header);
        lv_obj_set_size(btn_send, 55, 28);
        lv_obj_align(btn_send, LV_ALIGN_RIGHT_MID, -2, 0);
        lv_obj_set_style_bg_color(btn_send, theme_color(COLOR_ACCENT_PRIMARY), 0);
        lv_obj_set_style_radius(btn_send, 3, 0);
        lv_obj_add_event_cb(btn_send, cb_send_click, LV_EVENT_CLICKED, nullptr);

        lv_obj_t* lbl_send = lv_label_create(btn_send);
        lv_label_set_text(lbl_send, "SEND");
        lv_obj_set_style_text_font(lbl_send, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_send, lv_color_hex(0x000000), 0);
        lv_obj_center(lbl_send);

        content_body = lv_obj_create(page_root);
        lv_obj_set_size(content_body, 320, 173);
        lv_obj_align(content_body, LV_ALIGN_TOP_MID, 0, 35);
        lv_obj_set_style_bg_opa(content_body, 0, 0);
        lv_obj_set_style_border_width(content_body, 0, 0);
        lv_obj_set_style_pad_all(content_body, 0, 0);
        lv_obj_clear_flag(content_body, LV_OBJ_FLAG_SCROLLABLE);

        panel_macros = lv_obj_create(content_body);
        lv_obj_set_size(panel_macros, 320, 173);
        lv_obj_align(panel_macros, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_opa(panel_macros, 0, 0);
        lv_obj_set_style_border_width(panel_macros, 0, 0);
        lv_obj_set_style_pad_all(panel_macros, 4, 0);
        lv_obj_set_flex_flow(panel_macros, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(panel_macros, 4, 0);
        lv_obj_add_flag(panel_macros, LV_OBJ_FLAG_SCROLLABLE);

        for (int i = 0; i < 5; i++) {
            if (strlen(cfg.aprs_macros[i]) == 0) continue;

            lv_obj_t* row_mac = lv_button_create(panel_macros);
            lv_obj_set_size(row_mac, 312, 28);
            lv_obj_set_style_bg_color(row_mac, theme_color(COLOR_BG_PANEL), 0);
            lv_obj_set_style_border_color(row_mac, theme_color(COLOR_BORDER), 0);
            lv_obj_set_style_border_width(row_mac, 1, 0);
            lv_obj_set_style_radius(row_mac, 2, 0);
            lv_obj_set_style_pad_all(row_mac, 4, 0);
            lv_obj_add_event_cb(row_mac, cb_macro_select, LV_EVENT_CLICKED, (void*)cfg.aprs_macros[i]);

            lv_obj_t* lbl_m = lv_label_create(row_mac);
            lv_label_set_text(lbl_m, cfg.aprs_macros[i]);
            lv_obj_set_style_text_font(lbl_m, &font_jetbrains_10, 0);
            lv_obj_set_style_text_color(lbl_m, theme_color(COLOR_TEXT_MAIN), 0);
            lv_obj_align(lbl_m, LV_ALIGN_LEFT_MID, 4, 0);
        }

        panel_wizard = lv_obj_create(content_body);
        lv_obj_set_size(panel_wizard, 320, 173);
        lv_obj_align(panel_wizard, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_opa(panel_wizard, 0, 0);
        lv_obj_set_style_border_width(panel_wizard, 0, 0);
        lv_obj_set_style_pad_all(panel_wizard, 6, 0);
        lv_obj_clear_flag(panel_wizard, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* l_wnet = lv_label_create(panel_wizard);
        lv_label_set_text(l_wnet, "TARGET:");
        lv_obj_set_style_text_font(l_wnet, &font_jetbrains_10, 0);
        lv_obj_align(l_wnet, LV_ALIGN_TOP_LEFT, 4, 8);

        dd_wiz_net = lv_dropdown_create(panel_wizard);
        lv_obj_set_size(dd_wiz_net, 100, 26);
        lv_obj_align(dd_wiz_net, LV_ALIGN_TOP_LEFT, 65, 2);
        lv_dropdown_set_options(dd_wiz_net, "POTA\nSOTA");
        lv_obj_set_style_text_font(dd_wiz_net, &font_jetbrains_10, 0);
        lv_obj_add_event_cb(dd_wiz_net, cb_wizard_net_changed, LV_EVENT_VALUE_CHANGED, nullptr);

        lv_obj_t* l_wref = lv_label_create(panel_wizard);
        lv_label_set_text(l_wref, "REF CO:");
        lv_obj_set_style_text_font(l_wref, &font_jetbrains_10, 0);
        lv_obj_align(l_wref, LV_ALIGN_TOP_LEFT, 4, 38);

        ta_wiz_ref = lv_textarea_create(panel_wizard);
        lv_obj_set_size(ta_wiz_ref, 100, 26);
        lv_obj_align(ta_wiz_ref, LV_ALIGN_TOP_LEFT, 65, 32);
        lv_textarea_set_one_line(ta_wiz_ref, true);
        lv_textarea_set_text(ta_wiz_ref, "K-1234");
        lv_obj_set_style_text_font(ta_wiz_ref, &font_jetbrains_10, 0);
        lv_obj_add_event_cb(ta_wiz_ref, [](lv_event_t* e){
            if(kb_input && ta_wiz_ref) {
                lv_obj_clear_flag(kb_input, LV_OBJ_FLAG_HIDDEN);
                lv_keyboard_set_textarea(kb_input, ta_wiz_ref);
            }
        }, LV_EVENT_FOCUSED, nullptr);

        lv_obj_t* l_wfq = lv_label_create(panel_wizard);
        lv_label_set_text(l_wfq, "FREQ:");
        lv_obj_set_style_text_font(l_wfq, &font_jetbrains_10, 0);
        lv_obj_align(l_wfq, LV_ALIGN_TOP_LEFT, 4, 68);

        ta_wiz_freq = lv_textarea_create(panel_wizard);
        lv_obj_set_size(ta_wiz_freq, 100, 26);
        lv_obj_align(ta_wiz_freq, LV_ALIGN_TOP_LEFT, 65, 62);
        lv_textarea_set_one_line(ta_wiz_freq, true);
        lv_textarea_set_text(ta_wiz_freq, "14.300");
        lv_obj_set_style_text_font(ta_wiz_freq, &font_jetbrains_10, 0);
        lv_obj_add_event_cb(ta_wiz_freq, [](lv_event_t* e){
            if(kb_input && ta_wiz_freq) {
                lv_obj_clear_flag(kb_input, LV_OBJ_FLAG_HIDDEN);
                lv_keyboard_set_textarea(kb_input, ta_wiz_freq);
            }
        }, LV_EVENT_FOCUSED, nullptr);

        lv_obj_t* l_wmd = lv_label_create(panel_wizard);
        lv_label_set_text(l_wmd, "MODE:");
        lv_obj_set_style_text_font(l_wmd, &font_jetbrains_10, 0);
        lv_obj_align(l_wmd, LV_ALIGN_TOP_LEFT, 175, 8);

        dd_wiz_mode = lv_dropdown_create(panel_wizard);
        lv_obj_set_size(dd_wiz_mode, 75, 26);
        lv_obj_align(dd_wiz_mode, LV_ALIGN_TOP_LEFT, 230, 2);
        lv_dropdown_set_options(dd_wiz_mode, "SSB\nCW\nFT8\nFM\nAM");
        lv_obj_set_style_text_font(dd_wiz_mode, &font_jetbrains_10, 0);

        lv_obj_t* l_wcm = lv_label_create(panel_wizard);
        lv_label_set_text(l_wcm, "CMT:");
        lv_obj_set_style_text_font(l_wcm, &font_jetbrains_10, 0);
        lv_obj_align(l_wcm, LV_ALIGN_TOP_LEFT, 4, 98);

        ta_wiz_cmt = lv_textarea_create(panel_wizard);
        lv_obj_set_size(ta_wiz_cmt, 240, 26);
        lv_obj_align(ta_wiz_cmt, LV_ALIGN_TOP_LEFT, 65, 92);
        lv_textarea_set_one_line(ta_wiz_cmt, true);
        lv_textarea_set_max_length(ta_wiz_cmt, 24);
        lv_textarea_set_text(ta_wiz_cmt, "QRP 5W"); // FIXED: Dropped the word "TEST" from default comments
        lv_obj_set_style_text_font(ta_wiz_cmt, &font_jetbrains_10, 0);
        lv_obj_add_event_cb(ta_wiz_cmt, [](lv_event_t* e){
            if(kb_input && ta_wiz_cmt) {
                lv_obj_clear_flag(kb_input, LV_OBJ_FLAG_HIDDEN);
                lv_keyboard_set_textarea(kb_input, ta_wiz_cmt);
            }
        }, LV_EVENT_FOCUSED, nullptr);

        panel_qwerty = lv_obj_create(content_body);
        lv_obj_set_size(panel_qwerty, 320, 173);
        lv_obj_align(panel_qwerty, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_opa(panel_qwerty, 0, 0);
        lv_obj_set_style_border_width(panel_qwerty, 0, 0);
        lv_obj_set_style_pad_all(panel_qwerty, 4, 0);
        lv_obj_clear_flag(panel_qwerty, LV_OBJ_FLAG_SCROLLABLE);

        ta_body = lv_textarea_create(panel_qwerty);
        lv_obj_set_size(ta_body, 312, 45);
        lv_obj_align(ta_body, LV_ALIGN_TOP_MID, 0, 0);
        lv_textarea_set_max_length(ta_body, 60);
        lv_textarea_set_text(ta_body, "");
        lv_obj_set_style_text_font(ta_body, &font_jetbrains_10, 0);
        lv_obj_set_style_bg_color(ta_body, lv_color_hex(0x050505), 0);
        lv_obj_set_style_border_color(ta_body, theme_color(COLOR_BORDER), 0);
        lv_obj_add_event_cb(ta_body, [](lv_event_t* e){
            if(kb_input && ta_body) {
                lv_obj_clear_flag(kb_input, LV_OBJ_FLAG_HIDDEN);
                lv_keyboard_set_textarea(kb_input, ta_body);
            }
        }, LV_EVENT_FOCUSED, nullptr);

        kb_input = lv_keyboard_create(page_root);
        lv_obj_set_size(kb_input, 320, 93);
        lv_obj_align(kb_input, LV_ALIGN_BOTTOM_MID, 0, -32);
        lv_keyboard_set_mode(kb_input, LV_KEYBOARD_MODE_TEXT_LOWER);
        lv_obj_add_flag(kb_input, LV_OBJ_FLAG_HIDDEN);

        lv_obj_t* footer_nav = lv_obj_create(page_root);
        lv_obj_set_size(footer_nav, 320, 32);
        lv_obj_align(footer_nav, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(footer_nav, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_side(footer_nav, LV_BORDER_SIDE_TOP, 0);
        lv_obj_set_style_border_color(footer_nav, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(footer_nav, 1, 0);
        lv_obj_set_style_pad_all(footer_nav, 0, 0);
        lv_obj_clear_flag(footer_nav, LV_OBJ_FLAG_SCROLLABLE);

        btn_tab_mac = lv_button_create(footer_nav);
        lv_obj_set_size(btn_tab_mac, 106, 30);
        lv_obj_align(btn_tab_mac, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_style_radius(btn_tab_mac, 0, 0);
        lv_obj_add_event_cb(btn_tab_mac, [](lv_event_t*){ switch_messaging_view(0); }, LV_EVENT_CLICKED, nullptr);
        
        lv_obj_t* lbl_t1 = lv_label_create(btn_tab_mac);
        lv_label_set_text(lbl_t1, "MACROS");
        lv_obj_set_style_text_font(lbl_t1, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_t1, theme_color(COLOR_TEXT_MAIN), 0);
        lv_obj_center(lbl_t1);

        btn_tab_wiz = lv_button_create(footer_nav);
        lv_obj_set_size(btn_tab_wiz, 108, 30);
        lv_obj_align(btn_tab_wiz, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_radius(btn_tab_wiz, 0, 0);
        lv_obj_add_event_cb(btn_tab_wiz, [](lv_event_t*){ switch_messaging_view(1); }, LV_EVENT_CLICKED, nullptr);

        lv_obj_t* lbl_t2 = lv_label_create(btn_tab_wiz);
        lv_label_set_text(lbl_t2, "SPOT WIZ");
        lv_obj_set_style_text_font(lbl_t2, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_t2, theme_color(COLOR_TEXT_MAIN), 0);
        lv_obj_center(lbl_t2);

        btn_tab_qwr = lv_button_create(footer_nav);
        lv_obj_set_size(btn_tab_qwr, 106, 30);
        lv_obj_align(btn_tab_qwr, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_style_radius(btn_tab_qwr, 0, 0);
        lv_obj_add_event_cb(btn_tab_qwr, [](lv_event_t*){ switch_messaging_view(2); }, LV_EVENT_CLICKED, nullptr);

        lv_obj_t* lbl_t3 = lv_label_create(btn_tab_qwr);
        lv_label_set_text(lbl_t3, "CUSTOM");
        lv_obj_set_style_text_font(lbl_t3, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_t3, theme_color(COLOR_TEXT_MAIN), 0);
        lv_obj_center(lbl_t3);

        lv_obj_add_event_cb(page_root, [](lv_event_t* e) {
            page_root = content_body = ta_target = ta_body = kb_input = nullptr;
            panel_macros = panel_wizard = panel_qwerty = nullptr;
            dd_wiz_net = ta_wiz_ref = ta_wiz_freq = dd_wiz_mode = ta_wiz_cmt = nullptr;
            btn_tab_mac = btn_tab_wiz = btn_tab_qwr = nullptr;
        }, LV_EVENT_DELETE, nullptr);

        switch_messaging_view(0);
    }
}