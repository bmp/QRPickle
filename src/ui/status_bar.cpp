#include "status_bar.h"
#include "theme.h"
#include "fonts.h" // FIXED: Imported your project's custom font definitions
#include <lvgl.h>

namespace ui {

    static lv_obj_t* bar = nullptr;
    static lv_obj_t* lbl_clock = nullptr;
    static lv_obj_t* lbl_title = nullptr;
    static lv_obj_t* btn_menu = nullptr;
    static lv_obj_t* btn_gear = nullptr;
    static lv_obj_t* lbl_wifi = nullptr;
    static lv_obj_t* lbl_wifi_x = nullptr;
    static StatusBarCallbacks callbacks{};

    static void menu_clicked(lv_event_t* e) { if (callbacks.on_menu) callbacks.on_menu(); }
    static void gear_clicked(lv_event_t* e) { if (callbacks.on_settings) callbacks.on_settings(); }

    lv_obj_t* status_bar_create(lv_obj_t* parent, StatusBarCallbacks cb) {
        callbacks = cb;

        bar = lv_obj_create(parent);
        lv_obj_set_size(bar, 320, 24);
        lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_set_style_radius(bar, 0, 0);
        lv_obj_set_style_pad_all(bar, 2, 0);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

        btn_menu = lv_label_create(bar);
        lv_obj_set_style_text_font(btn_menu, &font_atkinson_14, 0); // FIXED
        lv_label_set_text(btn_menu, LV_SYMBOL_LIST);
        lv_obj_align(btn_menu, LV_ALIGN_LEFT_MID, 4, 0);
        lv_obj_add_flag(btn_menu, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(btn_menu, 15);
        lv_obj_add_event_cb(btn_menu, menu_clicked, LV_EVENT_CLICKED, NULL);

        lbl_clock = lv_label_create(bar);
        // FIXED: Using monospaced JetBrains so changing time numbers do not cause structural jitter
        lv_obj_set_style_text_font(lbl_clock, &font_jetbrains_14, 0);
        lv_label_set_text(lbl_clock, "--:--");
        lv_obj_align(lbl_clock, LV_ALIGN_LEFT_MID, 28, 0);

        lbl_title = lv_label_create(bar);
        lv_obj_set_style_text_font(lbl_title, &font_atkinson_14, 0); // FIXED
        lv_label_set_text(lbl_title, "Dashboard");
        lv_obj_align(lbl_title, LV_ALIGN_CENTER, 0, 0);

        lbl_wifi = lv_label_create(bar);
        lv_obj_set_style_text_font(lbl_wifi, &font_atkinson_14, 0); // FIXED
        lv_label_set_text(lbl_wifi, LV_SYMBOL_WIFI);
        lv_obj_align(lbl_wifi, LV_ALIGN_RIGHT_MID, -28, 0);
        lv_obj_add_flag(lbl_wifi, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(lbl_wifi, 15);
        lv_obj_add_event_cb(lbl_wifi, [](lv_event_t* e) { if (callbacks.on_network) callbacks.on_network(); }, LV_EVENT_CLICKED, NULL);

        lbl_wifi_x = lv_label_create(bar);
        lv_obj_set_style_text_font(lbl_wifi_x, &font_atkinson_14, 0); // FIXED
        lv_label_set_text(lbl_wifi_x, LV_SYMBOL_CLOSE);
        lv_obj_align_to(lbl_wifi_x, lbl_wifi, LV_ALIGN_CENTER, 0, 0);
        lv_obj_clear_flag(lbl_wifi_x, LV_OBJ_FLAG_CLICKABLE);

        btn_gear = lv_label_create(bar);
        lv_obj_set_style_text_font(btn_gear, &font_atkinson_14, 0); // FIXED
        lv_label_set_text(btn_gear, LV_SYMBOL_SETTINGS);
        lv_obj_align(btn_gear, LV_ALIGN_RIGHT_MID, -4, 0);
        lv_obj_add_flag(btn_gear, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(btn_gear, 15);
        lv_obj_add_event_cb(btn_gear, gear_clicked, LV_EVENT_CLICKED, NULL);

        status_bar_refresh_theme();
        return bar;
    }

    void status_bar_refresh_theme() {
        if (!bar) return;
        lv_obj_set_style_bg_color(bar, theme_color(COLOR_BG_BAR), 0);
        lv_obj_set_style_text_color(btn_menu, theme_color(COLOR_TEXT_MAIN), 0);
        lv_obj_set_style_text_color(lbl_clock, theme_color(COLOR_TEXT_MAIN), 0);
        lv_obj_set_style_text_color(lbl_title, theme_color(COLOR_ACCENT_PRIMARY), 0);
        lv_obj_set_style_text_color(lbl_wifi, theme_color(COLOR_TEXT_MAIN), 0);
        lv_obj_set_style_text_color(lbl_wifi_x, theme_color(COLOR_BAND_POOR), 0);
        lv_obj_set_style_text_color(btn_gear, theme_color(COLOR_TEXT_MAIN), 0);
    }

    void status_bar_set_title(const char* title) { if (lbl_title) lv_label_set_text(lbl_title, title); }
    void status_bar_set_clock(const char* hhmm) { if (lbl_clock) lv_label_set_text(lbl_clock, hhmm); }
    void status_bar_set_wifi(bool connected) {
        if (lbl_wifi_x) {
            if (connected) lv_obj_add_flag(lbl_wifi_x, LV_OBJ_FLAG_HIDDEN);
            else           lv_obj_clear_flag(lbl_wifi_x, LV_OBJ_FLAG_HIDDEN);
        }
    }
}
