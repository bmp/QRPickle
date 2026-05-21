#include "ui.h"
#include <lvgl.h>

static void splash_timer_cb(lv_timer_t * timer) {
    // Drop the placeholder code and route straight into the main clock page
    ui_navigate_to(PAGE_CLOCK);

    // Clean up the timer memory allocation safely
    lv_timer_delete(timer);
}

void ui_init() {
    lv_obj_t * splash_screen = lv_screen_active();
    lv_obj_set_style_bg_color(splash_screen, lv_color_hex(0x0A0A0A), 0);

    lv_obj_t * title_label = lv_label_create(splash_screen);
    lv_label_set_text(title_label, "FOXCLOCK");
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFB000), 0);
    lv_obj_align(title_label, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t * sub_label = lv_label_create(splash_screen);
    lv_label_set_text(sub_label, "SYSTEM INITIALIZING...");
    lv_obj_set_style_text_color(sub_label, lv_color_hex(0x555555), 0);
    lv_obj_align(sub_label, LV_ALIGN_CENTER, 0, 15);

    lv_obj_t * spinner = lv_spinner_create(splash_screen);
    lv_obj_set_size(spinner, 30, 30);
    lv_obj_align(spinner, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(0xFFB000), LV_PART_INDICATOR);

    lv_timer_create(splash_timer_cb, 3000, NULL);
}
