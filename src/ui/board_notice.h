#pragma once
// The board on the CYD's own screen (v0.3.2, approved mockups 1-4): confirm the detected board once,
// warn when the user's choice contradicts the screen, ask untested boards for a report (once per
// firmware version) and show the report link as a QR code. Everything runs on the main loop.
#include <lvgl.h>

namespace ui {
    namespace board_notice {

        // After the splash: shows whichever of the dialogs the board selection asks for.
        void start();

        // Full-screen QR code of the short report link; any tap closes it.
        void show_report_code();

        // Network screen: a "Board" line with [Report] at y, only for boards that aren't tested (so a
        // tested board's screen stays as it was). Returns the height used (0 when nothing was added).
        int add_network_line(lv_obj_t* page, int y);

    }  // namespace board_notice
}  // namespace ui
