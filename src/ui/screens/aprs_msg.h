#pragma once
#include <lvgl.h>

namespace ui {
    /**
     * @brief Constructs the full-screen tactical APRS messaging overlay.
     * @param parent The global view container pointer.
     */
    void draw_aprs_msg_page(lv_obj_t* parent);
}