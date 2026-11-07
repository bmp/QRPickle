#include "spots.h"
#include "../screen_cycle.h"
#include "../layout.h"
#include "../theme.h"
#include "../corner.h"
#include "../fonts.h"
#include "../../services/dx_manager.h"
#include "../../services/dx_status_text.h"
#include <Arduino.h>
#include <lvgl.h>
#include <cstdio>
#include <cstring>

namespace ui {

    static lv_obj_t* scr = nullptr;
    static lv_obj_t* btn_freq = nullptr;
    static lv_obj_t* btn_mode = nullptr;
    static lv_obj_t* list_container = nullptr;
    static lv_obj_t* lbl_comment = nullptr;
    static lv_obj_t* status_dot = nullptr;
    static lv_timer_t* ui_timer = nullptr;
    static bool filters_changed = false;  // set by the FREQ/MODE buttons

    static int active_band_idx = 0; 
    static const char* BANDS[] = {"ALL", "160m", "80m", "40m", "20m", "15m", "10m", "6m"};
    
    static int active_mode_idx = 0; 
    static const char* MODES[] = {"ALL", "CW", "SSB", "FT8", "FT4"};

    // One label per row (services::dx_row_text lines up the columns in the monospace font): five
    // labels per row made this the page with the highest LVGL pool use.
    struct RowWidgets {
        lv_obj_t* base_row;
        lv_obj_t* lbl;
    };
    static RowWidgets row_pool[20];
    static const services::DxSpot* displayed_spots[20]; 

    static bool match_band(float freq, int band_idx) {
        if (band_idx == 0) return true;
        float mhz = (freq > 1000.0f) ? (freq / 1000.0f) : freq;
        switch (band_idx) {
            case 1: return (mhz >= 1.8f && mhz <= 2.0f);
            case 2: return (mhz >= 3.5f && mhz <= 4.0f);
            case 3: return (mhz >= 7.0f && mhz <= 7.3f);
            case 4: return (mhz >= 14.0f && mhz <= 14.35f);
            case 5: return (mhz >= 21.0f && mhz <= 21.45f);
            case 6: return (mhz >= 28.0f && mhz <= 29.7f);
            case 7: return (mhz >= 50.0f && mhz <= 54.0f);
            default: return false;
        }
    }

    static bool match_mode(const char* spot_mode, int mode_idx) {
        if (mode_idx == 0) return true;
        return strcasecmp(spot_mode, MODES[mode_idx]) == 0;
    }

    static void row_clicked_cb(lv_event_t* e) {
        intptr_t index = (intptr_t)lv_event_get_user_data(e);
        if (displayed_spots[index] && lbl_comment) {
            char buf[64];
            snprintf(buf, sizeof(buf), "COMMENT: %s", displayed_spots[index]->comment);
            lv_label_set_text(lbl_comment, buf);
        }
    }

    static void filter_freq_cb(lv_event_t* e) {
        filters_changed = true;
        active_band_idx = (active_band_idx + 1) % (sizeof(BANDS) / sizeof(BANDS[0]));
        char buf[16];
        snprintf(buf, sizeof(buf), "FREQ: %s", BANDS[active_band_idx]);
        lv_label_set_text(lv_obj_get_child(btn_freq, 0), buf);
        services::DxManager::update(); // force check instantly
    }

    static void filter_mode_cb(lv_event_t* e) {
        filters_changed = true;
        active_mode_idx = (active_mode_idx + 1) % (sizeof(MODES) / sizeof(MODES[0]));
        char buf[16];
        snprintf(buf, sizeof(buf), "MODE: %s", MODES[active_mode_idx]);
        lv_label_set_text(lv_obj_get_child(btn_mode, 0), buf);
        services::DxManager::update(); 
    }

    static void update_spots_ui() {
        if (!scr) return;

        // 1. Stream data from the network ring buffer continuously
        services::DxManager::update();

        // 2. Refresh top system status connection indicators
        auto current_status = services::DxManager::get_status();

        // A lost link (cluster restart, silence timeout, lost WiFi) used to stay down until the page
        // was opened again: while the page is open, reconnect every 30 s (docs/DATA_FLOW.md).
        static uint32_t disconnected_since = 0;
        if (current_status != services::DX_STATUS_DISCONNECTED) {
            disconnected_since = 0;
        } else if (!disconnected_since) {
            disconnected_since = millis() | 1;
        } else if (millis() - disconnected_since > 30000) {
            disconnected_since = 0;
            services::DxManager::start();
        }
        if (status_dot) {
            if (current_status == services::DX_STATUS_CONNECTED) {
                lv_obj_set_style_bg_color(status_dot, theme_color(COLOR_STATUS_OK), 0);
            } else if (current_status == services::DX_STATUS_DISCONNECTED) {
                lv_obj_set_style_bg_color(status_dot, theme_color(COLOR_STATUS_ERROR), 0);
            } else {
                lv_obj_set_style_bg_color(status_dot, theme_color(COLOR_STATUS_WARN), 0);
            }
        }

        // The status line follows the connection on every tick (it used to change only with new
        // spots, so after a restart the page said nothing while it waited).
        static services::DxStatus last_status = services::DX_STATUS_DISCONNECTED;
        static size_t last_total = SIZE_MAX, last_shown = SIZE_MAX;
        static size_t shown = 0;

        const bool redraw = services::DxManager::is_dirty() || filters_changed;
        if (redraw) {
            filters_changed = false;
            const services::DxSpot* raw_spots = services::DxManager::get_spots();
            const size_t total_raw = raw_spots ? services::DxManager::get_spot_count() : 0;
            size_t match_count = 0;
            memset(displayed_spots, 0, sizeof(displayed_spots));

            // Suspend rendering frames during mass layout updates for smoother performance
            lv_obj_set_hidden(list_container, true);
            for (size_t i = 0; i < total_raw && match_count < 20; i++) {
                if (match_band(raw_spots[i].freq, active_band_idx) && match_mode(raw_spots[i].mode, active_mode_idx)) {
                    displayed_spots[match_count] = &raw_spots[i];
                    char line[80];
                    services::dx_row_text(raw_spots[i], line, sizeof(line));
                    lv_label_set_text(row_pool[match_count].lbl, line);
                    lv_obj_set_hidden(row_pool[match_count].base_row, false);
                    match_count++;
                }
            }
            for (size_t i = match_count; i < 20; i++) {
                if (row_pool[i].base_row) lv_obj_set_hidden(row_pool[i].base_row, true);
            }
            lv_obj_set_hidden(list_container, false);
            shown = match_count;
            services::DxManager::clear_dirty();
        }

        const size_t total = services::DxManager::get_spots() ? services::DxManager::get_spot_count() : 0;
        if (lbl_comment && (redraw || current_status != last_status || total != last_total || shown != last_shown)) {
            lv_label_set_text(lbl_comment, services::dx_status_line(current_status, total, shown));
            last_status = current_status;
            last_total = total;
            last_shown = shown;
        }
    }

    void draw_spots_page(lv_obj_t* parent) {
        scr = lv_obj_create(parent);
        lv_obj_set_size(scr, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_color(scr, theme_color(COLOR_BG_APP), 0);
        lv_obj_set_style_border_width(scr, 0, 0);
        lv_obj_set_style_pad_all(scr, 0, 0);
        lv_obj_set_scrollable(scr, false);

        // Shifted overlay position completely clear of text layout bounds
        status_dot = lv_obj_create(lv_layer_top());
        lv_obj_set_size(status_dot, 6, 6);
        lv_obj_set_pos(status_dot, 118, 9); 
        lv_obj_set_style_radius(status_dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(status_dot, 0, 0);
        lv_obj_set_style_bg_color(status_dot, theme_color(COLOR_STATUS_ERROR), 0);

        lv_obj_t* header_bar = lv_obj_create(scr);
        lv_obj_set_size(header_bar, SCREEN_W, 26);
        lv_obj_align(header_bar, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(header_bar, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_side(header_bar, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(header_bar, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(header_bar, 1, 0);
        lv_obj_set_style_pad_all(header_bar, 0, 0);
        lv_obj_set_scrollable(header_bar, false);

        lv_obj_t* l_call = lv_label_create(header_bar);
        lv_label_set_text(l_call, "CALL");
        lv_obj_set_style_text_font(l_call, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(l_call, theme_color(COLOR_TEXT_MUTED), 0);
        lv_obj_align(l_call, LV_ALIGN_LEFT_MID, 6, 0);

        btn_freq = lv_button_create(header_bar);
        lv_obj_set_size(btn_freq, 58, 20);  // over the frequency column (x 66, 9 characters)
        lv_obj_align(btn_freq, LV_ALIGN_LEFT_MID, 62, 0);
        lv_obj_set_style_bg_color(btn_freq, theme_color(COLOR_BG_APP), 0);
        lv_obj_set_style_border_color(btn_freq, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(btn_freq, 1, 0);
        lv_obj_set_style_radius(btn_freq, 3, 0);
        lv_obj_set_style_pad_all(btn_freq, 0, 0);
        lv_obj_add_event_cb(btn_freq, filter_freq_cb, LV_EVENT_CLICKED, nullptr);
        
        lv_obj_t* lbl_bf = lv_label_create(btn_freq);
        lv_label_set_text(lbl_bf, "FREQ: ALL");
        lv_obj_set_style_text_font(lbl_bf, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_bf, theme_color(COLOR_TEXT_MAIN), 0);
        lv_obj_center(lbl_bf);

        btn_mode = lv_button_create(header_bar);
        lv_obj_set_size(btn_mode, 58, 20);  // over the mode column (x 126)
        lv_obj_align(btn_mode, LV_ALIGN_LEFT_MID, 124, 0);
        lv_obj_set_style_bg_color(btn_mode, theme_color(COLOR_BG_APP), 0);
        lv_obj_set_style_border_color(btn_mode, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(btn_mode, 1, 0);
        lv_obj_set_style_radius(btn_mode, 3, 0);
        lv_obj_set_style_pad_all(btn_mode, 0, 0);
        lv_obj_add_event_cb(btn_mode, filter_mode_cb, LV_EVENT_CLICKED, nullptr);

        lv_obj_t* lbl_bm = lv_label_create(btn_mode);
        lv_label_set_text(lbl_bm, "MODE: ALL");
        lv_obj_set_style_text_font(lbl_bm, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_bm, theme_color(COLOR_TEXT_MAIN), 0);
        lv_obj_center(lbl_bm);

        lv_obj_t* l_spot = lv_label_create(header_bar);
        lv_label_set_text(l_spot, "SPOTTER");
        lv_obj_set_style_text_font(l_spot, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(l_spot, theme_color(COLOR_TEXT_MUTED), 0);
        lv_obj_align(l_spot, LV_ALIGN_LEFT_MID, 186, 0);

        lv_obj_t* l_cmt = lv_label_create(header_bar);
        lv_label_set_text(l_cmt, "CMT");
        lv_obj_set_style_text_font(l_cmt, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(l_cmt, theme_color(COLOR_TEXT_MUTED), 0);
        lv_obj_align(l_cmt, LV_ALIGN_LEFT_MID, 240, 0);

        lv_obj_t* footer = lv_obj_create(scr);
        lv_obj_set_size(footer, SCREEN_W, 24);
        lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(footer, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_side(footer, LV_BORDER_SIDE_TOP, 0);
        lv_obj_set_style_border_color(footer, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(footer, 1, 0);
        lv_obj_set_style_pad_all(footer, 0, 0);
        lv_obj_set_scrollable(footer, false);

        lbl_comment = lv_label_create(footer);
        lv_label_set_text(lbl_comment, "COMMENT: Tap any row to read full string.");
        lv_obj_set_style_text_font(lbl_comment, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_comment, theme_color(COLOR_TEXT_MUTED), 0);
        lv_obj_align(lbl_comment, LV_ALIGN_LEFT_MID, 8, 0);
        lv_obj_set_size(lbl_comment, CONTENT_RIGHT - 8, 12);  // one line, before the corner (corner.h)
        lv_label_set_long_mode(lbl_comment, LV_LABEL_LONG_DOT);

        list_container = lv_obj_create(scr);
        lv_obj_set_size(list_container, SCREEN_W, 158);
        lv_obj_align(list_container, LV_ALIGN_TOP_MID, 0, 26);
        lv_obj_set_style_bg_opa(list_container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(list_container, 0, 0);
        lv_obj_set_style_pad_all(list_container, 0, 0);
        lv_obj_set_flex_flow(list_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(list_container, 1, 0);
        lv_obj_set_scrollable(list_container, true);

        for (int i = 0; i < 20; i++) {
            lv_obj_t* row = lv_obj_create(list_container);
            lv_obj_set_size(row, SCREEN_W, 21);
            lv_obj_set_style_bg_color(row, i % 2 == 0 ? theme_color(COLOR_BG_PANEL) : theme_color(COLOR_BG_APP), 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_radius(row, 0, 0);
            lv_obj_set_style_pad_all(row, 0, 0);
            lv_obj_set_scrollable(row, false);
            lv_obj_set_clickable(row, true);
            lv_obj_set_hidden(row, true);

            lv_obj_add_event_cb(row, row_clicked_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            row_pool[i].base_row = row;

            row_pool[i].lbl = lv_label_create(row);
            lv_obj_set_style_text_font(row_pool[i].lbl, &font_jetbrains_10, 0);
            lv_obj_set_style_text_color(row_pool[i].lbl, theme_color(COLOR_TEXT_MAIN), 0);
            lv_obj_set_width(row_pool[i].lbl, SCREEN_W - 6);
            lv_label_set_long_mode(row_pool[i].lbl, LV_LABEL_LONG_CLIP);
            lv_obj_align(row_pool[i].lbl, LV_ALIGN_LEFT_MID, 6, 0);
        }

        lv_obj_add_event_cb(scr, [](lv_event_t* e) {
            if (ui_timer) { lv_timer_delete(ui_timer); ui_timer = nullptr; }
            if (status_dot) { lv_obj_delete(status_dot); status_dot = nullptr; }

            // While the cycle shows DX, stay connected (no new login every round); the cycle feeds it.
            if (!cycle::keeps_dx_connected()) services::DxManager::stop();

            scr = list_container = lbl_comment = btn_freq = btn_mode = nullptr;
            for(int i = 0; i < 20; i++) row_pool[i].base_row = nullptr;
        }, LV_EVENT_DELETE, NULL);

        services::DxManager::start();

        ui_timer = lv_timer_create([](lv_timer_t* t){ update_spots_ui(); }, 250, NULL);
        update_spots_ui();
    }

} // namespace ui
