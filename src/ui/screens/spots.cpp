#include "spots.h"
#include "../theme.h"
#include "../fonts.h"
#include "../../services/dx_manager.h"
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

    static int active_band_idx = 0; 
    static const char* BANDS[] = {"ALL", "160m", "80m", "40m", "20m", "15m", "10m", "6m"};
    
    static int active_mode_idx = 0; 
    static const char* MODES[] = {"ALL", "CW", "SSB", "FT8", "FT4"};

    struct RowWidgets {
        lv_obj_t* base_row;
        lv_obj_t* lbl_call;
        lv_obj_t* lbl_freq;
        lv_obj_t* lbl_mode;
        lv_obj_t* lbl_spotter;
        lv_obj_t* lbl_cmt;
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
        active_band_idx = (active_band_idx + 1) % (sizeof(BANDS) / sizeof(BANDS[0]));
        char buf[16];
        snprintf(buf, sizeof(buf), "FREQ: %s", BANDS[active_band_idx]);
        lv_label_set_text(lv_obj_get_child(btn_freq, 0), buf);
        services::DxManager::update(); // force check instantly
    }

    static void filter_mode_cb(lv_event_t* e) {
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
        if (status_dot) {
            if (current_status == services::DX_STATUS_CONNECTED) {
                lv_obj_set_style_bg_color(status_dot, lv_color_hex(0x00FF00), 0);
            } else if (current_status == services::DX_STATUS_DISCONNECTED) {
                lv_obj_set_style_bg_color(status_dot, lv_color_hex(0xFF0000), 0);
            } else {
                lv_obj_set_style_bg_color(status_dot, lv_color_hex(0xFFFF00), 0);
            }
        }

        if (!services::DxManager::is_dirty()) return;

        const services::DxSpot* raw_spots = services::DxManager::get_spots();
        size_t total_raw = services::DxManager::get_spot_count();
        size_t match_count = 0;

        memset(displayed_spots, 0, sizeof(displayed_spots));

        if (!raw_spots) {
            for (size_t i = 0; i < 20; i++) {
                if (row_pool[i].base_row) lv_obj_add_flag(row_pool[i].base_row, LV_OBJ_FLAG_HIDDEN);
            }
            if (lbl_comment) lv_label_set_text(lbl_comment, "STATUS: DX Cluster buffer empty.");
            return;
        }

        // Suspend rendering frames during mass layout updates for smoother performance
        lv_obj_add_flag(list_container, LV_OBJ_FLAG_HIDDEN);

        for (size_t i = 0; i < total_raw && match_count < 20; i++) {
            if (match_band(raw_spots[i].freq, active_band_idx) && match_mode(raw_spots[i].mode, active_mode_idx)) {
                displayed_spots[match_count] = &raw_spots[i];

                RowWidgets& rw = row_pool[match_count];
                char buf[16];

                lv_label_set_text(rw.lbl_call, raw_spots[i].dx_call);

                snprintf(buf, sizeof(buf), "%.3f", raw_spots[i].freq);
                lv_label_set_text(rw.lbl_freq, buf);

                lv_label_set_text(rw.lbl_mode, raw_spots[i].mode);
                lv_label_set_text(rw.lbl_spotter, raw_spots[i].spotter);
                lv_label_set_text(rw.lbl_cmt, raw_spots[i].comment);

                lv_obj_clear_flag(rw.base_row, LV_OBJ_FLAG_HIDDEN);
                match_count++;
            }
        }

        for (size_t i = match_count; i < 20; i++) {
            if (row_pool[i].base_row) {
                lv_obj_add_flag(row_pool[i].base_row, LV_OBJ_FLAG_HIDDEN);
            }
        }

        lv_obj_clear_flag(list_container, LV_OBJ_FLAG_HIDDEN);

        // --- NEW UX LOGIC: Dynamic Loading Status ---
        if (lbl_comment) {
            if (current_status == services::DX_STATUS_DISCONNECTED) {
                lv_label_set_text(lbl_comment, "OFFLINE: Lost connection to DX Cluster node.");
            } else if (current_status == services::DX_STATUS_CONNECTING) {
                lv_label_set_text(lbl_comment, "SYNCING: Negotiating Telnet link with cluster...");
            } else if (match_count == 0) {
                lv_label_set_text(lbl_comment, "WAITING: No spots received yet. Listening...");
            } else {
                lv_label_set_text(lbl_comment, "COMMENT: Tap any row to read full string.");
            }
        }

        services::DxManager::clear_dirty();
    }

    void draw_spots_page(lv_obj_t* parent) {
        scr = lv_obj_create(parent);
        lv_obj_set_size(scr, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_color(scr, theme_color(COLOR_BG_APP), 0);
        lv_obj_set_style_border_width(scr, 0, 0);
        lv_obj_set_style_pad_all(scr, 0, 0);
        lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

        // FIXED: Shifted overlay position completely clear of text layout bounds
        status_dot = lv_obj_create(lv_layer_top());
        lv_obj_set_size(status_dot, 6, 6);
        lv_obj_set_pos(status_dot, 118, 9); 
        lv_obj_set_style_radius(status_dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(status_dot, 0, 0);
        lv_obj_set_style_bg_color(status_dot, lv_color_hex(0xFF0000), 0);

        lv_obj_t* header_bar = lv_obj_create(scr);
        lv_obj_set_size(header_bar, 320, 26);
        lv_obj_align(header_bar, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(header_bar, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_side(header_bar, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(header_bar, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(header_bar, 1, 0);
        lv_obj_set_style_pad_all(header_bar, 0, 0);
        lv_obj_clear_flag(header_bar, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* l_call = lv_label_create(header_bar);
        lv_label_set_text(l_call, "CALL");
        lv_obj_set_style_text_font(l_call, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(l_call, theme_color(COLOR_TEXT_MUTED), 0);
        lv_obj_align(l_call, LV_ALIGN_LEFT_MID, 6, 0);

        btn_freq = lv_button_create(header_bar);
        lv_obj_set_size(btn_freq, 76, 20);
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
        lv_obj_set_size(btn_mode, 72, 20);
        lv_obj_align(btn_mode, LV_ALIGN_LEFT_MID, 142, 0);
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
        lv_obj_align(l_spot, LV_ALIGN_LEFT_MID, 218, 0);

        lv_obj_t* l_cmt = lv_label_create(header_bar);
        lv_label_set_text(l_cmt, "CMT");
        lv_obj_set_style_text_font(l_cmt, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(l_cmt, theme_color(COLOR_TEXT_MUTED), 0);
        lv_obj_align(l_cmt, LV_ALIGN_LEFT_MID, 274, 0);

        lv_obj_t* footer = lv_obj_create(scr);
        lv_obj_set_size(footer, 320, 24);
        lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(footer, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_side(footer, LV_BORDER_SIDE_TOP, 0);
        lv_obj_set_style_border_color(footer, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(footer, 1, 0);
        lv_obj_set_style_pad_all(footer, 0, 0);
        lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

        lbl_comment = lv_label_create(footer);
        lv_label_set_text(lbl_comment, "COMMENT: Tap any row to read full string.");
        lv_obj_set_style_text_font(lbl_comment, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_comment, theme_color(COLOR_TEXT_MUTED), 0);
        lv_obj_align(lbl_comment, LV_ALIGN_LEFT_MID, 8, 0);

        list_container = lv_obj_create(scr);
        lv_obj_set_size(list_container, 320, 158); 
        lv_obj_align(list_container, LV_ALIGN_TOP_MID, 0, 26);
        lv_obj_set_style_bg_opa(list_container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(list_container, 0, 0);
        lv_obj_set_style_pad_all(list_container, 0, 0);
        lv_obj_set_flex_flow(list_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(list_container, 1, 0);
        lv_obj_add_flag(list_container, LV_OBJ_FLAG_SCROLLABLE);

        for (int i = 0; i < 20; i++) {
            lv_obj_t* row = lv_obj_create(list_container);
            lv_obj_set_size(row, 320, 21);
            lv_obj_set_style_bg_color(row, i % 2 == 0 ? theme_color(COLOR_BG_PANEL) : lv_color_hex(0x000000), 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_radius(row, 0, 0);
            lv_obj_set_style_pad_all(row, 0, 0);
            lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN); 
            
            lv_obj_add_event_cb(row, row_clicked_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            row_pool[i].base_row = row;

            row_pool[i].lbl_call = lv_label_create(row);
            lv_obj_set_style_text_font(row_pool[i].lbl_call, &font_jetbrains_10, 0);
            lv_obj_set_style_text_color(row_pool[i].lbl_call, theme_color(COLOR_TEXT_MAIN), 0);
            lv_obj_align(row_pool[i].lbl_call, LV_ALIGN_LEFT_MID, 6, 0);

            row_pool[i].lbl_freq = lv_label_create(row);
            lv_obj_set_style_text_font(row_pool[i].lbl_freq, &font_jetbrains_10, 0);
            lv_obj_set_style_text_color(row_pool[i].lbl_freq, theme_color(COLOR_ACCENT_PRIMARY), 0);
            lv_obj_set_width(row_pool[i].lbl_freq, 76);
            lv_obj_set_style_text_align(row_pool[i].lbl_freq, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(row_pool[i].lbl_freq, LV_ALIGN_LEFT_MID, 62, 0);

            row_pool[i].lbl_mode = lv_label_create(row);
            lv_obj_set_style_text_font(row_pool[i].lbl_mode, &font_jetbrains_10, 0);
            lv_obj_set_style_text_color(row_pool[i].lbl_mode, theme_color(COLOR_TEXT_MAIN), 0);
            lv_obj_set_width(row_pool[i].lbl_mode, 72);
            lv_obj_set_style_text_align(row_pool[i].lbl_mode, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(row_pool[i].lbl_mode, LV_ALIGN_LEFT_MID, 142, 0);

            row_pool[i].lbl_spotter = lv_label_create(row);
            lv_obj_set_style_text_font(row_pool[i].lbl_spotter, &font_jetbrains_10, 0);
            lv_obj_set_style_text_color(row_pool[i].lbl_spotter, theme_color(COLOR_TEXT_MUTED), 0);
            lv_obj_align(row_pool[i].lbl_spotter, LV_ALIGN_LEFT_MID, 218, 0);

            row_pool[i].lbl_cmt = lv_label_create(row);
            lv_obj_set_style_text_font(row_pool[i].lbl_cmt, &font_jetbrains_10, 0);
            lv_obj_set_style_text_color(row_pool[i].lbl_cmt, theme_color(COLOR_TEXT_MUTED), 0);
            lv_obj_set_width(row_pool[i].lbl_cmt, 42); 
            lv_obj_align(row_pool[i].lbl_cmt, LV_ALIGN_LEFT_MID, 274, 0);
        }

        lv_obj_add_event_cb(scr, [](lv_event_t* e) {
            if (ui_timer) { lv_timer_delete(ui_timer); ui_timer = nullptr; }
            if (status_dot) { lv_obj_delete(status_dot); status_dot = nullptr; }
            
            services::DxManager::stop();
            
            scr = list_container = lbl_comment = btn_freq = btn_mode = nullptr;
            for(int i = 0; i < 20; i++) row_pool[i].base_row = nullptr;
        }, LV_EVENT_DELETE, NULL);

        services::DxManager::start();

        ui_timer = lv_timer_create([](lv_timer_t* t){ update_spots_ui(); }, 250, NULL);
        update_spots_ui();
    }

} // namespace ui
