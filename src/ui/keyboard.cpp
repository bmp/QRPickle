#include "keyboard.h"
#include "theme.h" // Hook into the centralized theme management token stream
#include <string.h>

namespace ui {

    static lv_obj_t* modal = nullptr;
    static lv_obj_t* ta = nullptr;
    static lv_obj_t* kb = nullptr;
    static KeyboardCallbacks callbacks{};

    static void event_cb(lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);

        if (code == LV_EVENT_READY) {
            if (callbacks.on_accept) callbacks.on_accept(lv_textarea_get_text(ta));
            keyboard_hide();
        } else if (code == LV_EVENT_CANCEL) {
            if (callbacks.on_cancel) callbacks.on_cancel();
            keyboard_hide();
        }
    }

    void keyboard_show(KeyboardMode mode, const char* initial, KeyboardCallbacks cb) {
        if (modal) keyboard_hide();
        callbacks = cb;

        // Fetch our active theme color palette snapshots
        lv_color_t bg_app    = theme_color(COLOR_BG_APP);
        lv_color_t bg_panel  = theme_color(COLOR_BG_PANEL);
        lv_color_t bg_bar    = theme_color(COLOR_BG_BAR);
        lv_color_t border    = theme_color(COLOR_BORDER);
        lv_color_t txt_main  = theme_color(COLOR_TEXT_MAIN);

        // 1. Instantiate full screen modal overlay matching the current app theme background
        modal = lv_obj_create(lv_layer_top());
        lv_obj_set_size(modal, 320, 216);
        lv_obj_align(modal, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(modal, bg_app, 0);
        lv_obj_set_style_border_width(modal, 0, 0);
        lv_obj_set_style_pad_all(modal, 4, 0);
        lv_obj_clear_flag(modal, LV_OBJ_FLAG_SCROLLABLE);

        // 2. Text Area input box framing adjustments
        ta = lv_textarea_create(modal);
        lv_obj_set_size(ta, 312, 38);
        lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 0);
        lv_textarea_set_one_line(ta, true);
        lv_textarea_set_text(ta, initial ? initial : "");

        lv_obj_set_style_bg_color(ta, bg_panel, 0);
        lv_obj_set_style_text_color(ta, txt_main, 0);
        lv_obj_set_style_border_color(ta, border, 0);
        lv_obj_set_style_border_width(ta, 1, 0);

        // 3. Native LVGL Keyboard implementation & target part mapping
        kb = lv_keyboard_create(modal);
        lv_obj_set_size(kb, 312, 160);
        lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_keyboard_set_textarea(kb, ta);
        lv_keyboard_set_popovers(kb, true);

        // THEMED: Apply color variables to the core frame layout of the keyboard
        lv_obj_set_style_bg_color(kb, bg_bar, LV_PART_MAIN);
        lv_obj_set_style_border_color(kb, border, LV_PART_MAIN);
        lv_obj_set_style_border_width(kb, 1, LV_PART_MAIN);
        lv_obj_set_style_pad_all(kb, 4, LV_PART_MAIN);

        // THEMED: Apply uniform colors to the interactive button matrix items
        lv_obj_set_style_bg_color(kb, bg_panel, LV_PART_ITEMS);
        lv_obj_set_style_text_color(kb, txt_main, LV_PART_ITEMS);
        lv_obj_set_style_border_color(kb, border, LV_PART_ITEMS);
        lv_obj_set_style_border_width(kb, 1, LV_PART_ITEMS);
        lv_obj_set_style_radius(kb, 4, LV_PART_ITEMS);

        switch (mode) {
            case KB_CALLSIGN:
                lv_textarea_set_max_length(ta, 11);
                break;
            case KB_GRID:
                lv_textarea_set_max_length(ta, 6);
                break;
            case KB_TEXT:
                lv_textarea_set_max_length(ta, 63);
                break;
        }

        lv_obj_add_event_cb(kb, event_cb, LV_EVENT_READY, nullptr);
        lv_obj_add_event_cb(kb, event_cb, LV_EVENT_CANCEL, nullptr);
    }

    void keyboard_hide() {
        if (modal) {
            lv_obj_delete(modal);
            modal = nullptr;
            ta = nullptr;
            kb = nullptr;
        }
    }

}  // namespace ui
