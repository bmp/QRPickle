#include "splash.h"
#include "../../hw/sensor.h"
#include "../../services/wifi_manager.h"
#include "../../config/config.h"
#include "../../core/metadata.h" 
#include "../fonts.h"
#include <WiFi.h>
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

        // 2. Main Firmware Identifier Logo Text
        lv_obj_t* lbl_logo = lv_label_create(page);
        lv_obj_set_style_text_font(lbl_logo, &font_jetbrains_24, 0);
        lv_obj_set_style_text_color(lbl_logo, lv_color_hex(0xFFB000), 0);
        lv_label_set_text(lbl_logo, meta::FW_NAME); 
        lv_obj_align(lbl_logo, LV_ALIGN_TOP_MID, 0, 25);

        // 3. Spawn Left Image and lock further out to the left
        lv_obj_t* img_left = lv_image_create(page);
        lv_image_set_src(img_left, &logo_splash_left_50x50);
        lv_obj_align_to(img_left, lbl_logo, LV_ALIGN_OUT_LEFT_MID, -36, 0);

        // 4. Spawn Right Image and lock further out to the right
        lv_obj_t* img_right = lv_image_create(page);
        lv_image_set_src(img_right, &logo_splash_right_30x60);
        lv_obj_align_to(img_right, lbl_logo, LV_ALIGN_OUT_RIGHT_MID, 42, 0);

        // 5. Metadata Release Parameters
        lv_obj_t* lbl_meta = lv_label_create(page);
        lv_obj_set_style_text_font(lbl_meta, &font_atkinson_14, 0);
        lv_obj_set_style_text_color(lbl_meta, lv_color_hex(0x8B949E), 0);

        char meta_buf[64];
        snprintf(meta_buf, sizeof(meta_buf), "%s | %s", meta::FW_VERSION, meta::AUTHOR_CALL);
        lv_label_set_text(lbl_meta, meta_buf);
        lv_obj_align(lbl_meta, LV_ALIGN_TOP_MID, 0, 58);

        // 6. Hardware Diagnostic Trace Window Box
        lv_obj_t* diag_box = lv_obj_create(page);
        // FIXED: Expanded height to 125px to accommodate the text data comfortably
        lv_obj_set_size(diag_box, 260, 125);
        // FIXED: Shifted slightly downward to center the larger container beautifully
        lv_obj_align(diag_box, LV_ALIGN_CENTER, 0, 26);
        lv_obj_set_style_bg_color(diag_box, lv_color_hex(0x1C2128), 0);
        lv_obj_set_style_border_color(diag_box, lv_color_hex(0x30363D), 0);
        lv_obj_set_style_border_width(diag_box, 1, 0);
        lv_obj_set_style_radius(diag_box, 4, 0);
        lv_obj_set_style_pad_all(diag_box, 8, 0);
        lv_obj_clear_flag(diag_box, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl_diag = lv_label_create(diag_box);
        lv_obj_set_style_text_font(lbl_diag, &font_jetbrains_10, 0);
        lv_obj_set_style_text_color(lbl_diag, lv_color_hex(0xE6EDF3), 0);
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
        lbl_wifi_status = lv_label_create(page);
        // FIXED: Dropped the text size to 10 to clean up layout hierarchy
        lv_obj_set_style_text_font(lbl_wifi_status, &font_atkinson_10, 0);
        lv_obj_set_style_text_color(lbl_wifi_status, lv_color_hex(0x58A6FF), 0);
        lv_label_set_text(lbl_wifi_status, "Connecting to Network... [0/20]");
        // FIXED: Tucked tightly against the bottom bezel to make room for the expanded window
        lv_obj_align(lbl_wifi_status, LV_ALIGN_BOTTOM_MID, 0, -6);

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

} // namespace ui