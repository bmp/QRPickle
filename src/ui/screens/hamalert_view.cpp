#include "hamalert_view.h"
#include "../theme.h"
#include "../fonts.h"
#include "../ui.h"
#include "../../services/hamalert_manager.h"
#include <cstdio>
#include <cstring>
#include <Arduino.h>

namespace ui {

    static lv_obj_t* scroll_box = nullptr;
    static lv_timer_t* refresh_timer = nullptr;

    static void update_hamalert_ui_cb(lv_timer_t* t) {
        // FIXED: Force an initial render path draw step even if the dirty bits aren't set yet
        bool initial_draw = (t == nullptr);
        if (!scroll_box) return;
        if (!initial_draw && !services::HamAlertManager::is_dirty()) return;

        Serial.printf("[HamAlert-UI] Executing scene viewport draw update. Total cached logs trace targets count: %d\n", 
                      (int)services::HamAlertManager::get_message_count());

        lv_obj_clean(scroll_box);
        const auto* alerts = services::HamAlertManager::get_messages();
        size_t count = services::HamAlertManager::get_message_count();

        if (count == 0) {
            lv_obj_t* empty_lbl = lv_label_create(scroll_box);
            lv_label_set_text(empty_lbl, "Awaiting prioritized spot alerts from stream...");
            lv_obj_set_style_text_font(empty_lbl, &font_jetbrains_10, 0);
            lv_obj_set_style_text_color(empty_lbl, theme_color(COLOR_TEXT_MUTED), 0);
            lv_obj_align(empty_lbl, LV_ALIGN_CENTER, 0, 20);
            return;
        }

        for (int i = (int)count - 1; i >= 0; i--) {
            lv_obj_t* card = lv_obj_create(scroll_box);
            lv_obj_set_size(card, 292, 48); // Shrunk slightly to avoid scrolling collision bars
            lv_obj_set_style_bg_color(card, theme_color(COLOR_BG_PANEL), 0);
            lv_obj_set_style_border_color(card, theme_color(COLOR_BORDER), 0);
            lv_obj_set_style_border_width(card, 1, 0);
            lv_obj_set_style_radius(card, 4, 0);
            lv_obj_set_style_pad_all(card, 3, 0);
            lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

            lv_color_t badge_bg = theme_color(COLOR_TEXT_MUTED);
            if (strcmp(alerts[i].type, "SOTA") == 0) badge_bg = theme_color(COLOR_BAND_GOOD);
            else if (strcmp(alerts[i].type, "POTA") == 0) badge_bg = theme_color(COLOR_ACCENT_PRIMARY);
            else if (strcmp(alerts[i].type, "DXCC") == 0) badge_bg = theme_color(COLOR_BAND_FAIR);

            lv_obj_t* badge = lv_obj_create(card);
            lv_obj_set_size(badge, 42, 18);
            lv_obj_align(badge, LV_ALIGN_TOP_LEFT, 2, 2);
            lv_obj_set_style_bg_color(badge, badge_bg, 0);
            lv_obj_set_style_border_width(badge, 0, 0);
            lv_obj_set_style_radius(badge, 2, 0);
            lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t* lbl_b = lv_label_create(badge);
            lv_label_set_text(lbl_b, alerts[i].type);
            lv_obj_set_style_text_font(lbl_b, &font_jetbrains_10, 0);
            lv_obj_set_style_text_color(lbl_b, lv_color_hex(0x000000), 0);
            lv_obj_center(lbl_b);

            lv_obj_t* l_main = lv_label_create(card);
            char main_buf[64];
            snprintf(main_buf, sizeof(main_buf), "%s  -  %s MHz  [%s]", 
                     alerts[i].time, alerts[i].freq, alerts[i].mode);
            lv_label_set_text(l_main, main_buf);
            lv_obj_set_style_text_font(l_main, &font_jetbrains_10, 0);
            lv_obj_set_style_text_color(l_main, theme_color(COLOR_TEXT_MUTED), 0);
            lv_obj_align(l_main, LV_ALIGN_TOP_LEFT, 52, 4);

            lv_obj_t* l_sub = lv_label_create(card);
            char sub_buf[64];
            snprintf(sub_buf, sizeof(sub_buf), "%s  ->  %s", alerts[i].callsign, alerts[i].info);
            lv_label_set_text(l_sub, sub_buf);
            lv_obj_set_style_text_font(l_sub, &font_jetbrains_14, 0);
            lv_obj_set_style_text_color(l_sub, theme_color(COLOR_TEXT_MAIN), 0);
            lv_obj_align(l_sub, LV_ALIGN_BOTTOM_LEFT, 4, -2);
        }

        services::HamAlertManager::clear_dirty();
    }

    void draw_hamalert_page(lv_obj_t* parent) {
        Serial.println("[HamAlert-UI] Rendering base scene panel window components wrapper layer.");
        
        scroll_box = lv_obj_create(parent);
        lv_obj_set_size(scroll_box, 320, 216); 
        lv_obj_set_style_bg_color(scroll_box, theme_color(COLOR_BG_APP), 0);
        lv_obj_set_style_border_width(scroll_box, 0, 0);
        lv_obj_set_style_pad_all(scroll_box, 4, 0);
        lv_obj_set_flex_flow(scroll_box, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(scroll_box, 6, 0);
        lv_obj_add_flag(scroll_box, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_add_event_cb(scroll_box, [](lv_event_t*){
            if (refresh_timer) { lv_timer_delete(refresh_timer); refresh_timer = nullptr; }
            scroll_box = nullptr;
        }, LV_EVENT_DELETE, nullptr);

        services::HamAlertManager::start(); 
        
        refresh_timer = lv_timer_create(update_hamalert_ui_cb, 500, nullptr);
        update_hamalert_ui_cb(nullptr); // Force explicit synchronous first draw pass right now
    }
} // namespace ui