#pragma once
#include <lvgl.h>
#include "widget_base.h"

namespace ui {

    // Spawns an isolated clock container. Returns the container pointer.
    lv_obj_t* widget_clock_create(lv_obj_t* parent, WidgetSize size);

} // namespace ui
