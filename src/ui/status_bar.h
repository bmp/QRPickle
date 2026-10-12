#pragma once
#include <lvgl.h>

namespace ui {

    struct StatusBarCallbacks {
        void (*on_menu)();
        void (*on_settings)();
        void (*on_network)();
    };

    // Generates the top diagnostics panel inside the parent screen instance
    lv_obj_t* status_bar_create(lv_obj_t* parent, StatusBarCallbacks cb);

    // Thread-safe text setters to change screen names dynamically
    void status_bar_set_title(const char* title);
    void status_bar_set_clock(const char* hhmm);
    void status_bar_set_wifi(bool connected);
    void status_bar_set_update_available(bool available);
    void status_bar_refresh_theme();
    // Screen-cycling icon: 0 off (hidden), 1 running, 2/3 paused (ui::cycle::State).
    void status_bar_set_cycle(int state);

}  // namespace ui
