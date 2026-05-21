#pragma once
#include <lvgl.h>
#include "../ui.h" // Injected to pull your project's standardized type size enums (e.g. WIDGET_SIZE_QUARTER)

namespace ui {
    // FIXED: Matches your dashboard's standardized size signature requirements
    lv_obj_t* widget_wx_all_create(lv_obj_t* parent, uint8_t size_type);
    void widget_wx_all_refresh();
}
