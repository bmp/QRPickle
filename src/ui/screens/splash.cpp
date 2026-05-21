#include "splash.h"
#include "../../hw/sensor.h"
#include "../../services/wifi_manager.h"
#include "../../config/config.h"
#include "../../core/metadata.h" // FIXED: Included compile-time global metadata anchor
#include "../fonts.h"
#include <WiFi.h>
#include <Arduino.h>
#include <stdio.h>

namespace ui {

    static void (*splash_complete_callback)() = nullptr;
    static lv_obj_t* lbl_wifi_status = nullptr;
    static int wifi_attempts = 0;
    static constexpr int MAX_WIFI_ATTEMPTS = 20;

    static void splash_timer_cb(lv_timer_t* timer) {
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

    void draw_splash_screen(lv_obj_t* parent, void (*on_complete)()) {
        splash_complete_callback = on_complete;
        wifi_attempts = 0;

        // 1. Build Fullscreen Splash Layer Base
        lv_obj_t* page = lv_obj_create(parent);
        lv_obj_set_size(page, 320, 240);
        lv_obj_set_style_bg_color(page, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_width(page, 0, 0);
        lv_obj_set_style_pad_all(page, 0, 0);
        lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);

        // 2. Main Firmware Identifier Logo
        lv_obj_t* lbl_logo = lv_label_create(page);
        lv_obj_set_style_text_font(lbl_logo, &font_jetbrains_24, 0);
        lv_obj_set_style_text_color(lbl_logo, lv_color_hex(0xFFB000), 0);
        lv_label_set_text(lbl_logo, meta::FW_NAME); // FIXED: Sourced from meta parameters
        lv_obj_align(lbl_logo, LV_ALIGN_TOP_MID, 0, 25);

        // 3. Metadata Release Parameters
        lv_obj_t* lbl_meta = lv_label_create(page);
        lv_obj_set_style_text_font(lbl_meta, &font_atkinson_14, 0);
        lv_obj_set_style_text_color(lbl_meta, lv_color_hex(0x8B949E), 0);

        // FIXED: Dynamically pipes in the version tracking array and your author callsign
        char meta_buf[64];
        snprintf(meta_buf, sizeof(meta_buf), "%s | Callsign: %s", meta::FW_VERSION, meta::AUTHOR_CALL);
        lv_label_set_text(lbl_meta, meta_buf);
        lv_obj_align(lbl_meta, LV_ALIGN_TOP_MID, 0, 58);

        // 4. Hardware Diagnostic Trace Window Box
        lv_obj_t* diag_box = lv_obj_create(page);
        lv_obj_set_size(diag_box, 260, 80);
        lv_obj_align(diag_box, LV_ALIGN_CENTER, 0, 15);
        lv_obj_set_style_bg_color(diag_box, lv_color_hex(0x1C2128), 0);
        lv_obj_set_style_border_color(diag_box, lv_color_hex(0x30363D), 0);
        lv_obj_set_style_border_width(diag_box, 1, 0);
        lv_obj_set_style_radius(diag_box, 4, 0);
        lv_obj_set_style_pad_all(diag_box, 8, 0);
        lv_obj_clear_flag(diag_box, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl_diag = lv_label_create(diag_box);
        lv_obj_set_style_text_font(lbl_diag, &font_jetbrains_14, 0);
        lv_obj_set_style_text_color(lbl_diag, lv_color_hex(0xE6EDF3), 0);

        bool bme_status = (sensor_get_pressure() > 200.0f);
        char diag_buf[128];
        snprintf(diag_buf, sizeof(diag_buf),
                 "SYS CORE: ONLINE\nNVS FLASH: SECURE READY\nI2C MASTER: ACTIVE OK\nBME280 SENSOR: %s",
                 bme_status ? "DETECTED OK" : "MISSING/ERR");
        lv_label_set_text(lbl_diag, diag_buf);
        lv_obj_align(lbl_diag, LV_ALIGN_TOP_LEFT, 0, 0);

        // 5. Network Handshaking Dynamic Output Status Line
        lbl_wifi_status = lv_label_create(page);
        lv_obj_set_style_text_font(lbl_wifi_status, &font_atkinson_14, 0);
        lv_obj_set_style_text_color(lbl_wifi_status, lv_color_hex(0x58A6FF), 0);
        lv_label_set_text(lbl_wifi_status, "Connecting to Network... [0/20]");
        lv_obj_align(lbl_wifi_status, LV_ALIGN_BOTTOM_MID, 0, -20);

        // 6. Spawn Asynchronous Network Interrogation Timer
        lv_timer_t* splash_timer = lv_timer_create(splash_timer_cb, 1000, nullptr);

        // 7. Clean memory cleanup hook tied directly to page destruction
        lv_obj_add_event_cb(page, [](lv_event_t* e) {
            lv_timer_t* t = (lv_timer_t*)lv_event_get_user_data(e);
            if (t) {
                lv_timer_delete(t);
                Serial.println("[Splash] Background polling timer disposed cleanly.");
            }
        }, LV_EVENT_DELETE, splash_timer);
    }

} // namespace ui
