#include "widget_band.h"
#include "../fonts.h"
#include "../theme.h"
#include "../ui.h"
#include "../../services/prop_manager.h"
#include <cstdio>

namespace ui {

    // Static pointers to hold our label references for the background timer
    static lv_timer_t* band_update_timer = nullptr;
    static lv_obj_t* s_solar_lbl = nullptr;
    static lv_obj_t* s_blk_day[4] = {nullptr};
    static lv_obj_t* s_lbl_day[4] = {nullptr};
    static lv_obj_t* s_blk_nt[4] = {nullptr};
    static lv_obj_t* s_lbl_nt[4] = {nullptr};

    // The timer callback that checks the RAM dirty flag every 60 seconds
    static void band_timer_cb(lv_timer_t* timer) {
        if (!services::PropagationManager::get_telemetry().dirty) return; // Costs 0 CPU if no new data

        const auto& tel = services::PropagationManager::get_telemetry();
        
        // Update Solar Text
        if (s_solar_lbl) {
            char sol_buf[32];
            snprintf(sol_buf, sizeof(sol_buf), "SFI: %u   K-INDEX: %u", tel.sfi, tel.k_index);
            lv_label_set_text(s_solar_lbl, sol_buf);
        }

        lv_color_t colors_rating[] = {
            theme_color(COLOR_BAND_POOR),
            theme_color(COLOR_BAND_FAIR),
            theme_color(COLOR_BAND_GOOD)
        };
        const char* text_rating[] = {"P", "F", "G"};

        // Update Grid Blocks
        for (int i = 0; i < 4; i++) {
            services::ConditionRating r_day = services::PropagationManager::get_band_rating(i, false);
            services::ConditionRating r_nt  = services::PropagationManager::get_band_rating(i, true);

            if (s_blk_day[i]) lv_obj_set_style_bg_color(s_blk_day[i], colors_rating[r_day], 0);
            if (s_lbl_day[i]) lv_label_set_text(s_lbl_day[i], text_rating[r_day]);

            if (s_blk_nt[i]) lv_obj_set_style_bg_color(s_blk_nt[i], colors_rating[r_nt], 0);
            if (s_lbl_nt[i]) lv_label_set_text(s_lbl_nt[i], text_rating[r_nt]);
        }

        services::PropagationManager::clear_dirty();
    }

    lv_obj_t* widget_band_create(lv_obj_t* parent, WidgetSize size) {
        lv_obj_t* widget = lv_obj_create(parent);
        lv_obj_clear_flag(widget, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_set_style_bg_color(widget, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_color(widget, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(widget, 1, 0);
        lv_obj_set_style_radius(widget, 6, 0);
        lv_obj_set_style_pad_all(widget, 0, 0);  

        if (size == WIDGET_SIZE_HALF_VERT) {
            lv_obj_set_size(widget, 154, 212);

            lv_obj_add_flag(widget, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(widget, [](lv_event_t*){
                ui_navigate_local(PAGE_BAND_COND);
            }, LV_EVENT_CLICKED, nullptr);

            const auto& tel = services::PropagationManager::get_telemetry();
             
            // Solar Weather Header Readout Summary
            s_solar_lbl = lv_label_create(widget);
            char sol_buf[32];
            snprintf(sol_buf, sizeof(sol_buf), "SFI: %u   K-INDEX: %u", tel.sfi, tel.k_index);
            lv_label_set_text(s_solar_lbl, sol_buf);
            lv_obj_set_style_text_font(s_solar_lbl, &font_jetbrains_10, 0);
            lv_obj_set_style_text_color(s_solar_lbl, theme_color(COLOR_TEXT_MUTED), 0);
            lv_obj_align(s_solar_lbl, LV_ALIGN_TOP_LEFT, 12, 12);

            // Grid Layout Matrix Headers
            lv_obj_t* table_hdr = lv_label_create(widget);
            lv_label_set_text(table_hdr, "BAND       DAY    NT");
            lv_obj_set_style_text_font(table_hdr, &font_jetbrains_10, 0);
            lv_obj_set_style_text_color(table_hdr, theme_color(COLOR_TEXT_MUTED), 0);
            lv_obj_align(table_hdr, LV_ALIGN_TOP_LEFT, 12, 32);

            const char* label_bands[] = {"80m-40m", "30m-20m", "17m-15m", "12m-10m"};
             
            lv_color_t colors_rating[] = {
                theme_color(COLOR_BAND_POOR),
                theme_color(COLOR_BAND_FAIR),
                theme_color(COLOR_BAND_GOOD)
            };
            const char* text_rating[] = {"P", "F", "G"};

            for (int i = 0; i < 4; i++) {
                int y_pos = 54 + (i * 38);

                lv_obj_t* b_name = lv_label_create(widget);
                lv_label_set_text(b_name, label_bands[i]);
                lv_obj_set_style_text_font(b_name, &font_jetbrains_10, 0);
                lv_obj_set_style_text_color(b_name, theme_color(COLOR_TEXT_MAIN), 0);
                lv_obj_align(b_name, LV_ALIGN_TOP_LEFT, 12, y_pos + 4);

                services::ConditionRating r_day = services::PropagationManager::get_band_rating(i, false);
                services::ConditionRating r_nt  = services::PropagationManager::get_band_rating(i, true);

                // Day status block
                s_blk_day[i] = lv_obj_create(widget);
                lv_obj_set_size(s_blk_day[i], 22, 18);
                lv_obj_align(s_blk_day[i], LV_ALIGN_TOP_LEFT, 76, y_pos);
                lv_obj_set_style_bg_color(s_blk_day[i], colors_rating[r_day], 0); 
                lv_obj_set_style_radius(s_blk_day[i], 3, 0);
                lv_obj_set_style_border_width(s_blk_day[i], 0, 0);
                lv_obj_clear_flag(s_blk_day[i], LV_OBJ_FLAG_SCROLLABLE);

                s_lbl_day[i] = lv_label_create(s_blk_day[i]);
                lv_label_set_text(s_lbl_day[i], text_rating[r_day]);
                lv_obj_set_style_text_font(s_lbl_day[i], &font_jetbrains_10, 0);
                lv_obj_set_style_text_color(s_lbl_day[i], lv_color_hex(0x000000), 0);
                lv_obj_center(s_lbl_day[i]);

                // Night status block
                s_blk_nt[i] = lv_obj_create(widget);
                lv_obj_set_size(s_blk_nt[i], 22, 18);
                lv_obj_align(s_blk_nt[i], LV_ALIGN_TOP_LEFT, 112, y_pos);
                lv_obj_set_style_bg_color(s_blk_nt[i], colors_rating[r_nt], 0); 
                lv_obj_set_style_radius(s_blk_nt[i], 3, 0);
                lv_obj_set_style_border_width(s_blk_nt[i], 0, 0);
                lv_obj_clear_flag(s_blk_nt[i], LV_OBJ_FLAG_SCROLLABLE);

                s_lbl_nt[i] = lv_label_create(s_blk_nt[i]);
                lv_label_set_text(s_lbl_nt[i], text_rating[r_nt]);
                lv_obj_set_style_text_font(s_lbl_nt[i], &font_jetbrains_10, 0);
                lv_obj_set_style_text_color(s_lbl_nt[i], lv_color_hex(0x000000), 0);
                lv_obj_center(s_lbl_nt[i]);
            }

            // Create the UI Polling Timer (Checks RAM every 60 seconds)
            band_update_timer = lv_timer_create(band_timer_cb, 60000, nullptr);

            // Cleanup event: Destroy the timer if the user leaves the Dashboard
            lv_obj_add_event_cb(widget, [](lv_event_t* e) {
                if (band_update_timer) {
                    lv_timer_del(band_update_timer);
                    band_update_timer = nullptr;
                }
                // Nullify pointers safely
                s_solar_lbl = nullptr;
                for(int i=0; i<4; i++) {
                    s_blk_day[i] = nullptr; s_lbl_day[i] = nullptr;
                    s_blk_nt[i] = nullptr; s_lbl_nt[i] = nullptr;
                }
            }, LV_EVENT_DELETE, nullptr);
        }

        return widget;
    }

} // namespace ui
