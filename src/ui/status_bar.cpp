#include "status_bar.h"
#include "theme.h"
#include "fonts.h"
#include "../services/display_manager.h" 
#include <lvgl.h>
#include <string.h>

namespace ui {

    static lv_obj_t* bg_panel = nullptr;
    static lv_obj_t* btn_menu = nullptr;
    static lv_obj_t* lbl_title = nullptr;
    static lv_obj_t* btn_wifi = nullptr;
    static lv_obj_t* lbl_wifi = nullptr;
    static lv_obj_t* btn_sleep = nullptr; 
    static lv_obj_t* lbl_sleep = nullptr; 
    static lv_obj_t* btn_settings = nullptr;
    static lv_obj_t* lbl_clock = nullptr;
    static StatusBarCallbacks callbacks;

    static void show_sleep_modal() {
        lv_obj_t * mbox = lv_msgbox_create(NULL);
        lv_msgbox_add_title(mbox, "Power Management");
        lv_msgbox_add_text(mbox, "Enter display sleep mode?");
        
        lv_obj_t * btn_ok = lv_msgbox_add_footer_button(mbox, "Sleep");
        lv_obj_t * btn_cancel = lv_msgbox_add_footer_button(mbox, "Cancel");

        // Apply active theme colors
        lv_color_t bg_col = theme_color(COLOR_BG_PANEL);
        lv_color_t text_col = theme_color(COLOR_TEXT_MAIN);
        lv_color_t border_col = theme_color(COLOR_BORDER);
        lv_color_t alert_col = theme_color(COLOR_BAND_POOR);

        // Style the main modal dialog box
        lv_obj_set_style_bg_color(mbox, bg_col, 0);
        lv_obj_set_style_border_color(mbox, border_col, 0);
        lv_obj_set_style_border_width(mbox, 1, 0);
        lv_obj_set_style_text_color(mbox, text_col, 0);

        // Darken the background screen overlay behind the dialog box
        lv_obj_t * obj_bg = lv_obj_get_parent(mbox);
        if (obj_bg) {
            lv_obj_set_style_bg_color(obj_bg, lv_color_hex(0x000000), 0);
            lv_obj_set_style_bg_opa(obj_bg, 180, 0);
        }

        // Style the action buttons
        if (btn_ok) {
            lv_obj_set_style_bg_color(btn_ok, alert_col, 0); // Red "Sleep" button
            lv_obj_set_style_text_color(btn_ok, lv_color_hex(0xFFFFFF), 0);
        }
        if (btn_cancel) {
            lv_obj_set_style_bg_color(btn_cancel, border_col, 0); // Neutral "Cancel" button
            lv_obj_set_style_text_color(btn_cancel, text_col, 0);
        }
        
        lv_obj_add_event_cb(btn_ok, [](lv_event_t * e) {
            lv_obj_t * m = (lv_obj_t *)lv_event_get_user_data(e);
            lv_msgbox_close(m);
            services::display_manager::sleep();
        }, LV_EVENT_CLICKED, mbox);
        
        lv_obj_add_event_cb(btn_cancel, [](lv_event_t * e) {
            lv_obj_t * m = (lv_obj_t *)lv_event_get_user_data(e);
            lv_msgbox_close(m);
        }, LV_EVENT_CLICKED, mbox);
    }

    lv_obj_t* status_bar_create(lv_obj_t* parent, StatusBarCallbacks cb) {
        callbacks = cb;

        bg_panel = lv_obj_create(parent);
        lv_obj_set_size(bg_panel, 320, 24);
        lv_obj_align(bg_panel, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_radius(bg_panel, 0, 0);
        lv_obj_set_style_border_width(bg_panel, 0, 0);
        lv_obj_clear_flag(bg_panel, LV_OBJ_FLAG_SCROLLABLE);

        btn_menu = lv_btn_create(bg_panel);
        lv_obj_set_size(btn_menu, 24, 24);
        lv_obj_align(btn_menu, LV_ALIGN_LEFT_MID, -10, 0);
        lv_obj_set_style_radius(btn_menu, 0, 0);
        lv_obj_add_event_cb(btn_menu, [](lv_event_t*) { if(callbacks.on_menu) callbacks.on_menu(); }, LV_EVENT_CLICKED, NULL);

        lv_obj_t* lbl_menu = lv_label_create(btn_menu);
        lv_label_set_text(lbl_menu, LV_SYMBOL_LIST);
        lv_obj_center(lbl_menu);

        lbl_title = lv_label_create(bg_panel);
        lv_obj_set_style_text_font(lbl_title, &font_atkinson_14, 0);
        lv_label_set_text(lbl_title, "Dashboard");
        lv_obj_align(lbl_title, LV_ALIGN_LEFT_MID, 20, 0);

        lbl_clock = lv_label_create(bg_panel);
        lv_obj_set_style_text_font(lbl_clock, &font_jetbrains_14, 0);
        lv_label_set_text(lbl_clock, "--:--");
        lv_obj_align(lbl_clock, LV_ALIGN_RIGHT_MID, 4, 0);

        btn_settings = lv_btn_create(bg_panel);
        lv_obj_set_size(btn_settings, 24, 24);
        lv_obj_align(btn_settings, LV_ALIGN_RIGHT_MID, -35, 0);
        lv_obj_set_style_radius(btn_settings, 0, 0);
        lv_obj_add_event_cb(btn_settings, [](lv_event_t*) { if(callbacks.on_settings) callbacks.on_settings(); }, LV_EVENT_CLICKED, NULL);

        lv_obj_t* lbl_set = lv_label_create(btn_settings);
        lv_label_set_text(lbl_set, LV_SYMBOL_SETTINGS);
        lv_obj_center(lbl_set);

        btn_wifi = lv_btn_create(bg_panel);
        lv_obj_set_size(btn_wifi, 24, 24);
        lv_obj_align(btn_wifi, LV_ALIGN_RIGHT_MID, -59, 0);
        lv_obj_set_style_radius(btn_wifi, 0, 0);
        lv_obj_add_event_cb(btn_wifi, [](lv_event_t*) { if(callbacks.on_network) callbacks.on_network(); }, LV_EVENT_CLICKED, NULL);

        lbl_wifi = lv_label_create(btn_wifi);
        lv_label_set_text(lbl_wifi, LV_SYMBOL_WIFI);
        lv_obj_center(lbl_wifi);

        btn_sleep = lv_btn_create(bg_panel);
        lv_obj_set_size(btn_sleep, 24, 24);
        lv_obj_align(btn_sleep, LV_ALIGN_RIGHT_MID, -83, 0);
        lv_obj_set_style_radius(btn_sleep, 0, 0);
        lv_obj_add_event_cb(btn_sleep, [](lv_event_t*) { show_sleep_modal(); }, LV_EVENT_CLICKED, NULL);

        lbl_sleep = lv_label_create(btn_sleep);
        // FIXED: Replaced Power symbol with the Eye-Closed symbol for sleep/privacy mode
        lv_label_set_text(lbl_sleep, LV_SYMBOL_EYE_CLOSE);
        lv_obj_center(lbl_sleep);

        status_bar_refresh_theme();
        return bg_panel;
    }

    void status_bar_set_title(const char* title) {
        if (lbl_title) lv_label_set_text(lbl_title, title);
    }

    void status_bar_set_wifi(bool connected) {
        if (lbl_wifi) {
            lv_color_t color = connected ? theme_color(COLOR_BAND_GOOD) : theme_color(COLOR_TEXT_MUTED);
            lv_obj_set_style_text_color(lbl_wifi, color, 0);
        }
    }

    void status_bar_set_clock(const char* time_str) {
        if (lbl_clock) lv_label_set_text(lbl_clock, time_str);
    }

    void status_bar_refresh_theme() {
        if (!bg_panel) return;

        lv_color_t bg_col = theme_color(COLOR_BG_PANEL);
        lv_color_t border_col = theme_color(COLOR_BORDER);
        lv_color_t text_col = theme_color(COLOR_TEXT_MAIN);

        lv_obj_set_style_bg_color(bg_panel, bg_col, 0);
        lv_obj_set_style_border_color(bg_panel, border_col, 0);

        if (btn_menu) lv_obj_set_style_bg_color(btn_menu, bg_col, 0);
        if (btn_settings) lv_obj_set_style_bg_color(btn_settings, bg_col, 0);
        if (btn_wifi) lv_obj_set_style_bg_color(btn_wifi, bg_col, 0);
        if (btn_sleep) lv_obj_set_style_bg_color(btn_sleep, bg_col, 0);

        if (lbl_title) lv_obj_set_style_text_color(lbl_title, text_col, 0);
        if (lbl_clock) lv_obj_set_style_text_color(lbl_clock, text_col, 0);

        lv_obj_t* lbl_menu = lv_obj_get_child(btn_menu, 0);
        if (lbl_menu) lv_obj_set_style_text_color(lbl_menu, text_col, 0);

        lv_obj_t* lbl_set = lv_obj_get_child(btn_settings, 0);
        if (lbl_set) lv_obj_set_style_text_color(lbl_set, text_col, 0);
        
        if (lbl_sleep) lv_obj_set_style_text_color(lbl_sleep, text_col, 0); 
    }

} // namespace ui