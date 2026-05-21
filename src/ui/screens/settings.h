#pragma once
#include <lvgl.h>

namespace ui {

struct SettingsCallbacks {
    void (*on_exit)();    // Code execution block to run on exit
    void (*on_reboot)();  // Code execution block to run a hardware restart
};

// Generates the settings scrollable form view, populating fields directly from NVS parameters
lv_obj_t* settings_create(SettingsCallbacks cb);

// Safe teardown routine to clean settings allocations from memory
void settings_destroy();

}  // namespace ui
