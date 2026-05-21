#pragma once
#include <lvgl.h>
#include "widget_base.h"

namespace ui {
    lv_obj_t* widget_band_create(lv_obj_t* parent, WidgetSize size);
}
