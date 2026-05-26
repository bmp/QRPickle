#include "cloud_ota.h"
#include "../theme.h"
#include "../fonts.h"
#include "../../services/cloud_ota.h"
#include "../../core/metadata.h"
#include <Arduino.h>

namespace ui {

    static lv_obj_t* page_container = nullptr;
    static lv_obj_t* notes_label = nullptr;
    static lv_obj_t* flash_btn = nullptr;

    static void flash_confirm_cb(lv_event_t* e) {
        lv_obj_t* mbox = lv_msgbox_create(NULL);
        lv_msgbox_add_title(mbox, "System Upgrade");
        lv_msgbox_add_text(mbox, "Halt telemetry and flash firmware from GitHub?");

        lv_obj_t* btn_ok = lv_msgbox_add_footer_button(mbox, "Flash");
        lv_obj_t* btn_cancel = lv_msgbox_add_footer_button(mbox, "Cancel");

        lv_obj_set_style_bg_color(mbox, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_text_color(mbox, theme_color(COLOR_TEXT_MAIN), 0);
        lv_obj_set_style_border_color(mbox, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(mbox, 1, 0);

        lv_obj_add_event_cb(btn_ok, [](lv_event_t* e) {
            lv_obj_t* m = (lv_obj_t*)lv_event_get_user_data(e);
            lv_msgbox_close(m);
            
            if (services::cloud_ota::execute_firmware_flash()) {
                ESP.restart();
            }
        }, LV_EVENT_CLICKED, mbox);

        lv_obj_add_event_cb(btn_cancel, [](lv_event_t* e) {
            lv_obj_t* m = (lv_obj_t*)lv_event_get_user_data(e);
            lv_msgbox_close(m);
        }, LV_EVENT_CLICKED, mbox);
    }

    void draw_cloud_ota_page(lv_obj_t* parent) {
        page_container = lv_obj_create(parent);
        lv_obj_set_size(page_container, 320, 216);
        lv_obj_set_style_bg_color(page_container, theme_color(COLOR_BG_APP), 0);
        lv_obj_set_style_border_width(page_container, 0, 0);
        lv_obj_set_style_pad_all(page_container, 8, 0);
        lv_obj_clear_flag(page_container, LV_OBJ_FLAG_SCROLLABLE);

        auto info = services::cloud_ota::get_release_info();

        // Version Comparison Header
        lv_obj_t* header = lv_obj_create(page_container);
        lv_obj_set_size(header, 304, 45);
        lv_obj_set_style_bg_color(header, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_color(header, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(header, 1, 0);
        lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

        lv_obj_t* local_lbl = lv_label_create(header);
        lv_label_set_text_fmt(local_lbl, "Local: %s", meta::FW_VERSION);
        lv_obj_set_style_text_font(local_lbl, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(local_lbl, theme_color(COLOR_TEXT_MUTED), 0);
        lv_obj_align(local_lbl, LV_ALIGN_LEFT_MID, 10, 0);

        // NEW: Manual Check Button
        lv_obj_t* btn_check = lv_button_create(header);
        lv_obj_set_size(btn_check, 60, 30);
        lv_obj_align(btn_check, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(btn_check, theme_color(COLOR_BG_APP), 0);
        lv_obj_set_style_border_color(btn_check, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(btn_check, 1, 0);

        lv_obj_t* btn_check_lbl = lv_label_create(btn_check);
        lv_label_set_text(btn_check_lbl, LV_SYMBOL_REFRESH);
        lv_obj_set_style_text_color(btn_check_lbl, theme_color(COLOR_TEXT_MAIN), 0);
        lv_obj_center(btn_check_lbl);

        lv_obj_add_event_cb(btn_check, [](lv_event_t* e) {
            services::cloud_ota::force_update_check();
            lv_obj_t* parent = lv_obj_get_parent(page_container);
            lv_obj_delete(page_container);
            draw_cloud_ota_page(parent);
        }, LV_EVENT_CLICKED, NULL);

        lv_obj_t* remote_lbl = lv_label_create(header);
        lv_label_set_text_fmt(remote_lbl, "GitHub: %s", info.latest_version);
        lv_obj_set_style_text_font(remote_lbl, &font_jetbrains_14, 0);
        lv_obj_set_style_text_color(remote_lbl, info.update_available ? theme_color(COLOR_BAND_POOR) : theme_color(COLOR_BAND_GOOD), 0);
        lv_obj_align(remote_lbl, LV_ALIGN_RIGHT_MID, -10, 0);

        // Changelog Container
        lv_obj_t* notes_area = lv_obj_create(page_container);
        lv_obj_set_size(notes_area, 304, 90);
        lv_obj_align_to(notes_area, header, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);
        lv_obj_set_style_bg_color(notes_area, lv_color_hex(0x0a0a0a), 0);
        lv_obj_set_style_border_color(notes_area, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(notes_area, 1, 0);

        notes_label = lv_label_create(notes_area);
        lv_obj_set_width(notes_label, 280);
        lv_label_set_text(notes_label, info.update_available ? info.release_notes : "System is currently up to date with repository head.");
        lv_obj_set_style_text_font(notes_label, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(notes_label, theme_color(COLOR_TEXT_MAIN), 0);
        lv_label_set_long_mode(notes_label, LV_LABEL_LONG_WRAP);

        // Flash Button
        flash_btn = lv_button_create(page_container);
        lv_obj_set_size(flash_btn, 240, 40);
        lv_obj_align(flash_btn, LV_ALIGN_BOTTOM_MID, 0, -5);
        lv_obj_set_style_bg_color(flash_btn, theme_color(COLOR_ACCENT_PRIMARY), 0);
        
        if (!info.update_available) {
            lv_obj_add_state(flash_btn, LV_STATE_DISABLED);
            lv_obj_set_style_bg_color(flash_btn, theme_color(COLOR_TEXT_MUTED), 0);
        }

        lv_obj_t* btn_lbl = lv_label_create(flash_btn);
        lv_label_set_text(btn_lbl, "INITIATE FIRMWARE FLASH");
        lv_obj_set_style_text_font(btn_lbl, &font_atkinson_14, 0);
        lv_obj_set_style_text_color(btn_lbl, lv_color_hex(0x000000), 0);
        lv_obj_center(btn_lbl);

        lv_obj_add_event_cb(flash_btn, flash_confirm_cb, LV_EVENT_CLICKED, NULL);
    }

} // namespace ui
