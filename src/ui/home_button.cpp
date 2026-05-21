#include "home_button.h"

namespace ui {

static void clicked(lv_event_t* e) {
    auto fn = (void (*)())lv_event_get_user_data(e);
    if (fn) fn();
}

lv_obj_t* home_button_create(lv_obj_t* parent, void (*on_tap)()) {
    // Generate a simple clickable text element utilizing built-in vector graphics icons
    lv_obj_t* btn = lv_label_create(parent);
    lv_label_set_text(btn, LV_SYMBOL_HOME);
    lv_obj_set_style_text_color(btn, lv_color_hex(0xE6EDF3), 0); // Light contrast font
    lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, -8, -8);           // Pin precisely into the bottom-right corner
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, clicked, LV_EVENT_CLICKED, (void*)on_tap);

    return btn;
}

}  // namespace ui
