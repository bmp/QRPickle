#pragma once
#include <lvgl.h>

namespace ui {

    enum DestScreen {
        DEST_DASHBOARD,
        DEST_SETTINGS,
        DEST_NETWORK,
        DEST_SPOTS,
        DEST_POTA,
        DEST_PROP,
        DEST_WEATHER
    };

    struct SidebarCallbacks {
        void (*on_select)(DestScreen);
    };

    // Structural interface routines to control menu visibility state transitions
    void sidebar_init(lv_obj_t* parent, SidebarCallbacks cb);
    void sidebar_show();
    void sidebar_hide();
    bool sidebar_is_visible();
    void sidebar_toggle();
    void sidebar_rebuild();

}  // namespace ui
