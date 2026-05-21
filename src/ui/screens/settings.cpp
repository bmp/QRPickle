#include "settings.h"
#include "../keyboard.h"
#include "../status_bar.h"
#include "../theme.h"
#include "../fonts.h"
#include "../../config/config.h"
#include "../../config/config_validation.h"
#include "../../hw/display.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <string.h>
#include <vector>

namespace ui {

    static lv_obj_t* scr = nullptr;
    static lv_obj_t* form = nullptr;
    static SettingsCallbacks callbacks{};

    static lv_obj_t* ta_call = nullptr;
    static lv_obj_t* ta_grid = nullptr;
    static lv_obj_t* ta_ssid = nullptr;
    static lv_obj_t* ta_pw = nullptr;
    static lv_obj_t* lbl_pw_masked = nullptr;
    static lv_obj_t* btn_pw_eye = nullptr;
    static lv_obj_t* lbl_tz = nullptr;
    static lv_obj_t* slider_bright = nullptr;
    static lv_obj_t* btn_save = nullptr;
    static lv_obj_t* btn_theme = nullptr;
    static lv_obj_t* lbl_theme = nullptr;

    static lv_obj_t* btn_minus = nullptr;
    static lv_obj_t* btn_plus = nullptr;
    static lv_obj_t* btn_exit = nullptr;
    static lv_obj_t* btn_reboot = nullptr;
    static bool pw_visible = false;

    // FIXED: Active profile list tracking state vectors
    static lv_obj_t* btn_profile = nullptr;
    static lv_obj_t* lbl_profile = nullptr;
    static std::vector<String> profile_list;
    static int current_profile_idx = -1;

    // FIXED: Isolated pending staging registers to guard against early parameter corruption
    static int8_t pending_tz_offset_hh = 0;
    static uint8_t pending_theme_id = 0;
    static char pending_owm_api_key[41] = "";

    static void settings_refresh_theme() {
        if (!scr || !form) return;

        lv_color_t bg_app = theme_color(COLOR_BG_APP);
        lv_color_t bg_panel = theme_color(COLOR_BG_PANEL);
        lv_color_t txt_main = theme_color(COLOR_TEXT_MAIN);
        lv_color_t txt_muted = theme_color(COLOR_TEXT_MUTED);
        lv_color_t accent = theme_color(COLOR_ACCENT_PRIMARY);
        lv_color_t border = theme_color(COLOR_BORDER);

        lv_obj_set_style_bg_color(scr, bg_app, 0);
        lv_obj_set_style_bg_color(form, bg_app, 0);

        if (ta_call) { lv_obj_set_style_bg_color(ta_call, bg_panel, 0); lv_obj_set_style_text_color(ta_call, txt_main, 0); lv_obj_set_style_border_color(ta_call, border, 0); }
        if (ta_grid) { lv_obj_set_style_bg_color(ta_grid, bg_panel, 0); lv_obj_set_style_text_color(ta_grid, txt_main, 0); lv_obj_set_style_border_color(ta_grid, border, 0); }
        if (ta_ssid) { lv_obj_set_style_bg_color(ta_ssid, bg_panel, 0); lv_obj_set_style_text_color(ta_ssid, txt_main, 0); lv_obj_set_style_border_color(ta_ssid, border, 0); }
        if (ta_pw)   { lv_obj_set_style_bg_color(ta_pw, bg_panel, 0);   lv_obj_set_style_text_color(ta_pw, txt_main, 0);   lv_obj_set_style_border_color(ta_pw, border, 0); }

        if (lbl_pw_masked) lv_obj_set_style_text_color(lbl_pw_masked, txt_muted, 0);
        if (btn_pw_eye)    lv_obj_set_style_text_color(btn_pw_eye, accent, 0);
        if (lbl_tz)        lv_obj_set_style_text_color(lbl_tz, txt_main, 0);

        if (btn_theme)     { lv_obj_set_style_bg_color(btn_theme, bg_panel, 0); lv_obj_set_style_border_color(btn_theme, border, 0); }
        if (lbl_theme)     lv_obj_set_style_text_color(lbl_theme, txt_main, 0);

        // THEMED: Profiles list container dropdown button hooks
        if (btn_profile)   { lv_obj_set_style_bg_color(btn_profile, bg_panel, 0); lv_obj_set_style_border_color(btn_profile, border, 0); }
        if (lbl_profile)   lv_obj_set_style_text_color(lbl_profile, txt_main, 0);

        if (slider_bright) {
            lv_obj_set_style_bg_color(slider_bright, bg_panel, LV_PART_MAIN);
            lv_obj_set_style_bg_color(slider_bright, accent, LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(slider_bright, txt_main, LV_PART_KNOB);
        }

        if (btn_minus) {
            lv_obj_set_style_bg_color(btn_minus, bg_panel, 0);
            lv_obj_set_style_border_color(btn_minus, border, 0);
            lv_obj_set_style_border_width(btn_minus, 1, 0);
            lv_obj_t* lbl = lv_obj_get_child(btn_minus, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, txt_main, 0);
        }
        if (btn_plus) {
            lv_obj_set_style_bg_color(btn_plus, bg_panel, 0);
            lv_obj_set_style_border_color(btn_plus, border, 0);
            lv_obj_set_style_border_width(btn_plus, 1, 0);
            lv_obj_t* lbl = lv_obj_get_child(btn_plus, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, txt_main, 0);
        }

        if (btn_exit) {
            lv_obj_set_style_bg_color(btn_exit, bg_panel, 0);
            lv_obj_set_style_border_color(btn_exit, border, 0);
            lv_obj_set_style_border_width(btn_exit, 1, 0);
            lv_obj_t* lbl = lv_obj_get_child(btn_exit, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, txt_main, 0);
        }
        if (btn_reboot) {
            lv_obj_set_style_bg_color(btn_reboot, theme_color(COLOR_BAND_POOR), 0);
            lv_obj_t* lbl = lv_obj_get_child(btn_reboot, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, bg_app, 0);
        }

        if (btn_save) {
            lv_obj_t* label = lv_obj_get_child(btn_save, 0);
            if (strcmp(lv_label_get_text(label), "SAVE") == 0) {
                lv_obj_set_style_bg_color(btn_save, accent, 0);
                lv_obj_set_style_text_color(label, bg_app, 0);
            }
        }

        uint32_t child_count = lv_obj_get_child_count(form);
        for (uint32_t i = 0; i < child_count; i++) {
            lv_obj_t* child = lv_obj_get_child(form, i);
            if (lv_obj_get_class(child) == &lv_label_class) {
                lv_obj_set_style_text_color(child, txt_muted, 0);
            }
        }
    }

    static void render_tz(int8_t hh) {
        char buf[12];
        int whole = hh / 2;
        int half  = abs(hh % 2) * 5;
        snprintf(buf, sizeof(buf), "%+d.%d", whole, half);
        if (lbl_tz) lv_label_set_text(lbl_tz, buf);
    }

    static void theme_clicked(lv_event_t* e) {
        pending_theme_id = (pending_theme_id + 1) % THEME_COUNT;
        if (lbl_theme) {
            lv_label_set_text(lbl_theme, theme_get_name(pending_theme_id));
        }
    }

    // FIXED: Processes profile filesystem reads and applies values to the staging components instantly
    static void profile_clicked(lv_event_t* e) {
        if (profile_list.empty()) return;

        current_profile_idx = (current_profile_idx + 1) % profile_list.size();
        String name = profile_list[current_profile_idx];

        if (lbl_profile) {
            String clean_name = name;
            clean_name.replace("_", " ");
            lv_label_set_text(lbl_profile, clean_name.c_str());
        }

        File file = LittleFS.open("/profiles/" + name + ".json", "r");
        if (!file) return;

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, file);
        file.close();

        if (!err) {
            if (!doc["callsign"].isNull())   lv_textarea_set_text(ta_call, doc["callsign"]);
            if (!doc["grid"].isNull())       lv_textarea_set_text(ta_grid, doc["grid"]);
            if (!doc["ssid"].isNull())       lv_textarea_set_text(ta_ssid, doc["ssid"]);
            if (!doc["password"].isNull())   lv_textarea_set_text(ta_pw, doc["password"]);
            if (!doc["brightness"].isNull()) lv_slider_set_value(slider_bright, doc["brightness"].as<int>(), LV_ANIM_OFF);
            if (!doc["theme_id"].isNull()) {
                pending_theme_id = doc["theme_id"].as<uint8_t>();
                if (lbl_theme) lv_label_set_text(lbl_theme, theme_get_name(pending_theme_id));
            }
            if (!doc["offset"].isNull()) {
                pending_tz_offset_hh = (int8_t)(doc["offset"].as<float>() * 2.0f);
                render_tz(pending_tz_offset_hh);
            }
        }
    }

    static void show_pw(bool visible) {
        pw_visible = visible;
        lv_label_set_text(btn_pw_eye, visible ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
        if (visible) {
            lv_obj_clear_flag(ta_pw, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(lbl_pw_masked, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(ta_pw, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(lbl_pw_masked, LV_OBJ_FLAG_HIDDEN);
        }
    }

    static void open_kb_for(lv_obj_t* target, KeyboardMode mode) {
        static lv_obj_t* current_target = nullptr;
        current_target = target;

        KeyboardCallbacks kcb;
        kcb.on_accept = [](const char* t) {
            if (current_target) lv_textarea_set_text(current_target, t);
        };
            kcb.on_cancel = []{};

            keyboard_show(mode, lv_textarea_get_text(target), kcb);
    }

    static void tz_minus(lv_event_t*) {
        pending_tz_offset_hh = config::clamp_tz_hh((int)pending_tz_offset_hh - 1);
        render_tz(pending_tz_offset_hh);
    }

    static void tz_plus(lv_event_t*) {
        pending_tz_offset_hh = config::clamp_tz_hh((int)pending_tz_offset_hh + 1);
        render_tz(pending_tz_offset_hh);
    }

    static void bright_changed(lv_event_t* e) {
        // Handled strictly inside the master modification engine validation sequence
    }

    // FIXED: Evaluates input changes against NVS state and applies modifications instantly
    static void save_clicked(lv_event_t*) {
        const auto& c = config::get();
        bool modified = false;

        const char* new_call = lv_textarea_get_text(ta_call);
        const char* new_grid = lv_textarea_get_text(ta_grid);
        const char* new_ssid = lv_textarea_get_text(ta_ssid);
        const char* new_pass = lv_textarea_get_text(ta_pw);
        uint8_t new_bright = (uint8_t)lv_slider_get_value(slider_bright);

        if (strcmp(c.callsign, new_call) != 0 ||
            strcmp(c.grid, new_grid) != 0 ||
            strcmp(c.wifi_ssid, new_ssid) != 0 ||
            strcmp(c.wifi_password, new_pass) != 0 ||
            strcmp(c.openweather_api_key, pending_owm_api_key) != 0 ||
            c.tz_offset_hh != pending_tz_offset_hh ||
            c.brightness != new_bright ||
            c.theme_id != pending_theme_id)
        {
            modified = true;
        }

        lv_obj_t* lbl = lv_obj_get_child(btn_save, 0);

        if (modified) {
            auto& mc = config::mutable_get();

            strncpy(mc.callsign, new_call, sizeof(mc.callsign) - 1);
            config::normalize_callsign(mc.callsign, sizeof(mc.callsign));

            strncpy(mc.grid, new_grid, sizeof(mc.grid) - 1);
            config::normalize_grid(mc.grid, sizeof(mc.grid));

            strncpy(mc.wifi_ssid, new_ssid, sizeof(mc.wifi_ssid) - 1);
            strncpy(mc.wifi_password, new_pass, sizeof(mc.wifi_password) - 1);
            strncpy(mc.openweather_api_key, pending_owm_api_key, sizeof(mc.openweather_api_key) - 1);

            mc.tz_offset_hh = pending_tz_offset_hh;
            mc.brightness = new_bright;
            mc.theme_id = pending_theme_id;

            config::save();

            // HOT-RELOAD: Force global layout layers to update instantly
            status_bar_refresh_theme();
            settings_refresh_theme();

            lv_label_set_text(lbl, "SAVED!");
            lv_obj_set_style_bg_color(btn_save, lv_color_hex(0x3FB950), 0);
            lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
        } else {
            lv_label_set_text(lbl, "NO CHANGES");
        }
    }

    static void exit_clicked(lv_event_t*) { if (callbacks.on_exit) callbacks.on_exit(); }
    static void reboot_clicked(lv_event_t*) { if (callbacks.on_reboot) callbacks.on_reboot(); }

    static lv_obj_t* make_label(lv_obj_t* parent, const char* text) {
        lv_obj_t* l = lv_label_create(parent);
        lv_label_set_text(l, text);
        return l;
    }

    lv_obj_t* settings_create(SettingsCallbacks cb) {
        callbacks = cb;
        const auto& c = config::get();

        // Populate initial verification values
        pending_tz_offset_hh = c.tz_offset_hh;
        pending_theme_id = c.theme_id;

        // FIXED: Query available filesystems to fetch profile names dynamically
        profile_list.clear();
        current_profile_idx = -1;
        File dir = LittleFS.open("/profiles");
        if (dir && dir.isDirectory()) {
            File file = dir.openNextFile();
            while (file) {
                String name = String(file.name());
                if (name.endsWith(".json")) {
                    name.replace(".json", "");
                    profile_list.push_back(name);
                }
                file = dir.openNextFile();
            }
        }

        scr = lv_obj_create(lv_screen_active());
        lv_obj_set_size(scr, 320, 216);
        lv_obj_align(scr, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_border_width(scr, 0, 0);
        lv_obj_set_style_pad_all(scr, 0, 0);
        lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

        status_bar_set_title("Settings");

        form = lv_obj_create(scr);
        lv_obj_set_size(form, 320, 216);
        lv_obj_align(form, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_border_width(form, 0, 0);
        lv_obj_set_style_pad_all(form, 8, 0);
        lv_obj_set_flex_flow(form, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_scroll_dir(form, LV_DIR_VER);

        make_label(form, "Callsign");
        ta_call = lv_textarea_create(form);
        lv_textarea_set_one_line(ta_call, true);
        lv_textarea_set_max_length(ta_call, 11);
        lv_textarea_set_text(ta_call, c.callsign);
        lv_obj_set_width(ta_call, lv_pct(100));
        lv_obj_add_event_cb(ta_call, [](lv_event_t* e){ open_kb_for((lv_obj_t*)lv_event_get_target(e), KB_CALLSIGN); }, LV_EVENT_FOCUSED, NULL);

        make_label(form, "Grid Square");
        ta_grid = lv_textarea_create(form);
        lv_textarea_set_one_line(ta_grid, true);
        lv_textarea_set_max_length(ta_grid, 6);
        lv_textarea_set_text(ta_grid, c.grid);
        lv_obj_set_width(ta_grid, lv_pct(100));
        lv_obj_add_event_cb(ta_grid, [](lv_event_t* e){ open_kb_for((lv_obj_t*)lv_event_get_target(e), KB_GRID); }, LV_EVENT_FOCUSED, NULL);

        make_label(form, "WiFi SSID");
        ta_ssid = lv_textarea_create(form);
        lv_textarea_set_one_line(ta_ssid, true);
        lv_textarea_set_text(ta_ssid, c.wifi_ssid);
        lv_obj_set_width(ta_ssid, lv_pct(100));
        lv_obj_add_event_cb(ta_ssid, [](lv_event_t* e){ open_kb_for((lv_obj_t*)lv_event_get_target(e), KB_TEXT); }, LV_EVENT_FOCUSED, NULL);

        {
            lv_obj_t* row = lv_obj_create(form);
            lv_obj_set_size(row, lv_pct(100), 44);
            lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_pad_all(row, 0, 0);
            make_label(row, "WiFi Password");

            ta_pw = lv_textarea_create(row);
            lv_textarea_set_one_line(ta_pw, true);
            lv_textarea_set_text(ta_pw, c.wifi_password);
            lv_obj_set_size(ta_pw, 240, 26);
            lv_obj_align(ta_pw, LV_ALIGN_BOTTOM_LEFT, 0, 0);
            lv_obj_add_flag(ta_pw, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_event_cb(ta_pw, [](lv_event_t* e){ open_kb_for((lv_obj_t*)lv_event_get_target(e), KB_TEXT); }, LV_EVENT_FOCUSED, NULL);

            lbl_pw_masked = lv_label_create(row);
            lv_label_set_text(lbl_pw_masked, "********");
            lv_obj_align(lbl_pw_masked, LV_ALIGN_BOTTOM_LEFT, 4, -4);

            btn_pw_eye = lv_label_create(row);
            lv_label_set_text(btn_pw_eye, LV_SYMBOL_EYE_OPEN);
            lv_obj_align(btn_pw_eye, LV_ALIGN_BOTTOM_RIGHT, -4, -4);
            lv_obj_add_flag(btn_pw_eye, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(btn_pw_eye, [](lv_event_t*){ show_pw(!pw_visible); }, LV_EVENT_CLICKED, NULL);
        }

        {
            lv_obj_t* row = lv_obj_create(form);
            lv_obj_set_size(row, lv_pct(100), 44);
            lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_pad_all(row, 0, 0);
            make_label(row, "UTC Offset (Hours)");

            btn_minus = lv_button_create(row);
            lv_obj_set_size(btn_minus, 36, 26);
            lv_obj_align(btn_minus, LV_ALIGN_BOTTOM_LEFT, 130, 0);
            lv_obj_add_event_cb(btn_minus, tz_minus, LV_EVENT_CLICKED, NULL);
            lv_obj_t* mlbl = lv_label_create(btn_minus); lv_label_set_text(mlbl, "-"); lv_obj_center(mlbl);
            lv_obj_set_style_text_font(mlbl, &font_atkinson_14, 0);

            lbl_tz = lv_label_create(row);
            render_tz(pending_tz_offset_hh);
            lv_obj_align(lbl_tz, LV_ALIGN_BOTTOM_LEFT, 180, -4);
            lv_obj_set_style_text_font(lbl_tz, &font_jetbrains_14, 0);

            btn_plus = lv_button_create(row);
            lv_obj_set_size(btn_plus, 36, 26);
            lv_obj_align(btn_plus, LV_ALIGN_BOTTOM_LEFT, 230, 0);
            lv_obj_add_event_cb(btn_plus, tz_plus, LV_EVENT_CLICKED, NULL);
            lv_obj_t* plbl = lv_label_create(btn_plus); lv_label_set_text(plbl, "+"); lv_obj_center(plbl);
            lv_obj_set_style_text_font(plbl, &font_atkinson_14, 0);
        }

        make_label(form, "Brightness");
        slider_bright = lv_slider_create(form);
        lv_slider_set_range(slider_bright, 10, 255);
        lv_slider_set_value(slider_bright, c.brightness, LV_ANIM_OFF);
        lv_obj_set_width(slider_bright, lv_pct(100));
        lv_obj_add_event_cb(slider_bright, bright_changed, LV_EVENT_VALUE_CHANGED, NULL);

        make_label(form, "UI Theme");
        btn_theme = lv_btn_create(form);
        lv_obj_set_width(btn_theme, lv_pct(100));
        lv_obj_set_height(btn_theme, 30);
        lv_obj_set_style_border_width(btn_theme, 1, 0);
        lv_obj_set_style_radius(btn_theme, 4, 0);
        lv_obj_add_event_cb(btn_theme, theme_clicked, LV_EVENT_CLICKED, NULL);

        lbl_theme = lv_label_create(btn_theme);
        lv_label_set_text(lbl_theme, theme_get_name(pending_theme_id));
        lv_obj_set_style_text_font(lbl_theme, &font_atkinson_14, 0);
        lv_obj_center(lbl_theme);

        // FIXED: Profile Selection Click Switch Container Control
        make_label(form, "Staged Deployment Profile");
        btn_profile = lv_btn_create(form);
        lv_obj_set_width(btn_profile, lv_pct(100));
        lv_obj_set_height(btn_profile, 30);
        lv_obj_set_style_border_width(btn_profile, 1, 0);
        lv_obj_set_style_radius(btn_profile, 4, 0);
        lv_obj_add_event_cb(btn_profile, profile_clicked, LV_EVENT_CLICKED, NULL);

        lbl_profile = lv_label_create(btn_profile);
        lv_label_set_text(lbl_profile, profile_list.empty() ? "No Profiles Found on FS" : "Tap to Cycle Stored Profiles...");
        lv_obj_set_style_text_font(lbl_profile, &font_atkinson_14, 0);
        lv_obj_center(lbl_profile);

        lv_obj_t* spacer = lv_obj_create(form);
        lv_obj_set_size(spacer, lv_pct(100), 10);
        lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(spacer, 0, 0);

        {
            lv_obj_t* row = lv_obj_create(form);
            lv_obj_set_size(row, lv_pct(100), 44);
            lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_pad_all(row, 0, 0);

            btn_save = lv_button_create(row);
            lv_obj_set_size(btn_save, 90, 34);
            lv_obj_align(btn_save, LV_ALIGN_LEFT_MID, 0, 0);
            lv_obj_add_event_cb(btn_save, save_clicked, LV_EVENT_CLICKED, NULL);
            lv_obj_t* slbl = lv_label_create(btn_save); lv_label_set_text(slbl, "SAVE"); lv_obj_center(slbl);
            lv_obj_set_style_text_font(slbl, &font_atkinson_14, 0);

            btn_exit = lv_button_create(row);
            lv_obj_set_size(btn_exit, 90, 34);
            lv_obj_align(btn_exit, LV_ALIGN_CENTER, 0, 0);
            lv_obj_add_event_cb(btn_exit, exit_clicked, LV_EVENT_CLICKED, NULL);
            lv_obj_t* elbl = lv_label_create(btn_exit); lv_label_set_text(elbl, "EXIT"); lv_obj_center(elbl);
            lv_obj_set_style_text_font(elbl, &font_atkinson_14, 0);

            btn_reboot = lv_button_create(row);
            lv_obj_set_size(btn_reboot, 90, 34);
            lv_obj_align(btn_reboot, LV_ALIGN_RIGHT_MID, 0, 0);
            lv_obj_add_event_cb(btn_reboot, reboot_clicked, LV_EVENT_CLICKED, NULL);
            lv_obj_t* rlbl = lv_label_create(btn_reboot); lv_label_set_text(rlbl, "REBOOT"); lv_obj_center(rlbl);
            lv_obj_set_style_text_font(rlbl, &font_atkinson_14, 0);
        }

        settings_refresh_theme();
        return scr;
    }

    void settings_destroy() {
        if (scr) {
            lv_obj_delete_async(scr);
            scr = nullptr;
            form = nullptr;
            btn_minus = nullptr;
            btn_plus = nullptr;
            btn_exit = nullptr;
            btn_reboot = nullptr;
            btn_profile = nullptr;
            lbl_profile = nullptr;
        }
    }

}  // namespace ui
