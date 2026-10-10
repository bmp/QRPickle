#include "band_cond.h"
#include "../band_badge.h"
#include "../layout.h"
#include "../theme.h"
#include "../fonts.h"
#include "../../services/prop_manager.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <time.h>

namespace ui {

    using services::PropagationManager;
    namespace bands = services::bands;
    namespace solar = services::solar;

    // Band-conditions screen (docs/UI_GUIDE.md): solar values (left), every band rated for day and
    // night with the current column highlighted (right), and the light/sun time, data age and a
    // rate-limited refresh button (bottom).
    namespace {
        constexpr int PANE_X = 146;  // left pane 0..141, divider at 142
        constexpr int COL_DAY_X = 206, COL_NIGHT_X = 258, COL_W = 34;
        constexpr int ROW_H = 14, FOOTER_Y = CONTENT_H - 20;
        constexpr int SOLAR_ROWS = 10;
        // Refresh button left of the global home button (bottom-right corner, home_button.cpp).
        constexpr int REFRESH_X = SCREEN_W - 72, REFRESH_W = 28;

        struct Screen {
            lv_obj_t* root;
            lv_timer_t* timer;
            lv_obj_t* heading;
            lv_obj_t* name[SOLAR_ROWS];
            lv_obj_t* value[SOLAR_ROWS];
            lv_obj_t* desc[SOLAR_ROWS];
            lv_obj_t* now_bar;
            BandBadge day[bands::BAND_COUNT];
            BandBadge night[bands::BAND_COUNT];
            lv_obj_t* sun;
            lv_obj_t* age;
            lv_obj_t* refresh_btn;
            uint32_t drawn_version;
            bool show_unlock;  // after a tap while rate-limited
        };
        Screen s;

        lv_obj_t* text(lv_obj_t* parent, ThemeToken color, int x, int y) {
            lv_obj_t* l = lv_label_create(parent);
            lv_obj_set_style_text_font(l, &font_jetbrains_10, 0);
            lv_obj_set_style_text_color(l, theme_color(color), 0);
            lv_obj_set_pos(l, x, y);
            lv_label_set_text(l, "");
            return l;
        }

        void hline(lv_obj_t* parent, int x, int y, int w) {
            lv_obj_t* l = lv_obj_create(parent);
            lv_obj_set_size(l, w, 1);
            lv_obj_set_pos(l, x, y);
            lv_obj_set_style_bg_color(l, theme_color(COLOR_BORDER), 0);
            lv_obj_set_style_border_width(l, 0, 0);
            lv_obj_set_style_radius(l, 0, 0);
        }

        // Quality for HF of one solar value: GOOD/FAIR/POOR colour, or main text when neutral.
        enum Q { Q_NONE,
                 Q_GOOD,
                 Q_FAIR,
                 Q_POOR };
        lv_color_t q_color(Q q) {
            switch (q) {
                case Q_GOOD: return theme_color(COLOR_BAND_GOOD);
                case Q_FAIR: return theme_color(COLOR_BAND_FAIR);
                case Q_POOR: return theme_color(COLOR_BAND_POOR);
                default:     return theme_color(COLOR_TEXT_MAIN);
            }
        }

        void set_row(int i, const char* name, const char* value, const char* desc, Q q) {
            lv_label_set_text(s.name[i], name);
            lv_label_set_text(s.value[i], value);
            lv_obj_set_style_text_color(s.value[i], q_color(q), 0);
            lv_label_set_text(s.desc[i], desc);
        }

        void draw_solar(const services::PropagationTelemetry& tel, bool storm) {
            const auto& d = tel.solar;
            const bool ok = tel.has_data;
            char v[16];
            auto num = [&](int16_t x) {
                if (!ok || x == solar::NONE) snprintf(v, sizeof(v), "--");
                else snprintf(v, sizeof(v), "%d", x);
                return v;
            };

            if (storm) {
                char h[32];
                snprintf(h, sizeof(h), LV_SYMBOL_WARNING " STORM  K %d  A %d", d.k_index, d.a_index);
                lv_label_set_text(s.heading, h);
                lv_obj_set_style_text_color(s.heading, theme_color(COLOR_BAND_POOR), 0);
            } else {
                lv_label_set_text(s.heading, "SOLAR");
                lv_obj_set_style_text_color(s.heading, theme_color(COLOR_TEXT_MUTED), 0);
            }

            const Q sfi_q = d.sfi >= 120 ? Q_GOOD : (d.sfi >= 90 ? Q_FAIR : Q_POOR);
            set_row(0, "SFI", num(d.sfi), !ok ? "" : (d.sfi >= 130 ? "HIGH" : (d.sfi >= 95 ? "MED" : "LOW")),
                    ok ? sfi_q : Q_NONE);
            set_row(1, "SSN", num(d.sunspots), "", Q_NONE);
            const Q a_q = d.a_index < 20 ? Q_GOOD : (d.a_index < 30 ? Q_FAIR : Q_POOR);
            set_row(2, "A", num(d.a_index), "", ok && d.a_index != solar::NONE ? a_q : Q_NONE);
            const Q k_q = d.k_index <= 2 ? Q_GOOD : (d.k_index <= 4 ? Q_FAIR : Q_POOR);
            set_row(3, "K", num(d.k_index), ok ? d.geomag : "", ok ? k_q : Q_NONE);

            const char c = d.xray[0];
            const Q x_q = (c == 'M' || c == 'X') ? Q_POOR : (c == 'C' ? Q_FAIR : Q_GOOD);
            const char* flare = c == 'X' ? "X FLARE" : (c == 'M' ? "M FLARE" : (c == 'C' ? "C FLARE" : "NO FLARE"));
            set_row(4, "X-RAY", ok && c ? d.xray : "--", ok && c ? flare : "", ok && c ? x_q : Q_NONE);

            const bool wind = ok && d.solar_wind >= 0;
            if (wind) snprintf(v, sizeof(v), "%.0f", d.solar_wind);
            const Q w_q = d.solar_wind > 700 ? Q_POOR : (d.solar_wind > 500 ? Q_FAIR : Q_GOOD);
            set_row(5, "WIND", wind ? v : "--", wind ? "km/s" : "", wind ? w_q : Q_NONE);

            char bz[16] = "--";
            const bool has_bz = ok && !std::isnan(d.bz);
            if (has_bz) snprintf(bz, sizeof(bz), "%.1f", d.bz);
            const Q bz_q = d.bz <= -10 ? Q_POOR : (d.bz <= -5 ? Q_FAIR : Q_GOOD);
            set_row(6, "BZ", bz, has_bz ? "nT" : "", has_bz ? bz_q : Q_NONE);

            set_row(7, "AURORA", num(d.aurora), "", Q_NONE);
            set_row(8, "MUF", ok && d.muf[0] ? d.muf : "--", ok && d.muf[0] ? "MHz" : "", Q_NONE);
            set_row(9, "NOISE", ok && d.noise[0] ? d.noise : "--", "", Q_NONE);
        }

        void draw_footer(const services::PropagationTelemetry& tel, const services::BandView& v) {
            char buf[48], hhmm[8];
            const uint32_t now = (uint32_t)time(nullptr);
            if (!v.time_valid) {
                snprintf(buf, sizeof(buf), "Waiting for the clock...");
            } else if (!v.next_sun_utc) {
                snprintf(buf, sizeof(buf), "%s  no sunrise/sunset", band_light_text(v));
            } else {
                band_local_hhmm(hhmm, sizeof(hhmm), v.next_sun_utc);
                const uint32_t left = v.next_sun_utc > now ? v.next_sun_utc - now : 0;
                snprintf(buf, sizeof(buf), "%s %s %s (%lu:%02lu)", band_light_text(v),
                         v.next_rising ? "sunrise" : "sunset", hhmm, (unsigned long)(left / 3600),
                         (unsigned long)(left / 60 % 60));
            }
            lv_label_set_text(s.sun, buf);

            // Right part: at most ~14 characters between the sun text and the refresh button.
            const uint32_t unlock = PropagationManager::refresh_unlock_utc();
            const uint32_t retry = PropagationManager::next_retry_utc();
            bool warn = false;
            if (PropagationManager::is_fetching()) {
                snprintf(buf, sizeof(buf), "updating...");
            } else if (s.show_unlock && unlock) {
                band_local_hhmm(hhmm, sizeof(hhmm), unlock);
                snprintf(buf, sizeof(buf), "next %s", hhmm);
            } else if (!tel.has_data && retry) {
                band_local_hhmm(hhmm, sizeof(hhmm), retry);
                snprintf(buf, sizeof(buf), "retry %s", hhmm);
                warn = true;
            } else if (tel.has_data) {
                char age[16];
                const uint32_t age_s = now > tel.fetched_utc ? now - tel.fetched_utc : 0;
                band_age_text(age, sizeof(age), age_s);
                snprintf(buf, sizeof(buf), "%s %s", tel.own_source ? "own src" : "hamqsl", age);
                warn = age_s > 6 * 3600;
            } else {
                snprintf(buf, sizeof(buf), "waiting...");
            }
            lv_label_set_text(s.age, buf);
            lv_obj_set_style_text_color(s.age, theme_color(warn ? COLOR_BAND_FAIR : COLOR_TEXT_MUTED), 0);
            lv_obj_align(s.age, LV_ALIGN_TOP_RIGHT, -(SCREEN_W - REFRESH_X + 4), FOOTER_Y + 4);
            lv_obj_set_style_opa(s.refresh_btn, unlock ? LV_OPA_40 : LV_OPA_COVER, 0);
        }

        void refresh(bool force) {
            const auto& tel = PropagationManager::get_telemetry();
            const auto& v = PropagationManager::get_view();
            if (!force && v.version == s.drawn_version) return;
            s.drawn_version = v.version;

            draw_solar(tel, v.storm);
            const int now = band_now_column(v);
            lv_obj_set_hidden(s.now_bar, now < 0);
            if (now >= 0) lv_obj_set_x(s.now_bar, now == 0 ? COL_DAY_X : COL_NIGHT_X);
            for (int i = 0; i < bands::BAND_COUNT; i++) {
                band_badge_set(s.day[i], v.band_day[i], now == 1);
                band_badge_set(s.night[i], v.band_night[i], now == 0);
            }
            draw_footer(tel, v);
        }
    }  // namespace

    void draw_band_cond_page(lv_obj_t* parent) {
        s = Screen{};
        s.root = lv_obj_create(parent);
        lv_obj_set_size(s.root, SCREEN_W, CONTENT_H);
        lv_obj_set_style_bg_color(s.root, theme_color(COLOR_BG_APP), 0);
        lv_obj_set_style_border_width(s.root, 0, 0);
        lv_obj_set_style_radius(s.root, 0, 0);
        lv_obj_set_style_pad_all(s.root, 0, 0);
        lv_obj_set_scrollable(s.root, false);

        // Left: solar values.
        s.heading = text(s.root, COLOR_TEXT_MUTED, 8, 6);
        for (int i = 0; i < SOLAR_ROWS; i++) {
            const int y = 24 + i * 16;
            s.name[i] = text(s.root, COLOR_TEXT_MUTED, 8, y);
            s.value[i] = text(s.root, COLOR_TEXT_MAIN, 50, y);
            lv_obj_set_width(s.value[i], 36);
            lv_obj_set_style_text_align(s.value[i], LV_TEXT_ALIGN_RIGHT, 0);
            s.desc[i] = text(s.root, COLOR_TEXT_MAIN, 90, y);
        }
        lv_obj_t* divider = lv_obj_create(s.root);
        lv_obj_set_size(divider, 1, FOOTER_Y - 8);
        lv_obj_set_pos(divider, PANE_X - 4, 4);
        lv_obj_set_style_bg_color(divider, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(divider, 0, 0);

        // Right: every band, groups separated by lines.
        lv_label_set_text(text(s.root, COLOR_TEXT_MUTED, PANE_X + 4, 6), "BAND");
        lv_label_set_text(text(s.root, COLOR_TEXT_MUTED, COL_DAY_X + 8, 6), "DAY");
        lv_label_set_text(text(s.root, COLOR_TEXT_MUTED, COL_NIGHT_X + 2, 6), "NIGHT");
        s.now_bar = lv_obj_create(s.root);
        lv_obj_set_size(s.now_bar, COL_W, 2);
        lv_obj_set_pos(s.now_bar, COL_DAY_X, 19);
        lv_obj_set_style_bg_color(s.now_bar, theme_color(COLOR_ACCENT_PRIMARY), 0);
        lv_obj_set_style_border_width(s.now_bar, 0, 0);
        lv_obj_set_style_radius(s.now_bar, 0, 0);
        int y = 24;
        for (int i = 0; i < bands::BAND_COUNT; i++) {
            if (i > 0 && bands::BANDS[i].group != bands::BANDS[i - 1].group) {
                hline(s.root, PANE_X, y + 1, SCREEN_W - PANE_X - 8);
                y += 4;
            }
            lv_obj_t* name = text(s.root, COLOR_TEXT_MAIN, PANE_X + 4, y);
            lv_label_set_text(name, bands::BANDS[i].name);
            s.day[i] = band_badge_create(s.root, COL_DAY_X, y, COL_W, ROW_H - 2);
            s.night[i] = band_badge_create(s.root, COL_NIGHT_X, y, COL_W, ROW_H - 2);
            y += ROW_H;
        }

        // Bottom: light and next sun event, data age, refresh.
        hline(s.root, 0, FOOTER_Y, SCREEN_W);
        s.sun = text(s.root, COLOR_TEXT_MAIN, 8, FOOTER_Y + 4);
        s.age = text(s.root, COLOR_TEXT_MUTED, 0, FOOTER_Y + 4);
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
                s.show_unlock = !PropagationManager::refresh_now();
                refresh(true);
            },
            LV_EVENT_CLICKED, nullptr);

        refresh(true);
        s.timer = lv_timer_create([](lv_timer_t*) { refresh(PropagationManager::is_fetching()); }, 2000, nullptr);
        lv_obj_add_event_cb(
            s.root,
            [](lv_event_t*) {
                if (s.timer) lv_timer_delete(s.timer);
                s = Screen{};
            },
            LV_EVENT_DELETE, nullptr);
    }
}  // namespace ui
