#include "corner.h"
#include "theme.h"

namespace ui {

    lv_obj_t* refresh_button_create(lv_obj_t* parent, lv_event_cb_t cb, void* user_data) {
        lv_obj_t* btn = lv_button_create(parent);
        lv_obj_set_size(btn, REFRESH_W, REFRESH_H);
        lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, REFRESH_X + REFRESH_W - SCREEN_W, -2);
        lv_obj_set_style_bg_color(btn, theme_color(COLOR_BG_BUTTON), 0);
        lv_obj_set_style_border_color(btn, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_radius(btn, 3, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_t* icon = lv_label_create(btn);
        lv_label_set_text(icon, LV_SYMBOL_REFRESH);
        lv_obj_set_style_text_color(icon, theme_color(COLOR_TEXT_MAIN), 0);
        lv_obj_center(icon);
        if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
        return btn;
    }

}  // namespace ui
