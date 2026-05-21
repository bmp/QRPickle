#include "ui.h"
#include "../hw/sensor.h"          // Link the environmental sensor metrics layer
#include "../services/wifi_manager.h" // THE FIX: Allow the UI to query background network status
#include "../core/timekeeper.h" // Allow the UI to pull live network time strings
#include "../core/config_manager.h" // Direct hook into flash runtime memory mapping structures
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

// Keeps track of the currently active display screen object reference
static lv_obj_t * current_screen = NULL;

// Persistent Global Settings Buffer: Stores callsign state across page changes
// static char station_call[12] = "N0CALL";

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
    // Extract the parent panel screen instance passed via user data
    lv_obj_t * parent = (lv_obj_t *)lv_timer_get_user_data(timer);

    // Retrieve the target sub-labels by their internal child indices
    lv_obj_t * utc_lbl = lv_obj_get_child(parent, 0);
    lv_obj_t * loc_lbl = lv_obj_get_child(parent, 1);

    // Safety check: verify both label instances exist before writing text
    if (!utc_lbl || !loc_lbl) return;

    // Fetch up-to-date formatted metrics from the background time core
    // (e.g. "23:45:12 UTC" and "05:15:12 LOC")
    const char * utc_text = timekeeper_get_utc_string();
    const char * local_text = timekeeper_get_local_string();

    // Push the updated string arrays straight to the glass layout layers
    lv_label_set_text(utc_lbl, utc_text);
    lv_label_set_text(loc_lbl, local_text);
}

static void draw_clock_page(lv_obj_t * parent) {
    // 1. Primary High-Visibility UTC Display Label
    lv_obj_t * utc_lbl = lv_label_create(parent);
    lv_label_set_text(utc_lbl, "00:00:00 UTC");
    lv_obj_set_style_text_font(utc_lbl, &lv_font_montserrat_32, 0); // Massive 32pt Font
    lv_obj_set_style_text_color(utc_lbl, lv_color_hex(0xFFB000), 0); // Tactical Amber
    lv_obj_align(utc_lbl, LV_ALIGN_CENTER, 0, -45);                  // Shift upward

    // 2. Secondary Local Display Label
    lv_obj_t * loc_lbl = lv_label_create(parent);
    lv_label_set_text(loc_lbl, "00:00:00 LOC");
    lv_obj_set_style_text_font(loc_lbl, &lv_font_montserrat_24, 0); // Balanced 24pt Font
    lv_obj_set_style_text_color(loc_lbl, lv_color_hex(0xCCA040), 0); // Muted Amber/Orange
    lv_obj_align(loc_lbl, LV_ALIGN_CENTER, 0, -5);                   // Stack beneath UTC

    // 3. Persistent Station Callsign Watermark
    lv_obj_t * call_lbl = lv_label_create(parent);
    // Fetch directly from storage cache so your callsign displays right on the main screen!
    lv_label_set_text(call_lbl, config_get_runtime()->station_call);
    lv_obj_set_style_text_font(call_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(call_lbl, lv_color_hex(0x444444), 0); // Stealth Low-Contrast Gray
    lv_obj_align(call_lbl, LV_ALIGN_TOP_RIGHT, -15, 15);              // Lock to top corner

    // Spawn our quick 200ms background visual refresh task loop
    // Passing 'parent' as user data lets the callback manipulate both sub-labels cleanly
    lv_timer_t * c_timer = lv_timer_create(clock_timer_cb, 200, parent);

    // Dynamic garbage collection: scrap the timer the instant this screen is unmounted
    lv_obj_add_event_cb(parent, [](lv_event_t * e) {
        lv_timer_t * tmr = (lv_timer_t *)lv_event_get_user_data(e);
        lv_timer_delete(tmr);
    }, LV_EVENT_DELETE, c_timer);
}

static void weather_timer_cb(lv_timer_t * timer) {
    // Extract the parent page container
    lv_obj_t * parent = (lv_obj_t *)lv_timer_get_user_data(timer);

    // Retrieve our UI components by their internal child hierarchy indices
    lv_obj_t * lbl = lv_obj_get_child(parent, 0);
    lv_obj_t * chart = lv_obj_get_child(parent, 1);

    if (!lbl || !chart) return;

    // Fetch fresh metrics from your physical environmental sensor core
    float t = sensor_get_temp();
    float h = sensor_get_humidity();
    float p = sensor_get_pressure();

    // 1. Update the textual metrics dashboard layout
    char buf[128];
    snprintf(buf, sizeof(buf), "TEMP: %.1f C  |  HUMIDITY: %.1f %%\nBARO: %.1f hPa", t, h, p);
    lv_label_set_text(lbl, buf);

    // 2. Feed the new barometric pressure reading into the live line chart!
    // Extract the series configuration pointer we attached to the chart's user data slot
    lv_chart_series_t * ser = (lv_chart_series_t *)lv_obj_get_user_data(chart);
    if (ser) {
        // Cast float pressure directly to an integer matching our chart scale (e.g. 1013 hPa)
        lv_chart_set_next_value(chart, ser, (int32_t)p);
    }
}

static void draw_weather_page(lv_obj_t * parent) {
    // [Component 0]: Text Metrics Header
    lv_obj_t * lbl = lv_label_create(parent);
    lv_label_set_text(lbl, "TEMP: --.- C  |  HUMIDITY: --.-\nBARO: ---- hPa");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x33FF33), 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 20, 15);

    // [Component 1]: Telemetry Barograph Trend Line Chart
    lv_obj_t * chart = lv_chart_create(parent);
    lv_obj_set_size(chart, 280, 100);
    lv_obj_align(chart, LV_ALIGN_TOP_MID, 0, 65);

    lv_obj_set_style_bg_color(chart, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_color(chart, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(chart, 1, 0);

    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, 20);

    // --- THE ELEVATION FIX: DYNAMIC ACCORDION SCALING ---
    // 1. Snag your exact local ambient pressure right now at startup
    float boot_pressure = sensor_get_pressure();

    // 2. Set a tight 10 hPa span centered exactly on your local altitude baseline.
    // This shifts the window down to match Bengaluru's atmosphere perfectly!
    int32_t y_min = (int32_t)(boot_pressure - 5.0f);
    int32_t y_max = (int32_t)(boot_pressure + 5.0f);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, y_min, y_max);

    lv_obj_set_style_line_color(chart, lv_color_hex(0x222222), LV_PART_ITEMS);
    lv_obj_set_style_line_width(chart, 1, LV_PART_ITEMS);

    lv_chart_series_t * ser = lv_chart_add_series(chart, lv_color_hex(0x33FF33), LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_set_user_data(chart, ser);

    // Prime the rolling timeline history using your actual boot pressure
    for(int i = 0; i < 20; i++) {
        lv_chart_set_next_value(chart, ser, (int32_t)boot_pressure);
    }

    lv_timer_t * w_timer = lv_timer_create(weather_timer_cb, 1000, parent);

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
            // 1. Fetch our active deep flash memory data pointer structure
            sys_config_t * cfg = config_get_runtime();

            // 2. Extract the keyboard string array input and write it straight to the system configuration
            strncpy(cfg->station_call, lv_textarea_get_text(ta), sizeof(cfg->station_call) - 1);
            cfg->station_call[sizeof(cfg->station_call) - 1] = '\0';

            // 3. Commit this change to the storage device immediately!
            config_manager_save();
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

    // Look down near the text-area creation inside draw_settings_page...
    lv_obj_t * ta = lv_textarea_create(parent);
    lv_obj_set_size(ta, 140, 38);
    lv_obj_align(ta, LV_ALIGN_TOP_LEFT, 140, 74);

    // THE CHANGE: Populate textbox with the callsign loaded straight from LittleFS partition configuration files
    lv_textarea_set_text(ta, config_get_runtime()->station_call);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, 10);

    lv_obj_set_style_bg_color(ta, lv_color_hex(0x1C1C1C), 0);
    lv_obj_set_style_border_color(ta, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_text_color(ta, lv_color_hex(0x33FF33), 0);
    lv_obj_set_style_radius(ta, 4, 0);

    lv_obj_add_event_cb(ta, textarea_event_cb, LV_EVENT_ALL, NULL);
}
