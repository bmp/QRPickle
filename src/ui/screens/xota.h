#pragma once
#include <lvgl.h>

namespace ui {
    void draw_xota_page(lv_obj_t* parent);
    // Screen cycling: open on the POTA or SOTA tab as a cycle visit, which shows the spots already
    // held and keeps DX connected; it refreshes only when the data is old (ui/xota_fetch.h:
    // POTA 5 min, SOTA 10 min; a manual visit uses 30 s for POTA).
    void xota_open_for_cycle(bool sota);
}