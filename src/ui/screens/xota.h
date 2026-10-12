#pragma once
#include <lvgl.h>

namespace ui {
    void draw_xota_page(lv_obj_t* parent);
    // Screen cycling: open on the POTA or SOTA tab as a passive visit, which shows the spots
    // already held and pauses nothing (no quiet window, DX stays connected); it refreshes only
    // when the data is old (POTA 5 min, SOTA 10 min).
    void xota_open_for_cycle(bool sota);
}