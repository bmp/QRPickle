#include "network.h"
#include "../layout.h"
#include "../fonts.h"
#include "../ui.h"
#include "../theme.h"
#include "../privacy.h"
#include "../../config/config.h"
#include <WiFi.h>
#include <stdio.h>

namespace ui {

    static lv_obj_t* lbl_stats = nullptr;
    static lv_obj_t* lbl_login = nullptr;
    static lv_obj_t* lbl_eye = nullptr;
    static uint32_t reveal_until_ms = 0;  // admin password shown until then (0 = masked)
    constexpr uint32_t REVEAL_MS = 10000;

    static void network_timer_cb(lv_timer_t* timer) {
        if (!lbl_stats) return;

        char buf[256];
        if (WiFi.isConnected()) {
            snprintf(buf, sizeof(buf), "Status: CONNECTED\nSSID: %s\nIP: %s\nRSSI: %d dBm\nMAC: %s",
                     privacy::ssid(WiFi.SSID().c_str()), privacy::ip(WiFi.localIP().toString().c_str()), WiFi.RSSI(),
                     privacy::mac(WiFi.macAddress().c_str()));
        } else {
            snprintf(buf, sizeof(buf), "Status: DISCONNECTED\nSSID: --\nIP: --\nRSSI: -- dBm\nMAC: --");
        }
        lv_label_set_text(lbl_stats, buf);

        // The admin password stays masked; the eye button shows it for 10 s (screenshot builds: never).
        if (reveal_until_ms && (int32_t)(millis() - reveal_until_ms) >= 0) reveal_until_ms = 0;
        const bool shown = reveal_until_ms && !privacy::SCREENSHOT_BUILD;
        snprintf(buf, sizeof(buf), "Web login: admin / %s", shown ? config::get().admin_password : privacy::MASK);
        lv_label_set_text(lbl_login, buf);
        lv_label_set_text(lbl_eye, shown ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
    }

    static void btn_eye_cb(lv_event_t*) {
        reveal_until_ms = reveal_until_ms ? 0 : millis() + REVEAL_MS;
        network_timer_cb(nullptr);
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

        // A scrolling flex column (lv_list is deprecated in LVGL 9.6). Each entry is a button with
        // an icon label (child 0) and the "SSID (RSSI dBm)" label (child 1) that the click handler reads.
        lv_obj_t* list = lv_obj_create(modal);
        lv_obj_set_size(list, 260, 140);
        lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(list, 4, 0);
        lv_obj_set_style_pad_row(list, 2, 0);

        // THEMED: Scanning inner list container background
        lv_obj_set_style_bg_color(list, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_color(list, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(list, 1, 0);

        if (n == 0) {
            lv_obj_t* txt = lv_label_create(list);
            lv_label_set_text(txt, "No Access Points Found");
            lv_obj_set_style_text_color(txt, theme_color(COLOR_TEXT_MUTED), 0); // THEMED
        } else {
            for (int i = 0; i < n; ++i) {
                char buf[64];
                // Screenshot builds hide the neighbours' network names.
                char demo[16];
                snprintf(demo, sizeof(demo), "Network %d", i + 1);
                snprintf(buf, sizeof(buf), "%s (%d dBm)", privacy::SCREENSHOT_BUILD ? demo : WiFi.SSID(i).c_str(),
                         WiFi.RSSI(i));

                lv_obj_t* list_btn = lv_button_create(list);
                lv_obj_set_width(list_btn, lv_pct(100));
                lv_obj_set_flex_flow(list_btn, LV_FLEX_FLOW_ROW);
                lv_obj_set_flex_align(list_btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
                lv_obj_set_style_pad_column(list_btn, 8, 0);
                lv_obj_set_style_bg_color(list_btn, theme_color(COLOR_BG_PANEL), 0);
                lv_obj_set_style_bg_color(list_btn, theme_color(COLOR_BG_BAR), LV_STATE_PRESSED);
                lv_obj_set_style_shadow_width(list_btn, 0, 0);
                lv_obj_set_style_radius(list_btn, 0, 0);
                lv_obj_t* icon = lv_label_create(list_btn);
                lv_label_set_text(icon, LV_SYMBOL_WIFI);
                lv_obj_t* name = lv_label_create(list_btn);
                lv_label_set_text(name, buf);
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
        lv_obj_set_size(page, SCREEN_W, CONTENT_H);
        lv_obj_set_style_bg_opa(page, 0, 0);
        lv_obj_set_style_border_width(page, 0, 0);
        lv_obj_set_scrollable(page, false);

        // Stats Panel
        lbl_stats = lv_label_create(page);
        lv_obj_set_style_text_font(lbl_stats, &font_jetbrains_14, 0);
        lv_obj_set_style_text_color(lbl_stats, theme_color(COLOR_ACCENT_PRIMARY), 0); // THEMED
        lv_obj_align(lbl_stats, LV_ALIGN_TOP_LEFT, 20, 20);

        // Web login line with an eye button (tap to reveal the password for 10 s).
        reveal_until_ms = 0;
        lbl_login = lv_label_create(page);
        lv_obj_set_style_text_font(lbl_login, &font_jetbrains_14, 0);
        lv_obj_set_style_text_color(lbl_login, theme_color(COLOR_ACCENT_PRIMARY), 0);
        lv_obj_align(lbl_login, LV_ALIGN_TOP_LEFT, 20, 20 + 5 * 18);
        lv_obj_t* btn_eye = lv_button_create(page);
        lv_obj_set_size(btn_eye, 36, 26);
        lv_obj_align(btn_eye, LV_ALIGN_TOP_RIGHT, -12, 20 + 5 * 18 - 5);
        lv_obj_set_style_bg_color(btn_eye, theme_color(COLOR_BG_BAR), 0);
        lv_obj_set_style_border_color(btn_eye, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(btn_eye, 1, 0);
        lv_obj_add_event_cb(btn_eye, btn_eye_cb, LV_EVENT_CLICKED, nullptr);
        lbl_eye = lv_label_create(btn_eye);
        lv_obj_set_style_text_color(lbl_eye, theme_color(COLOR_TEXT_MAIN), 0);
        lv_obj_center(lbl_eye);

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

        lv_timer_t* n_timer = lv_timer_create(network_timer_cb, 1000, page);  // 1 s: the reveal times out
        lv_obj_add_event_cb(page, [](lv_event_t* e) {
            lv_timer_t* t = (lv_timer_t*)lv_event_get_user_data(e);
            if (t) lv_timer_delete(t);
            lbl_stats = nullptr;
            lbl_login = nullptr;
            lbl_eye = nullptr;
            reveal_until_ms = 0;
        }, LV_EVENT_DELETE, n_timer);
    }

} // namespace ui
