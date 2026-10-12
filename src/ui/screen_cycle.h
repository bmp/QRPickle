#pragma once
#include <stdint.h>

// Screen cycling (docs/UI_GUIDE.md): shows the screens chosen in the settings one after another.
// - A touch pauses it on the current screen; it resumes after RESUME_AFTER_MS without a touch.
// - The status-bar icon pauses/resumes it until tapped again.
// - It waits while the backlight sleeps, Settings or the menu is open.
// Runs on the main loop (LVGL timer), like every UI module.
namespace ui {
    namespace cycle {

        enum Screen : uint8_t { DASHBOARD,
                                BAND,
                                WEATHER,
                                DX,
                                POTA,
                                SOTA,
                                APRS_RADAR,
                                SCREEN_COUNT };
        constexpr uint32_t RESUME_AFTER_MS = 120000;

        enum class State : uint8_t { OFF,
                                     RUNNING,
                                     PAUSED_TOUCH,
                                     PAUSED_USER };

        void init();  // after the UI shell exists
        State state();
        void toggle_pause();  // status-bar icon
        // While cycling is on and the DX screen is part of it, leaving the DX screen keeps the
        // cluster connected (no new login every round).
        bool keeps_dx_connected();

    }  // namespace cycle
}  // namespace ui
