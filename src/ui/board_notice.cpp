#include "board_notice.h"
#include "theme.h"
#include "fonts.h"
#include "../hw/board.h"
#include "../hw/boards_gen.h"
#include "../config/config.h"
#include "../core/metadata.h"
#include <Arduino.h>
#include <Preferences.h>

namespace ui {
    namespace board_notice {

        namespace {
            constexpr const char* NS = "qrp_board";  // NVS: which firmware version already showed the notice
            constexpr uint32_t NO_TOUCH_CLOSE_MS = 30000;

            void restart_soon() {
                lv_timer_create([](lv_timer_t*) { ESP.restart(); }, 600, nullptr);
            }

            void save_board(const char* value) {
                strlcpy(config::mutable_get().board, value, sizeof(config::Config::board));
                config::save();
            }

            // A message box in the style of the sleep dialog (status_bar.cpp).
            lv_obj_t* box(const char* title, const char* text) {
                lv_obj_t* m = lv_msgbox_create(NULL);
                lv_msgbox_add_title(m, title);
                if (text) lv_msgbox_add_text(m, text);
                lv_obj_set_width(m, 290);
                lv_obj_set_style_bg_color(m, theme_color(COLOR_BG_PANEL), 0);
                lv_obj_set_style_border_color(m, theme_color(COLOR_BORDER), 0);
                lv_obj_set_style_border_width(m, 1, 0);
                lv_obj_set_style_text_color(m, theme_color(COLOR_TEXT_MAIN), 0);
                if (lv_obj_t* bg = lv_obj_get_parent(m)) {
                    lv_obj_set_style_bg_color(bg, lv_color_hex(0x000000), 0);  // dimming overlay in every theme
                    lv_obj_set_style_bg_opa(bg, 180, 0);
                }
                return m;
            }

            lv_obj_t* button(lv_obj_t* m, const char* text, bool primary, lv_event_cb_t cb, void* data = nullptr) {
                lv_obj_t* b = lv_msgbox_add_footer_button(m, text);
                lv_obj_set_style_bg_color(b, theme_color(primary ? COLOR_ACCENT_PRIMARY : COLOR_BORDER), 0);
                lv_obj_set_style_text_color(b, theme_color(primary ? COLOR_TEXT_ON_ACCENT : COLOR_TEXT_MAIN), 0);
                lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, data ? data : m);
                return b;
            }

            void close_box(lv_event_t* e) { lv_msgbox_close((lv_obj_t*)lv_event_get_user_data(e)); }

            bool untested() {
                const auto& s = hw::board::selection();
                return hw::board::active().status != hw::BoardStatus::Tested ||
                       s.decision == hw::rules::Decision::Ambiguous || s.decision == hw::rules::Decision::Unknown;
            }

            void mark_noted() {
                Preferences p;
                p.begin(NS, false);
                p.putString("note_fw", meta::FW_VERSION);
                p.end();
            }

            bool noted() {
                Preferences p;
                p.begin(NS, true);
                const bool done = p.getString("note_fw", "") == meta::FW_VERSION;
                p.end();
                return done;
            }

            // ---- mockup 3: untested board ----
            void show_untested() {
                if (!untested() || noted()) return;
                mark_noted();
                char text[200];
                const bool touch = hw::board::selection().touch_on;
                snprintf(text, sizeof(text), "%s hasn't been tested yet. Please tell us how it works: it takes a minute.%s",
                         hw::board::active().name, touch ? "" : "\nReport it in the web console (System Info) or over USB.");
                lv_obj_t* m = box("Untested board", text);
                if (touch) {
                    button(m, "Show report code", true, [](lv_event_t* e) {
                        close_box(e);
                        show_report_code();
                    });
                    button(m, "Later", false, close_box);
                } else {  // nothing could close it by touch
                    lv_timer_t* t = lv_timer_create([](lv_timer_t* tm) { lv_msgbox_close((lv_obj_t*)lv_timer_get_user_data(tm)); },
                                                    NO_TOUCH_CLOSE_MS, m);
                    lv_timer_set_repeat_count(t, 1);
                }
            }

            // ---- mockup 1, "Choose another": boards with the same screen chip ----
            void show_chooser() {
                lv_obj_t* m = box("Choose your board", nullptr);
                lv_obj_t* content = lv_msgbox_get_content(m);
                lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
                const hw::BoardDef& now = hw::board::active();
                for (size_t i = 0; i < hw::BOARD_COUNT; i++) {
                    const hw::BoardDef& b = hw::BOARDS[i];
                    if (!b.complete || b.init != now.init) continue;
                    lv_obj_t* btn = lv_button_create(content);
                    lv_obj_set_width(btn, lv_pct(100));
                    lv_obj_set_style_bg_color(btn, theme_color(&b == &now ? COLOR_ACCENT_PRIMARY : COLOR_BG_BAR), 0);
                    lv_obj_t* l = lv_label_create(btn);
                    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
                    lv_obj_set_width(l, lv_pct(100));
                    char t[80];
                    snprintf(t, sizeof(t), "%s%s", b.name, b.status == hw::BoardStatus::Tested ? "" : "  (untested)");
                    lv_label_set_text(l, t);
                    lv_obj_set_style_text_color(l, theme_color(&b == &now ? COLOR_TEXT_ON_ACCENT : COLOR_TEXT_MAIN), 0);
                    lv_obj_add_event_cb(btn, [](lv_event_t* e) {
                        const hw::BoardDef* b = (const hw::BoardDef*)lv_event_get_user_data(e);
                        save_board(b->id);
                        if (b != &hw::board::active()) restart_soon();
                        lv_msgbox_close(lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(e)))); }, LV_EVENT_CLICKED, (void*)&b);
                }
                button(m, "Cancel", false, close_box);
            }

            // ---- mockup 1: confirm the detected board ----
            void show_confirm() {
                char text[200];
                snprintf(text, sizeof(text), "Detected: %s.\nTurn the CYD over: does the number on the back match?",
                         hw::board::active().name);
                lv_obj_t* m = box("Board", text);
                button(m, "Correct", true, [](lv_event_t* e) {
                    save_board(hw::board::active().id);
                    close_box(e);
                    show_untested();
                });
                button(m, "Choose another", false, [](lv_event_t* e) {
                    close_box(e);
                    show_chooser();
                });
            }

            // ---- mockup 2: the choice contradicts the screen ----
            void show_mismatch() {
                char text[220];
                snprintf(text, sizeof(text),
                         "You chose %s, but the screen reports a different chip. It may not work. "
                         "Check the number on the back.",
                         hw::board::active().name);
                lv_obj_t* m = box("Board check", text);  // no warning symbol: not in the icons font
                button(m, "Use it anyway", false, [](lv_event_t* e) {
                    char v[sizeof(config::Config::board)];
                    snprintf(v, sizeof(v), "%s!", hw::board::active().id);
                    save_board(v);  // same board, same pins: no restart
                    close_box(e);
                    show_untested();
                });
                button(m, "Use detected", true, [](lv_event_t*) {
                    save_board("auto");
                    restart_soon();
                });
            }
        }  // namespace

        void start() {
            // After the dashboard has drawn once.
            lv_timer_t* t = lv_timer_create([](lv_timer_t*) {
                const auto& s = hw::board::selection();
                if (s.warn_mismatch && s.touch_on) show_mismatch();
                else if (s.ask_confirm) show_confirm();
                else show_untested();
            },
                                            1500, nullptr);
            lv_timer_set_repeat_count(t, 1);
        }

        // ---- mockup 4: the report code ----
        void show_report_code() {
            static char url[200];
            if (!hw::board::qr_url(url, sizeof(url))) return;
            lv_obj_t* scr = lv_obj_create(lv_layer_top());
            lv_obj_remove_style_all(scr);
            lv_obj_set_size(scr, 320, 240);
            lv_obj_set_style_bg_color(scr, theme_color(COLOR_BG_APP), 0);
            lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
            lv_obj_set_clickable(scr, true);
            lv_obj_add_event_cb(scr, [](lv_event_t* e) { lv_obj_delete((lv_obj_t*)lv_event_get_current_target(e)); }, LV_EVENT_CLICKED, nullptr);

            lv_obj_t* l = lv_label_create(scr);
            lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(l, 118);
            lv_obj_set_style_text_font(l, &font_atkinson_14, 0);
            lv_obj_set_style_text_color(l, theme_color(COLOR_TEXT_MAIN), 0);
            lv_label_set_text(l,
                              "Scan to report this board\n\nOpens GitHub with the board details filled in. Add the number on "
                              "the back and a photo.\n\nTap to close");
            lv_obj_align(l, LV_ALIGN_LEFT_MID, 8, 0);

            // Dark on white in every theme, with a light margin: phones read it reliably.
            lv_obj_t* qr = lv_qrcode_create(scr);
            lv_qrcode_set_size(qr, 180);
            lv_qrcode_set_dark_color(qr, lv_color_black());
            lv_qrcode_set_light_color(qr, lv_color_white());
            lv_qrcode_update(qr, url, strlen(url));
            lv_obj_set_style_border_color(qr, lv_color_white(), 0);
            lv_obj_set_style_border_width(qr, 6, 0);
            lv_obj_align(qr, LV_ALIGN_RIGHT_MID, -8, 0);
        }

        int add_network_line(lv_obj_t* page, int y) {
            if (!untested()) return 0;
            lv_obj_t* l = lv_label_create(page);
            char t[64];
            snprintf(t, sizeof(t), "Board: %s (untested)", hw::board::active().model);
            lv_label_set_text(l, t);
            lv_obj_set_style_text_font(l, &font_jetbrains_10, 0);
            lv_obj_set_style_text_color(l, theme_color(COLOR_STATUS_WARN), 0);
            lv_obj_align(l, LV_ALIGN_TOP_LEFT, 20, y + 6);
            if (!hw::board::selection().touch_on) return 26;
            lv_obj_t* b = lv_button_create(page);
            lv_obj_set_size(b, 70, 24);
            lv_obj_align(b, LV_ALIGN_TOP_RIGHT, -12, y);
            lv_obj_set_style_bg_color(b, theme_color(COLOR_BG_BAR), 0);
            lv_obj_set_style_border_color(b, theme_color(COLOR_BORDER), 0);
            lv_obj_set_style_border_width(b, 1, 0);
            lv_obj_t* bl = lv_label_create(b);
            lv_label_set_text(bl, "Report");
            lv_obj_set_style_text_color(bl, theme_color(COLOR_TEXT_MAIN), 0);
            lv_obj_center(bl);
            lv_obj_add_event_cb(b, [](lv_event_t*) { show_report_code(); }, LV_EVENT_CLICKED, nullptr);
            return 26;
        }

    }  // namespace board_notice
}  // namespace ui
