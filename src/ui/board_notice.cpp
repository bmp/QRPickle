#include "board_notice.h"
#include "theme.h"
#include "fonts.h"
#include "../hw/board.h"
#include "../hw/boards_gen.h"
#include "../config/config.h"
#include "../core/metadata.h"
#include <Arduino.h>
#include <Preferences.h>
#include <src/libs/qrcode/qrcodegen.h>

namespace ui {
    namespace board_notice {

        namespace {
            constexpr const char* NS = "qrp_board";  // NVS: which firmware version already showed the notice
            constexpr uint32_t NO_TOUCH_CLOSE_MS = 30000;

            constexpr int QR_MAX_VERSION = 10;  // up to 213 bytes at medium error correction
            constexpr int QR_PX = 190;          // white square incl. the quiet zone
            uint8_t qr_modules[qrcodegen_BUFFER_LEN_FOR_VERSION(QR_MAX_VERSION)];

            void draw_qr(lv_event_t* e) {
                lv_obj_t* obj = (lv_obj_t*)lv_event_get_current_target(e);
                lv_layer_t* layer = lv_event_get_layer(e);
                const int n = qrcodegen_getSize(qr_modules);
                const int scale = QR_PX / (n + 4);  // at least 2 modules of quiet zone on each side
                lv_area_t box;
                lv_obj_get_coords(obj, &box);
                const int x0 = box.x1 + (QR_PX - n * scale) / 2, y0 = box.y1 + (QR_PX - n * scale) / 2;
                lv_draw_rect_dsc_t d;
                lv_draw_rect_dsc_init(&d);
                d.bg_color = lv_color_black();
                d.bg_opa = LV_OPA_COVER;
                d.radius = 0;
                for (int y = 0; y < n; y++) {
                    for (int x = 0; x < n;) {
                        if (!qrcodegen_getModule(qr_modules, x, y)) {
                            x++;
                            continue;
                        }
                        int run = x;
                        while (run < n && qrcodegen_getModule(qr_modules, run, y)) run++;
                        lv_area_t a = {x0 + x * scale, y0 + y * scale, x0 + run * scale - 1, y0 + y * scale + scale - 1};
                        lv_draw_rect(layer, &d, &a);
                        x = run;
                    }
                }
            }

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
            lv_obj_set_width(l, 104);
            lv_obj_set_style_text_font(l, &font_atkinson_14, 0);
            lv_obj_set_style_text_color(l, theme_color(COLOR_TEXT_MAIN), 0);
            lv_label_set_text(l,
                              "Scan to report this board\n\nOpens GitHub with the board details filled in. Add the number on "
                              "the back and a photo.\n\nTap to close");
            lv_obj_align(l, LV_ALIGN_LEFT_MID, 8, 0);

            // Dark modules on white, drawn as rectangles (runs per row). LVGL's QR widget renders a
            // 1-bit image that LVGL 9 can only draw after converting it to 32-bit colour (~129 KB for
            // 180 px), far beyond this board's heap; qrcodegen's bitmap here is ~400 bytes.
            uint8_t temp[qrcodegen_BUFFER_LEN_FOR_VERSION(QR_MAX_VERSION)];
            if (!qrcodegen_encodeText(url, temp, qr_modules, qrcodegen_Ecc_MEDIUM, 1, QR_MAX_VERSION, qrcodegen_Mask_AUTO,
                                      false))
                return;
            lv_obj_t* qr = lv_obj_create(scr);
            lv_obj_remove_style_all(qr);
            lv_obj_set_size(qr, QR_PX, QR_PX);
            lv_obj_set_style_bg_color(qr, lv_color_white(), 0);
            lv_obj_set_style_bg_opa(qr, LV_OPA_COVER, 0);
            lv_obj_add_event_cb(qr, draw_qr, LV_EVENT_DRAW_MAIN, nullptr);
            lv_obj_align(qr, LV_ALIGN_RIGHT_MID, -8, 0);
        }

        int add_network_line(lv_obj_t* page, int y) {
            if (!untested()) return 0;
            // One line, tappable when the board has touch: the Reconnect/Scan buttons start ~16 px below.
            lv_obj_t* l = lv_label_create(page);
            char t[72];
            snprintf(t, sizeof(t), "%s untested%s", hw::board::active().model,
                     hw::board::selection().touch_on ? " - tap to report" : ": report in the web console");
            lv_label_set_text(l, t);
            lv_obj_set_style_text_font(l, &font_jetbrains_10, 0);
            lv_obj_set_style_text_color(l, theme_color(COLOR_STATUS_WARN), 0);
            lv_obj_align(l, LV_ALIGN_TOP_LEFT, 20, y);
            if (hw::board::selection().touch_on) {
                lv_obj_set_clickable(l, true);
                lv_obj_add_event_cb(l, [](lv_event_t*) { show_report_code(); }, LV_EVENT_CLICKED, nullptr);
            }
            return 14;
        }

    }  // namespace board_notice
}  // namespace ui
