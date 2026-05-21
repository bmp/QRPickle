#include <Arduino.h>
#include "hw/display.h"
#include "hw/touch.h"
#include <lvgl.h>

// Button event callback function
static void btn_event_cb(lv_event_t * e) {
    // THE FIX: Explicitly cast the generic void* to an lv_obj_t* for C++
    lv_obj_t * btn = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t * label = lv_obj_get_child(btn, 0);

    // Change button text when successfully clicked
    lv_label_set_text(label, "ROGER COPIED!");
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x00FF00), 0); // Flash Green
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("--- FoxClock Interactive Setup ---");

    // Initialize display and touch tracking hardware
    display_init();
    touch_init();

    // Dark tactical background styling
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x0a0a0a), 0);

    // Create an interactive LVGL button widget
    lv_obj_t * btn = lv_button_create(lv_screen_active());
    lv_obj_set_size(btn, 180, 50);
    lv_obj_center(btn);
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, NULL);

    // Apply a tactical amber color aesthetic to the button
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFFB000), 0);

    // Label inside the button
    lv_obj_t * btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "TEST TOUCH");
    lv_obj_center(btn_label);
    lv_obj_set_style_text_color(btn_label, lv_color_hex(0x000000), 0); // Black text
}

void loop() {
    // Periodically updates both frame rendering and touch input evaluations
    display_update();
    delay(5);
}
