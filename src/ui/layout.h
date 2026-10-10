#pragma once

// Screen geometry of the 2.8" CYD (ILI9341, landscape), shared by every page (review 4.3).
namespace ui {
    constexpr int SCREEN_W = 320;
    constexpr int SCREEN_H = 240;
    constexpr int STATUS_BAR_H = 24;  // top bar shown above most pages
    constexpr int CONTENT_H = SCREEN_H - STATUS_BAR_H;  // page area below the status bar (216)
}  // namespace ui
