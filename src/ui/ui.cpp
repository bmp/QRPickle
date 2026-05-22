#include "ui.h"
#include "status_bar.h"
#include "sidebar.h"
#include "home_button.h"
#include "theme.h"
#include "screens/dashboard.h" 
#include "screens/settings.h"
#include "screens/weather.h"
#include "screens/splash.h"
#include "screens/network.h"
#include "screens/spots.h"
#include "screens/xota.h"
#include "screens/aprs.h"
#include "../services/aprs_manager.h"
#include "fonts.h"
#include "../config/config.h"
#include "../hw/sensor.h"
#include "../core/timekeeper.h"
#include "../services/wifi_manager.h"
#include <lvgl.h>
#include <WiFi.h>
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

namespace ui {

    void ui_init();
    void ui_navigate_local(LocalPage page);
    static void global_ui_timer_cb(lv_timer_t* timer);
    static void sidebar_selection_cb(DestScreen dest);
    static void open_settings_screen();
    void display_update();

    static lv_obj_t* view_container = nullptr;
    static lv_obj_t* status_bar_obj = nullptr;
    static lv_timer_t* global_timer = nullptr;
    static LocalPage active_page = PAGE_DASHBOARD;
    static lv_obj_t* global_home_btn = nullptr; 

    static void global_ui_timer_cb(lv_timer_t* timer) {
        const char* full_time = timekeeper_get_local_string();

        if (full_time != nullptr && strlen(full_time) >= 5) {
            char short_time[6];
            strncpy(short_time, full_time, 5);
            short_time[5] = '\0';
            status_bar_set_clock(short_time);
        } else {
            status_bar_set_clock("--:--");
        }

        status_bar_set_wifi(WiFi.isConnected());
    }

    static void sidebar_selection_cb(DestScreen dest) {
        if (active_page == PAGE_SETTINGS) {
            settings_destroy(); 
        }

        if (dest == DEST_DASHBOARD) {
            ui_navigate_local(PAGE_DASHBOARD);
        } else if (dest == DEST_WEATHER) {
            ui_navigate_local(PAGE_WEATHER);
        } else if (dest == DEST_NETWORK) {
            ui_navigate_local(PAGE_NETWORK);
        } else if (dest == DEST_SETTINGS) {
            open_settings_screen();
        } else if (dest == DEST_SPOTS) { 
            ui_navigate_local(PAGE_SPOTS);
        } else if (dest == DEST_XOTA) { // FIXED: Route directly to unified xOTA module
            ui_navigate_local(PAGE_XOTA);
        } else if (dest == DEST_APRS) {
            ui_navigate_local(PAGE_APRS); // Routes to APRS
        }
    }

    static void open_settings_screen() {
        sidebar_hide(); 
        if (global_home_btn) {
            lv_obj_clear_flag(global_home_btn, LV_OBJ_FLAG_HIDDEN); 
            lv_obj_move_foreground(global_home_btn); 
        }

        SettingsCallbacks scb;
        scb.on_exit = []() {
            settings_destroy();
            ui_navigate_local(PAGE_DASHBOARD);
        };
        scb.on_reboot = []() { ESP.restart(); };

        settings_create(scb);
        active_page = PAGE_SETTINGS; 
    }

    void ui_init() {
        lv_obj_t* main_screen = lv_screen_active();
        lv_obj_set_style_bg_color(main_screen, lv_color_hex(0x000000), 0);

        view_container = lv_obj_create(main_screen);
        lv_obj_set_size(view_container, 320, 240);
        lv_obj_align(view_container, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_opa(view_container, 0, 0);
        lv_obj_set_style_border_width(view_container, 0, 0);
        lv_obj_set_style_pad_all(view_container, 0, 0);
        lv_obj_clear_flag(view_container, LV_OBJ_FLAG_SCROLLABLE);

        StatusBarCallbacks sb_cb;
        sb_cb.on_menu = []() { sidebar_toggle(); };
        sb_cb.on_settings = []() { open_settings_screen(); };
        sb_cb.on_network = []() { ui_navigate_local(PAGE_NETWORK); };

        status_bar_obj = status_bar_create(main_screen, sb_cb);
        lv_obj_add_flag(status_bar_obj, LV_OBJ_FLAG_HIDDEN);

        SidebarCallbacks sb_panels;
        sb_panels.on_select = [](DestScreen dest) { sidebar_selection_cb(dest); };

        sidebar_init(main_screen, sb_panels);

        draw_splash_screen(view_container, []() {
            lv_obj_clean(view_container);
            lv_obj_clear_flag(status_bar_obj, LV_OBJ_FLAG_HIDDEN);

            lv_obj_set_size(view_container, 320, 216);
            lv_obj_align(view_container, LV_ALIGN_BOTTOM_MID, 0, 0);

            global_timer = lv_timer_create(global_ui_timer_cb, 500, nullptr);

            global_home_btn = home_button_create(lv_screen_active(), []() {
                if (active_page == PAGE_SETTINGS) settings_destroy();
                ui_navigate_local(PAGE_DASHBOARD);
            });

            if (config::get().aprs_enabled) {
                services::AprsManager::start();
            }

            ui_navigate_local(PAGE_DASHBOARD);
        });
    }

    void ui_navigate_local(LocalPage page) {
        if (page == PAGE_SETTINGS) {
            open_settings_screen();
            return;
        }

        active_page = page;
        if (!view_container) return;

        lv_obj_clean(view_container);

        if (page == PAGE_DASHBOARD) {
            if (global_home_btn) lv_obj_add_flag(global_home_btn, LV_OBJ_FLAG_HIDDEN); 

            char buf[32];
            snprintf(buf, sizeof(buf), "%s @ %s", config::get().callsign, config::get().grid);
            status_bar_set_title(buf);
            
            lv_obj_set_style_bg_color(view_container, theme_color(COLOR_BG_APP), 0);
            draw_dashboard_page(view_container);
        } else {
            if (global_home_btn) {
                lv_obj_clear_flag(global_home_btn, LV_OBJ_FLAG_HIDDEN); 
                lv_obj_move_foreground(global_home_btn); 
            }

            if (page == PAGE_WEATHER) {
                status_bar_set_title("Weather");
                draw_weather_page(view_container);
            } else if (page == PAGE_NETWORK) {
                status_bar_set_title("Network");
                draw_network_page(view_container);
            } else if (page == PAGE_SPOTS) { 
                status_bar_set_title("DX Cluster");
                draw_spots_page(view_container);
            } else if (page == PAGE_XOTA) { // FIXED: Route Mount 
                status_bar_set_title("xOTA");
                draw_xota_page(view_container);
            } else if (page == PAGE_APRS) {
                status_bar_set_title("APRS Radar");
                draw_aprs_page(view_container);
            }
        }
    }

    void display_update() {
        static uint32_t last_tick = millis();
        uint32_t current_tick = millis();

        lv_tick_inc(current_tick - last_tick);
        last_tick = current_tick;

        lv_timer_handler();
    }

}  // namespace ui
