#pragma once
#include <lvgl.h>

namespace ui {
    // Renders the splash view across the frame and invokes the callback upon termination
    void draw_splash_screen(lv_obj_t* parent, void (*on_complete)());
}
