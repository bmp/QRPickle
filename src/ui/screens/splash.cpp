#include "splash.h"
#include "../layout.h"
#include "../theme.h"
#include "../../hw/sensor.h"
#include "../../services/wifi_manager.h"
#include "../../config/config.h"
#include "../../core/metadata.h" 
#include "../fonts.h"
#include <WiFi.h>
#include <LittleFS.h>
#include "../../core/logo_file.h"
#ifdef QRP_SCREEN_TOOLS
#include "../../core/screen_tools.h"
#endif
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

LV_IMAGE_DECLARE(logo_splash_left_50x50);
LV_IMAGE_DECLARE(logo_splash_right_30x60);

namespace ui {

    static void (*splash_complete_callback)() = nullptr;
    static lv_obj_t* lbl_wifi_status = nullptr;
    static int wifi_attempts = 0;
    static constexpr int MAX_WIFI_ATTEMPTS = 20;

    static void splash_timer_cb(lv_timer_t* timer) {
#ifdef QRP_SCREEN_TOOLS
        if (screen_tools::splash_held()) return;  // being photographed (tools/device_screens.py)
#endif
        wifi_attempts++;
        Serial.printf("[Splash] Verifying Wi-Fi Link... Attempt %d/%d\n", wifi_attempts, MAX_WIFI_ATTEMPTS);

        if (WiFi.isConnected() || wifi_manager_is_connected()) {
            Serial.println("[Splash] Wi-Fi Connection Authenticated! Launching Dashboard...");
            if (splash_complete_callback) splash_complete_callback();
            return;
        }

        if (wifi_attempts >= MAX_WIFI_ATTEMPTS) {
            Serial.println("[Splash] Wi-Fi initialization timed out. Launching Offline Mode...");
            if (splash_complete_callback) splash_complete_callback();
            return;
        }

        char buf[64];
        snprintf(buf, sizeof(buf), "Connecting to Network... [%d/%d]", wifi_attempts, MAX_WIFI_ATTEMPTS);
        if (lbl_wifi_status) lv_label_set_text(lbl_wifi_status, buf);
    }

    // A custom logo (web console, /logos/<side>.bin) when present and valid, else the built-in one.
    static void set_logo(lv_obj_t* img, const char* side, const lv_image_dsc_t* builtin) {
        static char path[2][24];
        const int slot = side[0] == 'l' ? 0 : 1;
        char fs_path[20];
        snprintf(fs_path, sizeof(fs_path), "/logos/%s.bin", side);
        File f = LittleFS.exists(fs_path) ? LittleFS.open(fs_path, "r") : File();  // no error log when absent
        if (f) {
            uint8_t hdr[core::logo::HEADER_BYTES];
            const size_t n = f.read(hdr, sizeof(hdr));
            const size_t size = f.size();
            f.close();
            if (core::logo::header_valid(hdr, n, size)) {
                snprintf(path[slot], sizeof(path[slot]), "L:%s", fs_path);  // LVGL keeps the pointer
                lv_image_set_src(img, path[slot]);
                return;
            }
        }
        lv_image_set_src(img, builtin);
    }

    // The splash page (title, logos, version, diagnostics, status line); returns the status label.
    static lv_obj_t* build_splash(lv_obj_t* page) {
        // 1. Build Fullscreen Splash Layer Base
        lv_obj_set_size(page, SCREEN_W, SCREEN_H);
        lv_obj_set_style_bg_color(page, theme_color(COLOR_BG_APP), 0);
        lv_obj_set_style_border_width(page, 0, 0);
        lv_obj_set_style_pad_all(page, 0, 0);
        lv_obj_set_scrollable(page, false);

        // 2. Main Firmware Identifier Logo Text
        lv_obj_t* lbl_logo = lv_label_create(page);
        lv_obj_set_style_text_font(lbl_logo, &font_jetbrains_24, 0);
        lv_obj_set_style_text_color(lbl_logo, theme_color(COLOR_ACCENT_PRIMARY), 0);
        lv_label_set_text(lbl_logo, meta::FW_NAME); 
        lv_obj_align(lbl_logo, LV_ALIGN_TOP_MID, 0, 25);

        // 3. Left logo: centred where the built-in 50 x 50 logo is centred
        lv_obj_t* img_left = lv_image_create(page);
        set_logo(img_left, "left", &logo_splash_left_50x50);
        lv_obj_align_to(img_left, lbl_logo, LV_ALIGN_OUT_LEFT_MID, -36 - 25 + (int)lv_image_get_src_width(img_left) / 2, 0);

        // 4. Right logo: centred where the built-in 30 x 60 logo is centred
        lv_obj_t* img_right = lv_image_create(page);
        set_logo(img_right, "right", &logo_splash_right_30x60);
        lv_obj_align_to(img_right, lbl_logo, LV_ALIGN_OUT_RIGHT_MID, 42 + 15 - (int)lv_image_get_src_width(img_right) / 2, 0);

        // 5. Metadata Release Parameters
        lv_obj_t* lbl_meta = lv_label_create(page);
        lv_obj_set_style_text_font(lbl_meta, &font_atkinson_14, 0);
        lv_obj_set_style_text_color(lbl_meta, theme_color(COLOR_TEXT_MUTED), 0);

        char meta_buf[64];
        snprintf(meta_buf, sizeof(meta_buf), "%s | %s", meta::FW_VERSION, meta::AUTHOR_CALL);
        lv_label_set_text(lbl_meta, meta_buf);
        lv_obj_align(lbl_meta, LV_ALIGN_TOP_MID, 0, 58);

        // 6. Hardware Diagnostic Trace Window Box
        lv_obj_t* diag_box = lv_obj_create(page);
        // Expanded height to 125px to accommodate the text data comfortably
        lv_obj_set_size(diag_box, 260, 125);
        // Shifted slightly downward to center the larger container beautifully
        lv_obj_align(diag_box, LV_ALIGN_CENTER, 0, 26);
        lv_obj_set_style_bg_color(diag_box, theme_color(COLOR_BG_BAR), 0);
        lv_obj_set_style_border_color(diag_box, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(diag_box, 1, 0);
        lv_obj_set_style_radius(diag_box, 4, 0);
        lv_obj_set_style_pad_all(diag_box, 8, 0);
        lv_obj_set_scrollable(diag_box, false);

        lv_obj_t* lbl_diag = lv_label_create(diag_box);
        lv_obj_set_style_text_font(lbl_diag, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_diag, theme_color(COLOR_TEXT_MAIN), 0);
        lv_obj_set_style_text_line_space(lbl_diag, 4, 0);

        // Query configuration parameter states
        const auto& c = config::get();
        bool bme_status = (sensor_get_pressure() > 200.0f);
        bool station_set = (strlen(c.callsign) > 0 && strlen(c.grid) > 0);
        bool wifi_set = (strlen(c.wifi_ssid) > 0);
        bool owm_set = (strlen(c.openweather_api_key) > 0);

        char diag_buf[384];
        snprintf(diag_buf, sizeof(diag_buf),
                 "SYS CORE:         ONLINE\n"
                 "NVS FLASH:        SECURE READY\n"
                 "I2C MASTER:       ACTIVE OK\n"
                 "BME280 SENSOR:    %s\n"
                 "STATION INFO:     %s\n"
                 "WIFI CREDENTIALS: %s\n"
                 "OPENWEATHER API:  %s",
                 bme_status ? "DETECTED OK" : "MISSING/ERR",
                 station_set ? "SET" : "NOT SET",
                 wifi_set ? "SET" : "NOT SET",
                 owm_set ? "SET" : "NOT SET");
                 
        lv_label_set_text(lbl_diag, diag_buf);
        lv_obj_align(lbl_diag, LV_ALIGN_TOP_LEFT, 0, 0);

        // 7. Network Handshaking Dynamic Output Status Line
        lv_obj_t* status = lv_label_create(page);
        // Dropped the text size to 10 to clean up layout hierarchy
        lv_obj_set_style_text_font(status, &font_atkinson_10, 0);
        lv_obj_set_style_text_color(status, theme_color(COLOR_INFO), 0);
        lv_label_set_text(status, "Connecting to Network... [0/20]");
        // Tucked tightly against the bottom bezel to make room for the expanded window
        lv_obj_align(status, LV_ALIGN_BOTTOM_MID, 0, -6);
        return status;
    }

    void draw_splash_screen(lv_obj_t* parent, void (*on_complete)()) {
        splash_complete_callback = on_complete;
        wifi_attempts = 0;
        lv_obj_t* page = lv_obj_create(parent);
        lbl_wifi_status = build_splash(page);

        // 8. Spawn Asynchronous Network Interrogation Timer
        lv_timer_t* splash_timer = lv_timer_create(splash_timer_cb, 1000, nullptr);

        // 9. Clean memory cleanup hook tied directly to page destruction
        lv_obj_add_event_cb(page, [](lv_event_t* e) {
            lv_timer_t* t = (lv_timer_t*)lv_event_get_user_data(e);
            if (t) {
                lv_timer_delete(t);
                Serial.println("[Splash] Background polling timer disposed cleanly.");
            }
        }, LV_EVENT_DELETE, splash_timer);
    }

    // ---- Preview (web console, Splash Logos: "Preview on the device") ---------------------------
    static lv_obj_t* preview_page = nullptr;
    static lv_timer_t* preview_timer = nullptr;  // one-shot: LVGL deletes it after it fires

    bool splash_preview_active() { return preview_page != nullptr; }

    bool splash_preview_show(uint32_t ms) {
        if (preview_page) return false;
        // On the top layer, over the current page, which stays as it is underneath.
        preview_page = lv_obj_create(lv_layer_top());
        lv_obj_t* status = build_splash(preview_page);
        lv_label_set_text_fmt(status, "Preview: closes in %u s, or tap to close", (unsigned)(ms / 1000));
        lv_obj_add_flag(preview_page, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(preview_page, [](lv_event_t*) { lv_obj_delete_async(preview_page); }, LV_EVENT_CLICKED, nullptr);
        preview_timer = lv_timer_create([](lv_timer_t*) {
            preview_timer = nullptr;  // deleted by LVGL after this call (repeat count 1)
            if (preview_page) lv_obj_delete_async(preview_page);
        },
                                        ms, nullptr);
        lv_timer_set_repeat_count(preview_timer, 1);
        lv_obj_add_event_cb(preview_page, [](lv_event_t*) {
            // Closed by a tap: the timer is still pending. Deleting a timer that already fired
            // corrupted the LVGL pool (panic in lv_tlsf, 2026-11-06).
            if (preview_timer) lv_timer_delete(preview_timer);
            preview_timer = nullptr;
            preview_page = nullptr; }, LV_EVENT_DELETE, nullptr);
        return true;
    }

} // namespace ui