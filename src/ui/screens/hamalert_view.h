#pragma once
#include <lvgl.h>

namespace ui {
    /**
     * @brief Constructs the high-visibility, theme-compliant HamAlert Tactical display screen.
     * @param parent The target view container hardware vector mapping workspace.
     */
    void draw_hamalert_page(lv_obj_t* parent);
}