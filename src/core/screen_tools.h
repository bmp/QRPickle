#pragma once
#include <stddef.h>
#include <stdint.h>

// Screenshot support for tools/device_screens.py. Compiled only with -DQRP_SCREEN_TOOLS (the
// cyd-screens environment), never in release builds. Screenshots travel over WiFi in 20-row bands
// (12.8 KB each; there is no RAM for a whole 150 KB frame): the tool asks for band n, the main loop
// redraws just those rows and the flush hook copies them into a buffer, which the tool downloads.
// Requests come from the web server (any task); rendering runs on the main loop (LVGL isn't
// thread-safe).
namespace screen_tools {

    constexpr int BAND_ROWS = 20;
    constexpr int BANDS = 240 / BAND_ROWS;
    constexpr size_t BAND_BYTES = 320 * BAND_ROWS * 2;  // RGB565, little-endian

    // Open a page (ui::LocalPage) with a theme (0..THEME_ID_MAX, -1 = keep) without saving.
    void request_page(int page, int theme);
    // Render band n (0..BANDS-1) into the buffer. False if the buffer can't be allocated.
    bool request_band(int band);
    // The rendered band, or nullptr while band n isn't ready yet.
    const uint8_t* band_data(int band);
    // Main loop: performs pending requests.
    void update();
    // Display flush hook: copies the rows of the band being captured.
    void on_flush(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const uint8_t* px);

}  // namespace screen_tools
