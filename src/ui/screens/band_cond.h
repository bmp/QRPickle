#pragma once
#include <lvgl.h>

namespace ui {
    /**
     * @brief Creates a full-screen, high-density Space Weather evaluation panel interface.
     * @param parent The target view container pointer.
     */
    void draw_band_cond_page(lv_obj_t* parent);
}