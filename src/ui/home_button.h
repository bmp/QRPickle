#pragma once
#include <lvgl.h>

namespace ui {

// Attaches a home icon action token to the bottom-right frame space of the parent container
lv_obj_t* home_button_create(lv_obj_t* parent, void (*on_tap)());

}  // namespace ui
