#include "widget_band.h"
#include "../fonts.h"
#include "../theme.h"
#include "../ui.h"
#include "../../services/prop_manager.h"
#include <cstdio>

namespace ui {

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

            // Click callback to open detailed view
            lv_obj_add_flag(widget, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(widget, [](lv_event_t*){
                ui_navigate_local(PAGE_BAND_COND);
            }, LV_EVENT_CLICKED, nullptr);

            const auto& tel = services::PropagationManager::get_telemetry();
            
            // Solar Weather Header Readout Summary
            lv_obj_t* solar_lbl = lv_label_create(widget);
            char sol_buf[32];
            snprintf(sol_buf, sizeof(sol_buf), "SFI: %u    K-INDEX: %u", tel.sfi, tel.k_index);
            lv_label_set_text(solar_lbl, sol_buf);
            lv_obj_set_style_text_font(solar_lbl, &font_jetbrains_10, 0);
            lv_obj_set_style_text_color(solar_lbl, theme_color(COLOR_TEXT_MUTED), 0);
            lv_obj_align(solar_lbl, LV_ALIGN_TOP_LEFT, 12, 12);

            // Grid Layout Matrix Headers
            lv_obj_t* table_hdr = lv_label_create(widget);
            lv_label_set_text(table_hdr, "BAND      DAY  NT");
            lv_obj_set_style_text_font(table_hdr, &font_jetbrains_10, 0);
            lv_obj_set_style_text_color(table_hdr, theme_color(COLOR_TEXT_MUTED), 0);
            lv_obj_align(table_hdr, LV_ALIGN_TOP_LEFT, 12, 32);

            const char* label_bands[] = {"80m-40m", "30m-20m", "17m-15m", "12m-10m"};
            
            // SYSTEM THEME COMPLIANCE BOUND COLOR VARIABLE ASSIGNMENTS
            lv_color_t colors_rating[] = {
                theme_color(COLOR_BAND_POOR),
                theme_color(COLOR_BAND_FAIR),
                theme_color(COLOR_BAND_GOOD)
            };
            const char* text_rating[] = {"P", "F", "G"};

            for (int i = 0; i < 4; i++) {
                int y_pos = 54 + (i * 38);

                // Frequency band group tag line
                lv_obj_t* b_name = lv_label_create(widget);
                lv_label_set_text(b_name, label_bands[i]);
                lv_obj_set_style_text_font(b_name, &font_jetbrains_10, 0);
                lv_obj_set_style_text_color(b_name, theme_color(COLOR_TEXT_MAIN), 0);
                lv_obj_align(b_name, LV_ALIGN_TOP_LEFT, 12, y_pos + 4);

                services::ConditionRating r_day = services::PropagationManager::get_band_rating(i, false);
                services::ConditionRating r_nt  = services::PropagationManager::get_band_rating(i, true);

                // Day status block indicator layout geometry
                lv_obj_t* blk_day = lv_obj_create(widget);
                lv_obj_set_size(blk_day, 22, 18);
                lv_obj_align(blk_day, LV_ALIGN_TOP_LEFT, 76, y_pos);
                lv_obj_set_style_bg_color(blk_day, colors_rating[r_day], 0); // FIXED PARENTHESIS
                lv_obj_set_style_radius(blk_day, 3, 0);
                lv_obj_set_style_border_width(blk_day, 0, 0);
                lv_obj_clear_flag(blk_day, LV_OBJ_FLAG_SCROLLABLE);

                lv_obj_t* lbl_day = lv_label_create(blk_day);
                lv_label_set_text(lbl_day, text_rating[r_day]);
                lv_obj_set_style_text_font(lbl_day, &font_jetbrains_10, 0);
                lv_obj_set_style_text_color(lbl_day, lv_color_hex(0x000000), 0);
                lv_obj_center(lbl_day);

                // Night status block indicator layout geometry
                lv_obj_t* blk_nt = lv_obj_create(widget);
                lv_obj_set_size(blk_nt, 22, 18);
                lv_obj_align(blk_nt, LV_ALIGN_TOP_LEFT, 112, y_pos);
                lv_obj_set_style_bg_color(blk_nt, colors_rating[r_nt], 0); // FIXED PARENTHESIS
                lv_obj_set_style_radius(blk_nt, 3, 0);
                lv_obj_set_style_border_width(blk_nt, 0, 0);
                lv_obj_clear_flag(blk_nt, LV_OBJ_FLAG_SCROLLABLE);

                lv_obj_t* lbl_nt = lv_label_create(blk_nt);
                lv_label_set_text(lbl_nt, text_rating[r_nt]);
                lv_obj_set_style_text_font(lbl_nt, &font_jetbrains_10, 0);
                lv_obj_set_style_text_color(lbl_nt, lv_color_hex(0x000000), 0);
                lv_obj_center(lbl_nt);
            }
        }

        return widget;
    }

} // namespace ui