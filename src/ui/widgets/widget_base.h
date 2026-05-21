#pragma once
#include <lvgl.h>

namespace ui {

    enum WidgetSize {
        WIDGET_SIZE_FULL,       // 320 x 216 (Takes up the whole screen)
        WIDGET_SIZE_HALF_VERT,  // 160 x 216 (Takes up the left or right half)
        WIDGET_SIZE_QUARTER     // 160 x 108 (Takes up one of four quadrants)
    };

} // namespace ui
