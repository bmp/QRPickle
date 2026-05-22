#include "dashboard.h"
#include "../widgets/widget_clock.h"
#include "../widgets/widget_wx_all.h"
#include "../widgets/widget_band.h"

namespace ui {

    void draw_dashboard_page(lv_obj_t* parent) {
        lv_obj_t* clock_widget = widget_clock_create(parent, WIDGET_SIZE_QUARTER);
        lv_obj_align(clock_widget, LV_ALIGN_TOP_LEFT, 4, 4);

        lv_obj_t* wx_all_widget = widget_wx_all_create(parent, WIDGET_SIZE_QUARTER);
        lv_obj_align(wx_all_widget, LV_ALIGN_TOP_LEFT, 4, 112); 

        lv_obj_t* band_widget = widget_band_create(parent, WIDGET_SIZE_HALF_VERT);
        lv_obj_align(band_widget, LV_ALIGN_TOP_RIGHT, -4, 4);
    }

} // namespace ui