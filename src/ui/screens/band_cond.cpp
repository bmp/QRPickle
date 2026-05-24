#include "band_cond.h"
#include "../theme.h"
#include "../fonts.h"
#include "../ui.h"
#include "../../services/prop_manager.h"
#include <cstdio>

namespace ui {

    static lv_obj_t* root_layer = nullptr;

    static void generate_telemetry_row(lv_obj_t* parent, int y, const char* label, const char* value, const char* desc, lv_color_t text_color) {
        lv_obj_t* lbl_name = lv_label_create(parent);
        lv_label_set_text(lbl_name, label);
        lv_obj_set_style_text_font(lbl_name, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_name, theme_color(COLOR_TEXT_MUTED), 0);
        lv_obj_align(lbl_name, LV_ALIGN_TOP_LEFT, 8, y);

        lv_obj_t* lbl_val = lv_label_create(parent);
        lv_label_set_text(lbl_val, value);
        lv_obj_set_style_text_font(lbl_val, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_val, text_color, 0);
        lv_obj_align(lbl_val, LV_ALIGN_TOP_LEFT, 175, y);

        lv_obj_t* lbl_desc = lv_label_create(parent);
        lv_label_set_text(lbl_desc, desc);
        lv_obj_set_style_text_font(lbl_desc, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_desc, theme_color(COLOR_TEXT_MAIN), 0);
        lv_obj_align(lbl_desc, LV_ALIGN_TOP_LEFT, 215, y);
    }

    void draw_band_cond_page(lv_obj_t* parent) {
        root_layer = lv_obj_create(parent);
        // Correct footprint bounds allowing status bar visibility
        lv_obj_set_size(root_layer, 320, 216); 
        lv_obj_set_style_bg_color(root_layer, theme_color(COLOR_BG_APP), 0);
        lv_obj_set_style_border_width(root_layer, 0, 0);
        lv_obj_set_style_pad_all(root_layer, 0, 0);
        lv_obj_clear_flag(root_layer, LV_OBJ_FLAG_SCROLLABLE);

        const auto& tel = services::PropagationManager::get_telemetry();
        
        char buf_sfi[12], buf_k[12], buf_a[12];
        snprintf(buf_sfi, sizeof(buf_sfi), "%u", tel.sfi);
        snprintf(buf_k, sizeof(buf_k), "%u", tel.k_index);
        snprintf(buf_a, sizeof(buf_a), "%u", tel.a_index);

        const char* text_sfi_desc = (tel.sfi >= 130) ? "HIGH" : ((tel.sfi >= 95) ? "MED" : "LOW");
        lv_color_t color_sfi_val  = (tel.sfi >= 120) ? theme_color(COLOR_BAND_GOOD) : 
                                    ((tel.sfi >= 95) ? theme_color(COLOR_BAND_FAIR) : theme_color(COLOR_BAND_POOR));

        const char* text_k_desc = (tel.k_index >= 5) ? "STORM" : ((tel.k_index >= 3) ? "WARN" : "QUIET");
        lv_color_t color_k_val  = (tel.k_index >= 4) ? theme_color(COLOR_BAND_POOR) : 
                                  ((tel.k_index == 3) ? theme_color(COLOR_BAND_FAIR) : theme_color(COLOR_BAND_GOOD));

        int row_start_y = 12;
        generate_telemetry_row(root_layer, row_start_y + 0,  "SOLAR FLUX INDEX      (SFI)", buf_sfi, text_sfi_desc, color_sfi_val);
        generate_telemetry_row(root_layer, row_start_y + 18, "GEOMAGNETIC K-INDEX     (K)", buf_k,   text_k_desc,   color_k_val);
        generate_telemetry_row(root_layer, row_start_y + 36, "GEOMAGNETIC A-INDEX     (A)", buf_a,   "24H AVG",     theme_color(COLOR_TEXT_MAIN));
        
        lv_obj_t* sep_line = lv_obj_create(root_layer);
        lv_obj_set_size(sep_line, 304, 1);
        lv_obj_align(sep_line, LV_ALIGN_TOP_MID, 0, row_start_y + 60);
        lv_obj_set_style_bg_color(sep_line, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(sep_line, 0, 0);

        int env_row_start_y = row_start_y + 70;
        generate_telemetry_row(root_layer, env_row_start_y + 0,  "IONOSPHERIC REACTION", "ACTIVE", "[F2 BOUNDS]", theme_color(COLOR_BAND_GOOD));
        generate_telemetry_row(root_layer, env_row_start_y + 18, "CORONAL PLASMA FLUX",  "C1.0",   "[NO FLARE]",   theme_color(COLOR_TEXT_MAIN));
        generate_telemetry_row(root_layer, env_row_start_y + 36, "AURORAL SCATTER PATH", "CLOSED", "[LAT <60\xC2\xB0]", theme_color(COLOR_BAND_POOR));
        
        // Dynamic Bottom Forecast Message Card Banner
        lv_obj_t* banner_card = lv_obj_create(root_layer);
        lv_obj_set_size(banner_card, 304, 30);
        lv_obj_align(banner_card, LV_ALIGN_BOTTOM_MID, 0, -8);
        lv_obj_set_style_bg_color(banner_card, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_color(banner_card, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(banner_card, 1, 0);
        lv_obj_set_style_pad_all(banner_card, 4, 0);
        lv_obj_clear_flag(banner_card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl_f_title = lv_label_create(banner_card);
        lv_label_set_text(lbl_f_title, "NOAA FORECAST: ");
        lv_obj_set_style_text_font(lbl_f_title, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_f_title, theme_color(COLOR_TEXT_MUTED), 0);
        lv_obj_align(lbl_f_title, LV_ALIGN_LEFT_MID, 4, 0);

        lv_obj_t* lbl_f_body = lv_label_create(banner_card);
        lv_label_set_text(lbl_f_body, tel.forecast[0] ? tel.forecast : "NO DYNAMIC REPORTING RECORDED");
        lv_obj_set_style_text_font(lbl_f_body, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_f_body, theme_color(COLOR_TEXT_MAIN), 0);
        lv_obj_align(lbl_f_body, LV_ALIGN_LEFT_MID, 98, 0);

        lv_obj_add_event_cb(root_layer, [](lv_event_t*){
            root_layer = nullptr;
        }, LV_EVENT_DELETE, nullptr);
    }
}