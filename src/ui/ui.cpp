#include "ui.h"
#include "../hw/sensor.h"          // Link the environmental sensor metrics layer
#include "../services/wifi_manager.h" // THE FIX: Allow the UI to query background network status
#include "../core/timekeeper.h" // Allow the UI to pull live network time strings
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

static void clock_timer_cb(lv_timer_t * timer) {
    lv_obj_t * lbl = (lv_obj_t *)lv_timer_get_user_data(timer);

    char buf[64];
    // Fetch accurate time data directly out of our background time core
    snprintf(buf, sizeof(buf), "%s\n%s", timekeeper_get_utc_string(), timekeeper_get_local_string());

    lv_label_set_text(lbl, buf);
}

static void draw_clock_page(lv_obj_t * parent) {
    lv_obj_t * lbl = lv_label_create(parent);
    lv_label_set_text(lbl, "AWAITING SYNC\n00:00:00 LOC");
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFB000), 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, -30);

    // Spawn a high-speed 200ms refresher to capture rolling digital clock changes flawlessly
    lv_timer_t * c_timer = lv_timer_create(clock_timer_cb, 200, lbl);

    // Self-cleaning protection: delete the timer instance if we navigate away from this screen container
    lv_obj_add_event_cb(parent, [](lv_event_t * e) {
        lv_timer_t * tmr = (lv_timer_t *)lv_event_get_user_data(e);
        lv_timer_delete(tmr);
    }, LV_EVENT_DELETE, c_timer);
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

static void keyboard_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * kb = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t * ta = (lv_obj_t *)lv_event_get_user_data(e);

    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        if (code == LV_EVENT_READY) {
            strncpy(station_call, lv_textarea_get_text(ta), sizeof(station_call) - 1);
            station_call[sizeof(station_call) - 1] = '\0';
        }
        lv_obj_remove_state(ta, LV_STATE_FOCUSED);
        lv_obj_delete(kb);
    }
}

static void textarea_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t * parent_scr = lv_obj_get_screen(ta);

    if (code == LV_EVENT_CLICKED) {
        lv_obj_t * kb = lv_keyboard_create(parent_scr);
        lv_obj_set_size(kb, 320, 130);
        lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);

        lv_obj_set_style_bg_color(kb, lv_color_hex(0x1A1A1A), 0);
        lv_obj_set_style_text_color(kb, lv_color_hex(0xFFB000), 0);

        lv_keyboard_set_textarea(kb, ta);
        lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_UPPER);
        lv_obj_add_event_cb(kb, keyboard_event_cb, LV_EVENT_ALL, ta);
    }
}

static void wifi_timer_cb(lv_timer_t * timer) {
    lv_obj_t * lbl = (lv_obj_t *)lv_timer_get_user_data(timer);
    char buf[128];
    snprintf(buf, sizeof(buf), "Wi-Fi Link Status: %s", wifi_manager_get_status_text());
    lv_label_set_text(lbl, buf);
}

static void draw_settings_page(lv_obj_t * parent) {
    lv_obj_t * title_lbl = lv_label_create(parent);
    lv_label_set_text(title_lbl, "SYSTEM CONFIGURATION");
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(0x00FFFF), 0);
    lv_obj_align(title_lbl, LV_ALIGN_TOP_LEFT, 20, 15);

    lv_obj_t * wifi_lbl = lv_label_create(parent);
    lv_label_set_text(wifi_lbl, "Wi-Fi Link Status: INITIALIZING...");
    lv_obj_set_style_text_color(wifi_lbl, lv_color_hex(0x888888), 0);
    lv_obj_align(wifi_lbl, LV_ALIGN_TOP_LEFT, 20, 45);

    lv_timer_t * w_timer = lv_timer_create(wifi_timer_cb, 1000, wifi_lbl);

    lv_obj_add_event_cb(parent, [](lv_event_t * e) {
        lv_timer_t * tmr = (lv_timer_t *)lv_event_get_user_data(e);
        lv_timer_delete(tmr);
    }, LV_EVENT_DELETE, w_timer);

    lv_obj_t * call_lbl = lv_label_create(parent);
    lv_label_set_text(call_lbl, "Station Call:");
    lv_obj_set_style_text_color(call_lbl, lv_color_hex(0x00FFFF), 0);
    lv_obj_align(call_lbl, LV_ALIGN_TOP_LEFT, 20, 85);

    lv_obj_t * ta = lv_textarea_create(parent);
    lv_obj_set_size(ta, 140, 38);
    lv_obj_align(ta, LV_ALIGN_TOP_LEFT, 140, 74);

    lv_textarea_set_text(ta, station_call);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, 10);

    lv_obj_set_style_bg_color(ta, lv_color_hex(0x1C1C1C), 0);
    lv_obj_set_style_border_color(ta, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_text_color(ta, lv_color_hex(0x33FF33), 0);
    lv_obj_set_style_radius(ta, 4, 0);

    lv_obj_add_event_cb(ta, textarea_event_cb, LV_EVENT_ALL, NULL);
}
