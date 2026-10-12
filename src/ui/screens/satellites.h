#pragma once
#include <lvgl.h>

namespace ui {
    // Satellites screen: the next passes over the station, ISS SSTV events and school contacts,
    // and a short guide (docs/UI_GUIDE.md).
    void draw_satellites_page(lv_obj_t* parent);
}
