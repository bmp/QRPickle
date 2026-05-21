#pragma once
#include <lvgl.h>

namespace ui {
    void draw_weather_page(lv_obj_t* parent);

    // Spawns full screen multi-tab view operational layers
    lv_obj_t* weather_create();

    // Completely unloads structural elements safely from memory loops on close
    void weather_destroy();
}
