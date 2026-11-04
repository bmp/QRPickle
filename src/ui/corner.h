#pragma once
#include <lvgl.h>
#include "layout.h"

// The bottom-right corner (UI guide, "The corner"): home always at the far right, a page's refresh
// button always just left of it, nothing else in this box on any page (backgrounds and borders may
// pass under). Screen coordinates.
namespace ui {
    constexpr int CORNER_X = 256, CORNER_H = 22;  // box: x 256-319, the bottom 22 px
    constexpr int REFRESH_X = 262, REFRESH_W = 26, REFRESH_H = 18;
    constexpr int CONTENT_RIGHT = CORNER_X - 6;  // footer texts and tiles in the bottom strip end here

    // The page's refresh button at the fixed spot. parent: an object whose bottom-right corner is
    // the screen's (a page root of SCREEN_W x CONTENT_H at the bottom).
    lv_obj_t* refresh_button_create(lv_obj_t* parent, lv_event_cb_t cb, void* user_data = nullptr);
}  // namespace ui
