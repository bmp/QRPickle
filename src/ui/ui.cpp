#include "ui.h"
#include "../hw/sensor.h"
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

// Keeps track of the currently active display screen object reference
static lv_obj_t * current_screen = NULL;

// Persistent Global Settings Buffer: Stores callsign state across page changes
static char station_call[12] = "N0CALL";

// Forward declarations for internal component subroutines
static void draw_clock_page(lv_obj_t * parent);
static void draw_weather_page(lv_obj_t * parent);
static void draw_settings_page(lv_obj_t * parent);
static void add_navigation_bar(lv_obj_t * parent, ui_page_t active_page);

void ui_navigate_to(ui_page_t target_page) {
    lv_obj_t * next_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(next_screen, lv_color_hex(0x0A0A0A), 0);

    switch (target_page) {
        case PAGE_CLOCK:    draw_clock_page(next_screen); break;
        case PAGE_WEATHER:  draw_weather_page(next_screen); break;
        case PAGE_SETTINGS: draw_settings_page(next_screen); break;
    }

    add_navigation_bar(next_screen, target_page);

    lv_screen_load_anim(next_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 250, 0, true);
    current_screen = next_screen;
}

// --- Shared Navigation Bar Component ---
static void add_navigation_bar(lv_obj_t * parent, ui_page_t active_page) {
    lv_obj_t * nav_bar = lv_obj_create(parent);
    lv_obj_set_size(nav_bar, 320, 45);
    lv_obj_align(nav_bar, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_set_style_bg_color(nav_bar, lv_color_hex(0x141414), 0);
    lv_obj_set_style_border_color(nav_bar, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(nav_bar, 1, 0);
    lv_obj_set_style_radius(nav_bar, 0, 0);
    lv_obj_set_style_pad_all(nav_bar, 2, 0);

    lv_obj_set_layout(nav_bar, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(nav_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(nav_bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    const char * btn_labels[] = {"TIME", "WX", "SET"};
    ui_page_t btn_targets[] = {PAGE_CLOCK, PAGE_WEATHER, PAGE_SETTINGS};

    for (int i = 0; i < 3; i++) {
        lv_obj_t * btn = lv_button_create(nav_bar);
        lv_obj_set_size(btn, 90, 34);

        lv_obj_t * lbl = lv_label_create(btn);
        lv_label_set_text(lbl, btn_labels[i]);
        lv_obj_center(lbl);

        if (active_page == btn_targets[i]) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0xFFB000), 0);
            lv_obj_set_style_text_color(lbl, lv_color_hex(0x0A0A0A), 0);
        } else {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x2A2A2A), 0);
            lv_obj_set_style_text_color(lbl, lv_color_hex(0xDDDDDD), 0);
        }

        ui_page_t target = btn_targets[i];
        lv_obj_add_event_cb(btn, [](lv_event_t * e) {
            ui_page_t tgt = (ui_page_t)(uintptr_t)lv_event_get_user_data(e);
            ui_navigate_to(tgt);
        }, LV_EVENT_CLICKED, (void*)(uintptr_t)target);
    }
}

// --- Page Content Layout Painters ---

static void draw_clock_page(lv_obj_t * parent) {
    lv_obj_t * lbl = lv_label_create(parent);
    lv_label_set_text(lbl, "00:00:00 UTC\n12:00:00 LOC");
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFB000), 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, -30);
}

static void weather_timer_cb(lv_timer_t * timer) {
    lv_obj_t * lbl = (lv_obj_t *)lv_timer_get_user_data(timer);
    float t = sensor_get_temp();
    float h = sensor_get_humidity();
    float p = sensor_get_pressure();

    char buf[128];
    snprintf(buf, sizeof(buf), "TEMPERATURE: %.1f C\nHUMIDITY: %.1f %%\nBARO: %.1f hPa", t, h, p);
    lv_label_set_text(lbl, buf);
}

static void draw_weather_page(lv_obj_t * parent) {
    lv_obj_t * lbl = lv_label_create(parent);
    lv_label_set_text(lbl, "TEMPERATURE: --.- C\nHUMIDITY: --.-\nBARO: ---- hPa");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x33FF33), 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 20, -30);

    lv_timer_t * w_timer = lv_timer_create(weather_timer_cb, 1000, lbl);

    lv_obj_add_event_cb(parent, [](lv_event_t * e) {
        lv_timer_t * tmr = (lv_timer_t *)lv_event_get_user_data(e);
        lv_timer_delete(tmr);
    }, LV_EVENT_DELETE, w_timer);
}

// Callback macro to manage Keyboard lifecycle configurations
static void keyboard_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    // Explicit typecast from void* to lv_obj_t* resolved C++ pointer compiler errors
    lv_obj_t * kb = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t * ta = (lv_obj_t *)lv_event_get_user_data(e);

    // If the user clicks the checkmark (Apply) or close button on the keyboard layout
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        if (code == LV_EVENT_READY) {
            // Save the newly input string straight into our persistent text array buffer
            strncpy(station_call, lv_textarea_get_text(ta), sizeof(station_call) - 1);
            station_call[sizeof(station_call) - 1] = '\0'; // Guarantee safe string termination
        }

        // Remove focus from text area and cleanly delete the keyboard object from RAM
        lv_obj_remove_state(ta, LV_STATE_FOCUSED);
        lv_obj_delete(kb);
    }
}

// Callback macro triggered when the user taps on the interactive Text Area box
static void textarea_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    // Explicit typecast from void* to lv_obj_t* resolved C++ pointer compiler errors
    lv_obj_t * ta = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t * parent_scr = lv_obj_get_screen(ta);

    if (code == LV_EVENT_CLICKED) {
        // Instantiate a sliding visual keyboard directly over the parent workspace canvas
        lv_obj_t * kb = lv_keyboard_create(parent_scr);
        lv_obj_set_size(kb, 320, 130);
        lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);

        // Optimize layout layout properties for tactical look
        lv_obj_set_style_bg_color(kb, lv_color_hex(0x1A1A1A), 0);
        lv_obj_set_style_text_color(kb, lv_color_hex(0xFFB000), 0);

        // Bind the keyboard tracking engine to input text frames explicitly into this text area box
        lv_keyboard_set_textarea(kb, ta);

        // Enforce upper-case character layouts by default for cleaner Ham Radio Call Sign entry
        lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_UPPER);

        // Attach completion and cancel handler callbacks to the keyboard entity
        lv_obj_add_event_cb(kb, keyboard_event_cb, LV_EVENT_ALL, ta);
    }
}

static void draw_settings_page(lv_obj_t * parent) {
    // 1. Static Configuration Section Heading Label
    lv_obj_t * title_lbl = lv_label_create(parent);
    lv_label_set_text(title_lbl, "SYSTEM CONFIGURATION");
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(0x00FFFF), 0); // Cyber Cyan
    lv_obj_align(title_lbl, LV_ALIGN_TOP_LEFT, 20, 15);

    // 2. Wi-Fi Status read-out trace
    lv_obj_t * wifi_lbl = lv_label_create(parent);
    lv_label_set_text(wifi_lbl, "Wi-Fi Link Status: DISCONNECTED");
    lv_obj_set_style_text_color(wifi_lbl, lv_color_hex(0x888888), 0);
    lv_obj_align(wifi_lbl, LV_ALIGN_TOP_LEFT, 20, 45);

    // 3. Station Identification Field descriptor label
    lv_obj_t * call_lbl = lv_label_create(parent);
    lv_label_set_text(call_lbl, "Station Call:");
    lv_obj_set_style_text_color(call_lbl, lv_color_hex(0x00FFFF), 0);
    lv_obj_align(call_lbl, LV_ALIGN_TOP_LEFT, 20, 85);

    // 4. Interactive Data Field Container Component (Text Area)
    lv_obj_t * ta = lv_textarea_create(parent);
    lv_obj_set_size(ta, 140, 38);
    lv_obj_align(ta, LV_ALIGN_TOP_LEFT, 140, 74);

    // Bind current text state configurations from persistent buffer allocation
    lv_textarea_set_text(ta, station_call);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, 10);

    // Apply dark high-contrast panel design overrides to the text box
    lv_obj_set_style_bg_color(ta, lv_color_hex(0x1C1C1C), 0);
    lv_obj_set_style_border_color(ta, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_text_color(ta, lv_color_hex(0x33FF33), 0); // Terminal Green Entry text
    lv_obj_set_style_radius(ta, 4, 0);

    // Bind event trigger to spawn virtual keyboard upon cursor click focus
    lv_obj_add_event_cb(ta, textarea_event_cb, LV_EVENT_ALL, NULL);
}
