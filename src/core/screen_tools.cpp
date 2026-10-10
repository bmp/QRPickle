#ifdef QRP_SCREEN_TOOLS
#include "screen_tools.h"
#include "../config/config.h"
#include "../ui/status_bar.h"
#include "../ui/ui.h"
#include <Arduino.h>
#include <atomic>
#include <lvgl.h>
#include <string.h>

namespace screen_tools {

    static std::atomic<int> pending_page{-1};
    static std::atomic<int> pending_theme{-1};
    static std::atomic<int> pending_band{-1};
    static std::atomic<int> ready_band{-1};
    static uint8_t* band_buf = nullptr;
    static int capture_band = -1;  // main loop only

    void request_page(int page, int theme) {
        ready_band = -1;
        pending_theme = theme;
        pending_page = page;
    }

    bool request_band(int band) {
        if (!band_buf) band_buf = static_cast<uint8_t*>(malloc(BAND_BYTES));  // once, kept
        if (!band_buf || band < 0 || band >= BANDS) return false;
        ready_band = -1;
        pending_band = band;
        return true;
    }

    const uint8_t* band_data(int band) { return ready_band == band ? band_buf : nullptr; }

    void on_flush(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const uint8_t* px) {
        if (capture_band < 0) return;
        const int32_t top = capture_band * BAND_ROWS, bottom = top + BAND_ROWS - 1;
        const int32_t w = x2 - x1 + 1;
        for (int32_t y = (y1 > top ? y1 : top); y <= (y2 < bottom ? y2 : bottom); y++) {
            memcpy(band_buf + ((y - top) * 320 + x1) * 2, px + (y - y1) * w * 2, w * 2);
        }
    }

    void update() {
        int page = pending_page.exchange(-1);
        if (page >= 0) {
            int theme = pending_theme.exchange(-1);
            if (theme >= 0) {
                config::mutable_get().theme_id = (uint8_t)theme;  // not saved: preview only
                ui::status_bar_refresh_theme();
            }
            ui::ui_navigate_local(static_cast<ui::LocalPage>(page));
        }
        int band = pending_band.exchange(-1);
        if (band >= 0) {
            lv_area_t area = {0, (int32_t)(band * BAND_ROWS), 319, (int32_t)(band * BAND_ROWS + BAND_ROWS - 1)};
            capture_band = band;
            lv_obj_invalidate_area(lv_screen_active(), &area);
            lv_refr_now(nullptr);  // redraws just these rows; on_flush() copies them
            capture_band = -1;
            ready_band = band;
        }
    }

}  // namespace screen_tools
#endif
