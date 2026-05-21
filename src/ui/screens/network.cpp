#include "network.h"
#include "../fonts.h"
#include "../ui.h"
#include "../theme.h"
#include "../../config/config.h"
#include <WiFi.h>
#include <stdio.h>

namespace ui {

    static lv_obj_t* lbl_stats = nullptr;

    static void network_timer_cb(lv_timer_t* timer) {
        if (!lbl_stats) return;

        if (WiFi.isConnected()) {
            char buf[256];
            snprintf(buf, sizeof(buf), "Status: CONNECTED\nSSID: %s\nIP: %s\nRSSI: %d dBm\nMAC: %s",
                     WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI(), WiFi.macAddress().c_str());
            lv_label_set_text(lbl_stats, buf);
        } else {
            lv_label_set_text(lbl_stats, "Status: DISCONNECTED\nSSID: --\nIP: --\nRSSI: -- dBm");
        }
    }

    static void btn_reconnect_cb(lv_event_t* e) {
        WiFi.disconnect();
        WiFi.reconnect();
    }

    static void network_list_btn_cb(lv_event_t* e) {
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        lv_obj_t* modal = (lv_obj_t*)lv_event_get_user_data(e);

        lv_obj_t* label = lv_obj_get_child(btn, 1);
        if (label) {
            char extracted_ssid[64];
            strncpy(extracted_ssid, lv_label_get_text(label), sizeof(extracted_ssid) - 1);
            extracted_ssid[sizeof(extracted_ssid) - 1] = '\0';

            char* split = strstr(extracted_ssid, " (");
            if (split) *split = '\0';

            strncpy(config::mutable_get().wifi_ssid, extracted_ssid, sizeof(config::mutable_get().wifi_ssid) - 1);
            config::mutable_get().wifi_ssid[sizeof(config::mutable_get().wifi_ssid) - 1] = '\0';

            if (modal) lv_obj_del(modal);

            ui_navigate_local(PAGE_SETTINGS);
        }
    }

    static void btn_scan_cb(lv_event_t* e) {
        lv_obj_t* modal = lv_obj_create(lv_screen_active());
        lv_obj_set_size(modal, 280, 200);
        lv_obj_center(modal);

        // THEMED: Scanning Modal Layout Box
        lv_obj_set_style_bg_color(modal, theme_color(COLOR_BG_BAR), 0);
        lv_obj_set_style_border_color(modal, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(modal, 1, 0);

        lv_obj_t* lbl_loading = lv_label_create(modal);
        lv_label_set_text(lbl_loading, "Scanning RF... (Standby)");
        lv_obj_set_style_text_font(lbl_loading, &font_atkinson_14, 0);
        lv_obj_set_style_text_color(lbl_loading, theme_color(COLOR_TEXT_MAIN), 0); // THEMED
        lv_obj_align(lbl_loading, LV_ALIGN_CENTER, 0, 0);

        lv_refr_now(nullptr);

        int n = WiFi.scanNetworks();
        lv_obj_del(lbl_loading);

        lv_obj_t* list = lv_list_create(modal);
        lv_obj_set_size(list, 260, 140);
        lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 0);

        // THEMED: Scanning inner list container background
        lv_obj_set_style_bg_color(list, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_color(list, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(list, 1, 0);

        if (n == 0) {
            lv_obj_t* txt = lv_list_add_text(list, "No Access Points Found");
            lv_obj_set_style_text_color(txt, theme_color(COLOR_TEXT_MUTED), 0); // THEMED
        } else {
            for (int i = 0; i < n; ++i) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%s (%d dBm)", WiFi.SSID(i).c_str(), WiFi.RSSI(i));

                lv_obj_t* list_btn = lv_list_add_btn(list, LV_SYMBOL_WIFI, buf);
                lv_obj_set_style_text_color(list_btn, theme_color(COLOR_TEXT_MAIN), 0); // THEMED
                lv_obj_add_event_cb(list_btn, network_list_btn_cb, LV_EVENT_CLICKED, modal);
            }
        }

        lv_obj_t* btn_close = lv_btn_create(modal);
        lv_obj_align(btn_close, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(btn_close, theme_color(COLOR_BG_PANEL), 0); // THEMED
        lv_obj_set_style_border_color(btn_close, theme_color(COLOR_BORDER), 0); // THEMED
        lv_obj_set_style_border_width(btn_close, 1, 0);

        lv_obj_t* lbl_close = lv_label_create(btn_close);
        lv_label_set_text(lbl_close, "Close");
        lv_obj_set_style_text_color(lbl_close, theme_color(COLOR_TEXT_MAIN), 0); // THEMED

        lv_obj_add_event_cb(btn_close, [](lv_event_t* ev) {
            lv_obj_t* target = (lv_obj_t*)lv_event_get_target(ev);
            lv_obj_t* modal_parent = (lv_obj_t*)lv_obj_get_parent(target);
            lv_obj_del(modal_parent);
        }, LV_EVENT_CLICKED, nullptr);
    }

    void draw_network_page(lv_obj_t* parent) {
        lv_obj_t* page = lv_obj_create(parent);
        lv_obj_set_size(page, 320, 216);
        lv_obj_set_style_bg_opa(page, 0, 0);
        lv_obj_set_style_border_width(page, 0, 0);
        lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);

        // Stats Panel
        lbl_stats = lv_label_create(page);
        lv_obj_set_style_text_font(lbl_stats, &font_jetbrains_14, 0);
        lv_obj_set_style_text_color(lbl_stats, theme_color(COLOR_ACCENT_PRIMARY), 0); // THEMED
        lv_obj_align(lbl_stats, LV_ALIGN_TOP_LEFT, 20, 20);

        network_timer_cb(nullptr);

        // Reconnect Button
        lv_obj_t* btn_recon = lv_btn_create(page);
        lv_obj_align(btn_recon, LV_ALIGN_BOTTOM_LEFT, 20, -20);
        lv_obj_set_style_bg_color(btn_recon, theme_color(COLOR_BG_BAR), 0); // THEMED
        lv_obj_set_style_border_color(btn_recon, theme_color(COLOR_BORDER), 0); // THEMED
        lv_obj_set_style_border_width(btn_recon, 1, 0);

        lv_obj_t* lbl_recon = lv_label_create(btn_recon);
        lv_label_set_text(lbl_recon, LV_SYMBOL_REFRESH " Reconnect");
        lv_obj_set_style_text_font(lbl_recon, &font_atkinson_14, 0);
        lv_obj_set_style_text_color(lbl_recon, theme_color(COLOR_TEXT_MAIN), 0); // THEMED
        lv_obj_add_event_cb(btn_recon, btn_reconnect_cb, LV_EVENT_CLICKED, nullptr);

        // Scan Button
        lv_obj_t* btn_scan = lv_btn_create(page);
        lv_obj_align(btn_scan, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
        lv_obj_set_style_bg_color(btn_scan, theme_color(COLOR_BG_BAR), 0); // THEMED
        lv_obj_set_style_border_color(btn_scan, theme_color(COLOR_BORDER), 0); // THEMED
        lv_obj_set_style_border_width(btn_scan, 1, 0);

        lv_obj_t* lbl_scan = lv_label_create(btn_scan);
        lv_label_set_text(lbl_scan, LV_SYMBOL_WIFI " Scan APs");
        lv_obj_set_style_text_font(lbl_scan, &font_atkinson_14, 0);
        lv_obj_set_style_text_color(lbl_scan, theme_color(COLOR_TEXT_MAIN), 0); // THEMED
        lv_obj_add_event_cb(btn_scan, btn_scan_cb, LV_EVENT_CLICKED, nullptr);

        lv_timer_t* n_timer = lv_timer_create(network_timer_cb, 2000, page);
        lv_obj_add_event_cb(page, [](lv_event_t* e) {
            lv_timer_t* t = (lv_timer_t*)lv_event_get_user_data(e);
            if (t) lv_timer_delete(t);
            lbl_stats = nullptr;
        }, LV_EVENT_DELETE, n_timer);
    }

} // namespace ui
