#pragma once
#include <lvgl.h>

namespace ui {

    enum KeyboardMode {
        KB_TEXT,        // Standard alphanumeric with shift and symbols (Wi-Fi, API Keys)
        KB_CALLSIGN,    // Standard alphanumeric (User can capitalize as needed)
        KB_GRID         // Standard alphanumeric
    };

    struct KeyboardCallbacks {
        void (*on_accept)(const char* text);
        void (*on_cancel)();
    };

    // Summons a full-screen native LVGL keyboard modal beneath the status header
    void keyboard_show(KeyboardMode mode, const char* initial, KeyboardCallbacks cb);

    // Destroys internal heap buffers and wipes the keyboard structure safely from memory
    void keyboard_hide();

}  // namespace ui
