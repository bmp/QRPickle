#include "satellites.h"
#include "../band_badge.h"
#include "../layout.h"
#include "../theme.h"
#include "../fonts.h"
#include "../../config/config.h"
#include "../../services/space_manager.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <time.h>

namespace ui {

    using services::SpaceManager;
    namespace space = services::space;

    // Satellites screen (docs/UI_GUIDE.md): PASSES (the next passes, tap one for its times and
    // frequencies), EVENTS (ISS SSTV and school contacts) and GUIDE. Footer: feed age and refresh,
    // laid out like the band-conditions screen.
    namespace {
        enum Tab { TAB_PASSES,
                   TAB_EVENTS,
                   TAB_GUIDE,
                   TAB_COUNT };
        Tab active_tab = TAB_PASSES;

        constexpr int TAB_H = 24, FOOTER_H = 20;
        constexpr int BODY_Y = TAB_H, BODY_H = CONTENT_H - TAB_H - FOOTER_H;  // 172
        constexpr int FOOTER_Y = CONTENT_H - FOOTER_H;
        constexpr int REFRESH_X = SCREEN_W - 72, REFRESH_W = 28;
        constexpr int ROWS = 9, ROW_H = 15, ROW_Y = 15;  // and the geostationary line below

        const char* const CREDIT = "Data: SatNOGS DB (CC BY-SA 4.0), CelesTrak, ARISS.";

        struct Screen {
            lv_obj_t* root;
            lv_obj_t* body;
            lv_timer_t* timer;
            lv_obj_t* tab_btn[TAB_COUNT];
            lv_obj_t* tab_lbl[TAB_COUNT];
            lv_obj_t* row[ROWS];
            uint8_t row_pass[ROWS];  // index into passes() of each row
            lv_obj_t* geo;
            lv_obj_t* status;
            lv_obj_t* age;
            lv_obj_t* refresh_btn;
            uint32_t drawn_version;
            uint32_t drawn_minute;
            int detail;  // pass shown in detail, -1 = the list
            bool busy_tap;  // refresh refused (busy or too soon)
        };
        Screen s;

        lv_obj_t* text(lv_obj_t* parent, ThemeToken color, int x, int y, const lv_font_t* font = &font_jetbrains_10) {
            lv_obj_t* l = lv_label_create(parent);
            lv_obj_set_style_text_font(l, font, 0);
            lv_obj_set_style_text_color(l, theme_color(color), 0);
            lv_obj_set_pos(l, x, y);
            lv_label_set_text(l, "");
            return l;
        }

        lv_obj_t* rect(lv_obj_t* parent, int x, int y, int w, int h, ThemeToken color) {
            lv_obj_t* r = lv_obj_create(parent);
            lv_obj_set_size(r, w, h);
            lv_obj_set_pos(r, x, y);
            lv_obj_set_style_bg_color(r, theme_color(color), 0);
            lv_obj_set_style_border_width(r, 0, 0);
            lv_obj_set_style_radius(r, 0, 0);
            lv_obj_set_style_pad_all(r, 0, 0);
            lv_obj_set_scrollable(r, false);
            return r;
        }

        // "Mon 12 Oct 14:05" in local time.
        void local_day_time(char* out, size_t len, uint32_t utc) {
            static const char* const DAYS[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
            static const char* const MONTHS[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
            const time_t local = (time_t)utc + (time_t)config::get().tz_offset_hh * 1800;
            struct tm tm;
            gmtime_r(&local, &tm);
            snprintf(out, len, "%s %d %s %02d:%02d", DAYS[tm.tm_wday], tm.tm_mday, MONTHS[tm.tm_mon], tm.tm_hour,
                     tm.tm_min);
        }

        void freq_text(char* out, size_t len, uint32_t hz) { snprintf(out, len, "%lu.%03lu", (unsigned long)(hz / 1000000), (unsigned long)(hz / 1000 % 1000)); }

        void make_scrolling_text(lv_obj_t* body, const char* str) {
            lv_obj_set_scrollable(body, true);
            lv_obj_set_scroll_dir(body, LV_DIR_VER);
            lv_obj_t* l = text(body, COLOR_TEXT_MAIN, 8, 4, &font_atkinson_14);
            lv_obj_set_width(l, SCREEN_W - 24);
            lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
            lv_label_set_text(l, str);
        }

        // ---- PASSES tab ------------------------------------------------------------------------

        void show_detail(int pass_index);
        void show_tab(Tab t);

        void draw_passes_tab(lv_obj_t* body) {
            lv_obj_t* hdr = text(body, COLOR_TEXT_MUTED, 6, 1);
            lv_label_set_text(hdr, "LOCAL SATELLITE     MAX  FROM>TO  MIN");
            for (int i = 0; i < ROWS; i++) {
                s.row[i] = text(body, COLOR_TEXT_MAIN, 6, ROW_Y + i * ROW_H);
                lv_obj_set_size(s.row[i], SCREEN_W - 12, ROW_H);
                lv_obj_set_clickable(s.row[i], true);
                lv_obj_add_event_cb(
                    s.row[i],
                    [](lv_event_t* e) {
                        const int r = (int)(intptr_t)lv_event_get_user_data(e);
                        if (s.row_pass[r] != 0xFF) show_detail(s.row_pass[r]);
                    },
                    LV_EVENT_CLICKED, (void*)(intptr_t)i);
            }
            s.geo = text(body, COLOR_TEXT_MUTED, 6, ROW_Y + ROWS * ROW_H + 2);
        }

        void refresh_passes_tab() {
            const auto& v = SpaceManager::view();
            const uint32_t now = (uint32_t)time(nullptr);
            for (int i = 0; i < ROWS; i++) {
                s.row_pass[i] = 0xFF;
                lv_label_set_text(s.row[i], "");
            }
            if (!SpaceManager::lock(20)) return;
            const space::Pass* p = SpaceManager::passes();
            const space::SatInfo* sats = SpaceManager::sats();
            int r = 0;
            for (int i = 0; i < v.pass_count && r < ROWS; i++) {
                if (p[i].los <= now) continue;
                const bool up = p[i].aos <= now;
                char when[8], line[64];
                if (up) snprintf(when, sizeof(when), " NOW ");
                else band_local_hhmm(when, sizeof(when), p[i].aos);
                const char* mark = (p[i].flags & space::PASS_CONTACT)      ? " ARISS"
                                   : (p[i].flags & space::PASS_SSTV_EVENT) ? " SSTV"
                                                                           : "";
                snprintf(line, sizeof(line), "%-5s %-12.12s %2u\xc2\xb0 %3s>%-3s %3u%s", when, sats[p[i].sat].name,
                         p[i].max_el, space::compass(p[i].az_aos), space::compass(p[i].az_los),
                         (unsigned)((p[i].los - p[i].aos + 30) / 60), mark);
                lv_label_set_text(s.row[r], line);
                const ThemeToken c = up ? COLOR_ACCENT_PRIMARY : (*mark ? COLOR_BAND_FAIR : COLOR_TEXT_MAIN);
                lv_obj_set_style_text_color(s.row[r], theme_color(c), 0);
                s.row_pass[r++] = (uint8_t)i;
            }
            // Geostationary satellites (QO-100): no passes, just where they are.
            char geo[64] = "";
            for (int i = 0; i < v.sat_count; i++) {
                const int8_t el = SpaceManager::geo_elevation((uint8_t)i);
                if (el == -128) continue;
                snprintf(geo, sizeof(geo), "%s: %s (%d\xc2\xb0)", sats[i].name,
                         el > 0 ? "always in view" : "below your horizon", el);
                break;
            }
            SpaceManager::unlock();
            if (r == 0) {
                lv_label_set_text(s.row[0], !v.has_feed   ? "Waiting for the satellite feed..."
                                            : v.computing ? "Computing passes..."
                                                          : "No passes in the next 24 hours.");
                lv_obj_set_style_text_color(s.row[0], theme_color(COLOR_TEXT_MUTED), 0);
            }
            lv_label_set_text(s.geo, geo);
        }

        void show_detail(int pass_index) {
            if (!SpaceManager::lock(20)) return;
            const space::Pass p = SpaceManager::passes()[pass_index];
            const space::SatInfo info = SpaceManager::sats()[p.sat];
            const int n_ev = SpaceManager::view().event_count;
            space::Event ev[space::MAX_EVENTS];
            memcpy(ev, SpaceManager::events(), sizeof(space::Event) * n_ev);
            SpaceManager::unlock();

            s.detail = pass_index;
            lv_obj_clean(s.body);
            for (auto& r : s.row) r = nullptr;
            s.geo = nullptr;

            char* buf = static_cast<char*>(malloc(900));
            if (!buf) return;
            char aos[24], tca[8], los[8];
            local_day_time(aos, sizeof(aos), p.aos);
            band_local_hhmm(tca, sizeof(tca), p.tca);
            band_local_hhmm(los, sizeof(los), p.los);
            int n = snprintf(buf, 900, "%s (NORAD %lu)\nRises %s in the %s, highest %u\xc2\xb0 at %s, sets %s in the %s.\n",
                             info.name, (unsigned long)info.norad, aos, space::compass(p.az_aos), p.max_el, tca, los,
                             space::compass(p.az_los));
            space::Sat sat;
            if (SpaceManager::load_sat(p.sat, sat) && sat.norad == info.norad) {
                for (int i = 0; i < sat.tx_count && n < 820; i++) {
                    const space::Tx& t = sat.tx[i];
                    char d[16], u[16];
                    freq_text(d, sizeof(d), t.down_hz);
                    if (t.up_hz) {
                        freq_text(u, sizeof(u), t.up_hz);
                        n += snprintf(buf + n, 900 - n, "\nDown %s, up %s MHz %s", d, u, t.mode);
                    } else {
                        n += snprintf(buf + n, 900 - n, "\nDown %s MHz %s", d, t.mode);
                    }
                }
                if (!sat.tx_count) n += snprintf(buf + n, 900 - n, "\nNo frequencies listed.");
            } else {
                n += snprintf(buf + n, 900 - n, "\nFrequencies: the feed is being updated.");
            }
            for (int e = 0; e < n_ev && n < 820; e++) {
                if (!(p.aos < ev[e].end && p.los > ev[e].start)) continue;
                if (info.norad != space::ISS) break;
                if (ev[e].kind == space::EVENT_SSTV) {
                    char f[16] = "";
                    if (ev[e].freq_hz) freq_text(f, sizeof(f), ev[e].freq_hz);
                    n += snprintf(buf + n, 900 - n, "\n\nSSTV during this pass: %s%s%s%s", ev[e].title,
                                  *f ? ", " : "", f, *f ? " MHz" : "");
                } else {
                    n += snprintf(buf + n, 900 - n, "\n\nSchool contact during this pass: %s", ev[e].title);
                }
            }
            snprintf(buf + n, 900 - n, "\n\nTap to go back.");
            make_scrolling_text(s.body, buf);
            free(buf);
            lv_obj_t* label = lv_obj_get_child(s.body, 0);
            lv_obj_set_clickable(label, true);
            lv_obj_add_event_cb(label, [](lv_event_t*) { show_tab(TAB_PASSES); }, LV_EVENT_CLICKED, nullptr);
        }

        // ---- EVENTS tab ------------------------------------------------------------------------

        void draw_events_tab(lv_obj_t* body) {
            const auto& v = SpaceManager::view();
            if (!SpaceManager::lock(20)) return;
            const int n_ev = v.event_count, n_pass = v.pass_count;
            space::Event ev[space::MAX_EVENTS];
            memcpy(ev, SpaceManager::events(), sizeof(space::Event) * n_ev);
            // ISS passes per event (only those the pass list holds).
            uint8_t count[space::MAX_EVENTS] = {};
            uint32_t first[space::MAX_EVENTS] = {};
            const space::Pass* p = SpaceManager::passes();
            const space::SatInfo* sats = SpaceManager::sats();
            for (int i = 0; i < n_pass; i++) {
                if (sats[p[i].sat].norad != space::ISS) continue;
                for (int e = 0; e < n_ev; e++) {
                    if (p[i].aos < ev[e].end && p[i].los > ev[e].start) {
                        if (!count[e]++) first[e] = p[i].aos;
                    }
                }
            }
            SpaceManager::unlock();

            char* buf = static_cast<char*>(malloc(1800));
            if (!buf) return;
            int n = 0;
            for (int e = 0; e < n_ev && n < 1500; e++) {
                char from[24], to[24];
                local_day_time(from, sizeof(from), ev[e].start);
                if (ev[e].kind == space::EVENT_SSTV) {
                    local_day_time(to, sizeof(to), ev[e].end);
                    char f[16] = "";
                    if (ev[e].freq_hz) freq_text(f, sizeof(f), ev[e].freq_hz);
                    n += snprintf(buf + n, 1800 - n, "ISS SSTV: %s\n%s to %s\n%s%s%s %s\n", ev[e].title, from, to,
                                  *f ? f : "Frequency: see ariss.org", *f ? " MHz" : "", "", ev[e].mode);
                    if (count[e]) {
                        char t[8];
                        band_local_hhmm(t, sizeof(t), first[e]);
                        n += snprintf(buf + n, 1800 - n, "Passes over you: %u listed, the first at %s.\n\n", count[e], t);
                    } else {
                        n += snprintf(buf + n, 1800 - n, "No ISS pass over you in the list yet.\n\n");
                    }
                } else {
                    n += snprintf(buf + n, 1800 - n, "ARISS contact: %s\n%s. %s\n%s\n\n", ev[e].title, from,
                                  ev[e].detail,
                                  ev[e].audible_here ? "The ISS is above your horizon then: listen on 145.800 MHz FM."
                                                     : "The ISS is below your horizon then.");
                }
            }
            if (!n_ev)
                n += snprintf(buf + n, 1800 - n, "%s\n\n",
                              v.has_feed ? "No ISS SSTV events or school contacts announced right now."
                                         : "Waiting for the satellite feed...");
            snprintf(buf + n, 1800 - n, "%s", CREDIT);
            make_scrolling_text(body, buf);
            free(buf);
        }

        // ---- GUIDE tab -------------------------------------------------------------------------

        const char* const GUIDE_TEXT =
            "PASSES lists the next passes above your minimum elevation (web console, Satellites). "
            "Tap one for its times, directions and frequencies. NOW means it is up.\n\n"
            "FM satellites (SO-50, AO-91, the ISS) work with a dual-band handheld and a small Yagi: "
            "listen on the downlink, allow for Doppler (about 3 kHz on 70 cm), keep calls short.\n\n"
            "SSTV: during ISS SSTV events, listen on 145.800 MHz FM and decode the audio with an app "
            "(Robot36, MMSSTV). Passes in an event are marked SSTV; school contacts ARISS.\n\n"
            "LED: purple blinks when an SSTV or contact pass is due (lead time in the web console), "
            "and breathes while it is up.\n\n"
            "Passes are computed on the CYD from orbits (SGP4) updated every 6 hours. Your location "
            "never leaves the device.";

        void draw_guide_tab(lv_obj_t* body) {
            char buf[1100];
            snprintf(buf, sizeof(buf), "%s\n\n%s", GUIDE_TEXT, CREDIT);
            make_scrolling_text(body, buf);
        }

        // ---- Footer ----------------------------------------------------------------------------

        void draw_footer() {
            const auto& v = SpaceManager::view();
            const uint32_t now = (uint32_t)time(nullptr);
            char buf[48];
            if (v.computing) snprintf(buf, sizeof(buf), "computing passes...");
            else if (!v.has_feed) snprintf(buf, sizeof(buf), "waiting for the feed");
            else snprintf(buf, sizeof(buf), "%u satellites, %u passes", v.sat_count, v.pass_count);
            lv_label_set_text(s.status, buf);

            bool warn = false;
            if (s.busy_tap) {
                snprintf(buf, sizeof(buf), "try later");
            } else if (v.failed_utc && !v.has_feed) {
                snprintf(buf, sizeof(buf), "feed failed");
                warn = true;
            } else if (v.generated_utc) {
                char age[16];
                const uint32_t age_s = now > v.generated_utc ? now - v.generated_utc : 0;
                band_age_text(age, sizeof(age), age_s);
                snprintf(buf, sizeof(buf), "feed %s", age);
                warn = age_s > 24 * 3600;  // orbits a day old still give passes within a minute or so
            } else {
                buf[0] = '\0';
            }
            lv_label_set_text(s.age, buf);
            lv_obj_set_style_text_color(s.age, theme_color(warn ? COLOR_BAND_FAIR : COLOR_TEXT_MUTED), 0);
        }

        void refresh(bool force) {
            const auto& v = SpaceManager::view();
            const uint32_t minute = (uint32_t)time(nullptr) / 60;  // rows move on as passes end
            if (!force && v.version == s.drawn_version && minute == s.drawn_minute) {
                draw_footer();
                return;
            }
            s.drawn_version = v.version;
            s.drawn_minute = minute;
            if (active_tab == TAB_PASSES && s.detail < 0 && s.row[0]) refresh_passes_tab();
            draw_footer();
        }

        void show_tab(Tab t) {
            active_tab = t;
            s.detail = -1;
            for (int i = 0; i < TAB_COUNT; i++) {
                const bool on = i == t;
                lv_obj_set_style_text_color(s.tab_lbl[i], theme_color(on ? COLOR_ACCENT_PRIMARY : COLOR_TEXT_MUTED), 0);
                lv_obj_set_style_border_width(s.tab_btn[i], on ? 2 : 0, 0);
            }
            lv_obj_clean(s.body);  // only the open tab's objects exist (less LVGL memory)
            for (auto& r : s.row) r = nullptr;
            s.geo = nullptr;
            lv_obj_scroll_to_y(s.body, 0, LV_ANIM_OFF);
            lv_obj_set_scrollable(s.body, false);
            if (t == TAB_PASSES) draw_passes_tab(s.body);
            if (t == TAB_EVENTS) draw_events_tab(s.body);
            if (t == TAB_GUIDE) draw_guide_tab(s.body);
            refresh(true);
        }
    }  // namespace

    void draw_satellites_page(lv_obj_t* parent) {
        s = Screen{};
        s.detail = -1;
        s.root = lv_obj_create(parent);
        lv_obj_set_size(s.root, SCREEN_W, CONTENT_H);
        lv_obj_set_style_bg_color(s.root, theme_color(COLOR_BG_APP), 0);
        lv_obj_set_style_border_width(s.root, 0, 0);
        lv_obj_set_style_radius(s.root, 0, 0);
        lv_obj_set_style_pad_all(s.root, 0, 0);
        lv_obj_set_scrollable(s.root, false);

        static const char* const TAB_NAMES[TAB_COUNT] = {"PASSES", "EVENTS", "GUIDE"};
        lv_obj_t* tabs = rect(s.root, 0, 0, SCREEN_W, TAB_H, COLOR_BG_PANEL);
        const int tab_w = SCREEN_W / TAB_COUNT;
        for (int i = 0; i < TAB_COUNT; i++) {
            lv_obj_t* b = lv_button_create(tabs);
            lv_obj_set_size(b, i == TAB_COUNT - 1 ? SCREEN_W - i * tab_w : tab_w, TAB_H);
            lv_obj_set_pos(b, i * tab_w, 0);
            lv_obj_set_style_radius(b, 0, 0);
            lv_obj_set_style_shadow_width(b, 0, 0);
            lv_obj_set_style_bg_color(b, theme_color(COLOR_BG_PANEL), 0);
            lv_obj_set_style_border_side(b, LV_BORDER_SIDE_BOTTOM, 0);
            lv_obj_set_style_border_color(b, theme_color(COLOR_ACCENT_PRIMARY), 0);
            lv_obj_add_event_cb(
                b, [](lv_event_t* e) { show_tab((Tab)(intptr_t)lv_event_get_user_data(e)); }, LV_EVENT_CLICKED,
                (void*)(intptr_t)i);
            s.tab_btn[i] = b;
            s.tab_lbl[i] = lv_label_create(b);
            lv_label_set_text(s.tab_lbl[i], TAB_NAMES[i]);
            lv_obj_set_style_text_font(s.tab_lbl[i], &font_atkinson_14, 0);
            lv_obj_center(s.tab_lbl[i]);
        }

        s.body = rect(s.root, 0, BODY_Y, SCREEN_W, BODY_H, COLOR_BG_APP);

        rect(s.root, 0, FOOTER_Y, SCREEN_W, 1, COLOR_BORDER);
        s.status = text(s.root, COLOR_TEXT_MAIN, 6, FOOTER_Y + 5);
        lv_obj_set_width(s.status, 150);
        lv_label_set_long_mode(s.status, LV_LABEL_LONG_CLIP);
        s.age = text(s.root, COLOR_TEXT_MUTED, REFRESH_X - 6 - 84, FOOTER_Y + 5);
        lv_obj_set_width(s.age, 84);
        lv_label_set_long_mode(s.age, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(s.age, LV_TEXT_ALIGN_RIGHT, 0);
        s.refresh_btn = lv_button_create(s.root);
        lv_obj_set_size(s.refresh_btn, REFRESH_W, 16);
        lv_obj_set_pos(s.refresh_btn, REFRESH_X, FOOTER_Y + 2);
        lv_obj_set_style_bg_color(s.refresh_btn, theme_color(COLOR_BG_BUTTON), 0);
        lv_obj_set_style_pad_all(s.refresh_btn, 0, 0);
        lv_obj_t* icon = lv_label_create(s.refresh_btn);
        lv_label_set_text(icon, LV_SYMBOL_REFRESH);
        lv_obj_set_style_text_color(icon, theme_color(COLOR_TEXT_MAIN), 0);
        lv_obj_center(icon);
        lv_obj_add_event_cb(
            s.refresh_btn,
            [](lv_event_t*) {
                s.busy_tap = !SpaceManager::refresh_now();
                refresh(true);
            },
            LV_EVENT_CLICKED, nullptr);

        show_tab(active_tab);
        s.timer = lv_timer_create([](lv_timer_t*) { refresh(false); }, 2000, nullptr);
        lv_obj_add_event_cb(
            s.root,
            [](lv_event_t*) {
                if (s.timer) lv_timer_delete(s.timer);
                s = Screen{};
            },
            LV_EVENT_DELETE, nullptr);
    }
}  // namespace ui
