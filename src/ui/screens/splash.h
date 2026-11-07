#pragma once
#include <lvgl.h>

namespace ui {
    // Renders the splash view across the frame and invokes the callback upon termination
    void draw_splash_screen(lv_obj_t* parent, void (*on_complete)());
    // The splash over the current page for ms (or until tapped), with the current logos; main loop
    // only. False when a preview is already showing.
    bool splash_preview_show(uint32_t ms);
    bool splash_preview_active();
}
