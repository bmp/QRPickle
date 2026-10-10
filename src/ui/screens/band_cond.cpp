#include "band_cond.h"
#include "../band_badge.h"
#include "../layout.h"
#include "../theme.h"
#include "../fonts.h"
#include "../../services/prop_manager.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <time.h>

namespace ui {

    using services::PropagationManager;
    namespace bands = services::bands;
    namespace solar = services::solar;

    // Band-conditions screen (docs/UI_GUIDE.md): three tabs, BANDS (every band rated for day and
    // night, the current column underlined), SOLAR (the indicators that matter for HF, each with a
    // scale and its meaning) and GUIDE (how to read them, with notes for portable operation). The
    // footer (light and next sunrise/sunset, data age, refresh) is shared by all tabs.
    namespace {
        enum Tab { TAB_BANDS,
                   TAB_SOLAR,
                   TAB_GUIDE,
                   TAB_COUNT };
        Tab active_tab = TAB_BANDS;  // kept between visits, like xOTA

        constexpr int TAB_H = 24, FOOTER_H = 20;
        constexpr int BODY_Y = TAB_H, BODY_H = CONTENT_H - TAB_H - FOOTER_H;  // 172
        constexpr int FOOTER_Y = CONTENT_H - FOOTER_H;
        // Refresh button left of the global home button (bottom-right corner, home_button.cpp).
        constexpr int REFRESH_X = SCREEN_W - 72, REFRESH_W = 28;
        constexpr int AGE_W = 90;  // "hamqsl 59m ago" fits; the sun text gets the rest
        constexpr int SUN_W = REFRESH_X - AGE_W - 14;

        // BANDS tab columns.
        constexpr int B_NAME_X = 16, B_DAY_X = 86, B_NIGHT_X = 146, B_COL_W = 44, B_GROUP_X = 206;
        constexpr int B_ROW_H = 13;
        const char* const GROUP_LABEL[bands::GROUP_COUNT] = {"LOW BANDS", "ALL-ROUND", "MAIN DX", "HIGH BANDS",
                                                             "VHF / Es"};

        // SOLAR tab rows.
        constexpr int S_ROWS = 6, S_ROW_H = 24;
        constexpr int S_NAME_X = 6, S_VALUE_X = 48, S_VALUE_W = 44, S_BAR_X = 98, S_BAR_W = 96, S_TEXT_X = 202;

        struct Scale {
            lv_obj_t* marker;
        };

        struct Screen {
            lv_obj_t* root;
            lv_obj_t* body;
            lv_timer_t* timer;
            lv_obj_t* tab_btn[TAB_COUNT];
            lv_obj_t* tab_lbl[TAB_COUNT];
            // BANDS
            lv_obj_t* bar[2];
            BandBadge day[bands::BAND_COUNT];
            BandBadge night[bands::BAND_COUNT];
            // SOLAR
            lv_obj_t* s_value[S_ROWS];
            lv_obj_t* s_line1[S_ROWS];
            lv_obj_t* s_line2[S_ROWS];
            Scale s_scale[S_ROWS];
            lv_obj_t* s_extra;
            // footer
            lv_obj_t* sun;
            lv_obj_t* age;
            lv_obj_t* refresh_btn;
            uint32_t drawn_version;
            bool show_unlock;  // after a tap while rate-limited
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

        // ---- Value quality (green/amber/red), shared by the scale and the value colour ----------

        enum Q { Q_NONE,
                 Q_GOOD,
                 Q_FAIR,
                 Q_POOR };
        lv_color_t q_color(Q q) {
            switch (q) {
                case Q_GOOD: return theme_color(COLOR_BAND_GOOD);
                case Q_FAIR: return theme_color(COLOR_BAND_FAIR);
                case Q_POOR: return theme_color(COLOR_BAND_POOR);
                default:     return theme_color(COLOR_TEXT_MUTED);
            }
        }

        // A scale: three coloured zones (in the order given) and a marker. The zone boundaries
        // b1/b2 are fractions of the width.
        Scale scale_create(lv_obj_t* parent, int y, float b1, float b2, Q first, Q mid, Q last) {
            const int w1 = (int)(S_BAR_W * b1), w2 = (int)(S_BAR_W * b2) - w1, w3 = S_BAR_W - w1 - w2;
            const Q q[3] = {first, mid, last};
            const int w[3] = {w1, w2, w3};
            int x = S_BAR_X;
            for (int i = 0; i < 3; i++) {
                lv_obj_t* z = rect(parent, x, y + 8, w[i], 5, COLOR_BORDER);
                lv_obj_set_style_bg_color(z, q_color(q[i]), 0);
                lv_obj_set_style_bg_opa(z, LV_OPA_70, 0);
                x += w[i];
            }
            Scale sc;
            sc.marker = rect(parent, S_BAR_X, y + 4, 3, 13, COLOR_TEXT_MAIN);
            return sc;
        }

        // Marker at pos (0..1 along the scale); hidden while the value is unknown.
        void scale_set(const Scale& sc, float pos, bool known) {
            lv_obj_set_hidden(sc.marker, !known);
            if (pos < 0) pos = 0;
            if (pos > 1) pos = 1;
            lv_obj_set_x(sc.marker, S_BAR_X + (int)(pos * (S_BAR_W - 3)));
        }

        // ---- BANDS tab -------------------------------------------------------------------------

        void draw_bands_tab(lv_obj_t* body) {
            // 11 rows + 4 separators + header must fit the 172 px body (the 6 m row touched the footer).
            lv_label_set_text(text(body, COLOR_TEXT_MUTED, B_NAME_X, 1), "BAND");
            lv_label_set_text(text(body, COLOR_TEXT_MUTED, B_DAY_X + 13, 1), "DAY");
            lv_label_set_text(text(body, COLOR_TEXT_MUTED, B_NIGHT_X + 7, 1), "NIGHT");
            for (int c = 0; c < 2; c++)
                s.bar[c] = rect(body, c == 0 ? B_DAY_X : B_NIGHT_X, 13, B_COL_W, 2, COLOR_ACCENT_PRIMARY);
            int y = 17;
            for (int i = 0; i < bands::BAND_COUNT; i++) {
                const int g = bands::BANDS[i].group;
                const bool new_group = i == 0 || g != bands::BANDS[i - 1].group;
                if (i > 0 && new_group) {
                    rect(body, B_NAME_X - 6, y, SCREEN_W - 2 * (B_NAME_X - 6), 1, COLOR_BORDER);
                    y += 2;
                }
                if (new_group) lv_label_set_text(text(body, COLOR_TEXT_MUTED, B_GROUP_X, y), GROUP_LABEL[g]);
                lv_label_set_text(text(body, COLOR_TEXT_MAIN, B_NAME_X, y), bands::BANDS[i].name);
                s.day[i] = band_badge_create(body, B_DAY_X, y, B_COL_W, B_ROW_H - 2);
                s.night[i] = band_badge_create(body, B_NIGHT_X, y, B_COL_W, B_ROW_H - 2);
                y += B_ROW_H;
            }
        }

        void refresh_bands_tab(const services::BandView& v) {
            const int now = band_now_column(v);
            for (int c = 0; c < 2; c++) lv_obj_set_hidden(s.bar[c], !band_underline(v, c));
            for (int i = 0; i < bands::BAND_COUNT; i++) {
                band_badge_set(s.day[i], v.band_day[i], now == 1);
                band_badge_set(s.night[i], v.band_night[i], now == 0);
            }
        }

        // ---- SOLAR tab -------------------------------------------------------------------------

        void draw_solar_tab(lv_obj_t* body) {
            static const char* const NAMES[S_ROWS] = {"SFI", "K", "A", "X-RAY", "NOISE", "STORM"};
            for (int i = 0; i < S_ROWS; i++) {
                const int y = 2 + i * S_ROW_H;
                lv_label_set_text(text(body, COLOR_TEXT_MUTED, S_NAME_X, y + 5), NAMES[i]);
                s.s_value[i] = text(body, COLOR_TEXT_MAIN, S_VALUE_X, y + 5);
                lv_obj_set_width(s.s_value[i], S_VALUE_W);
                lv_obj_set_style_text_align(s.s_value[i], LV_TEXT_ALIGN_RIGHT, 0);
                s.s_line1[i] = text(body, COLOR_TEXT_MAIN, S_TEXT_X, y);
                s.s_line2[i] = text(body, COLOR_TEXT_MUTED, S_TEXT_X, y + 11);
            }
            // Zones as fractions of each scale (ranges in refresh_solar_tab()).
            s.s_scale[0] = scale_create(body, 2, 30.f / 190, 60.f / 190, Q_POOR, Q_FAIR, Q_GOOD);  // SFI 60..250
            s.s_scale[1] = scale_create(body, 2 + S_ROW_H, 3.f / 9, 5.f / 9, Q_GOOD, Q_FAIR, Q_POOR);  // K 0..9
            s.s_scale[2] = scale_create(body, 2 + 2 * S_ROW_H, 20.f / 60, 30.f / 60, Q_GOOD, Q_FAIR, Q_POOR);  // A 0..60
            s.s_scale[3] = scale_create(body, 2 + 3 * S_ROW_H, 2.f / 5, 3.f / 5, Q_GOOD, Q_FAIR, Q_POOR);  // AB|C|MX
            s.s_scale[4] = scale_create(body, 2 + 4 * S_ROW_H, 4.f / 9, 6.f / 9, Q_GOOD, Q_FAIR, Q_POOR);  // S0..S9
            s.s_scale[5] = scale_create(body, 2 + 5 * S_ROW_H, 1.f / 3, 2.f / 3, Q_GOOD, Q_FAIR, Q_POOR);  // risk
            s.s_extra = text(body, COLOR_TEXT_MUTED, S_NAME_X, 2 + S_ROWS * S_ROW_H + 2);
        }

        void set_solar_row(int i, const char* value, Q q, float pos, bool known, const char* l1, const char* l2) {
            lv_label_set_text(s.s_value[i], known ? value : "--");
            lv_obj_set_style_text_color(s.s_value[i], known ? q_color(q) : theme_color(COLOR_TEXT_MUTED), 0);
            scale_set(s.s_scale[i], pos, known);
            lv_label_set_text(s.s_line1[i], known ? l1 : "");
            lv_obj_set_style_text_color(s.s_line1[i], q_color(q), 0);
            lv_label_set_text(s.s_line2[i], known ? l2 : "");
        }

        // Highest HF band rated good in daytime (for "day: good to 15m"), or nullptr.
        const char* best_day_band(const services::BandView& v) {
            const char* best = nullptr;
            for (int i = 0; i < bands::BAND_COUNT - 1; i++)
                if (v.band_day[i] == bands::Rating::GOOD) best = bands::BANDS[i].name;
            return best;
        }

        void refresh_solar_tab(const services::PropagationTelemetry& tel, const services::BandView& v) {
            const auto& d = tel.solar;
            const bool ok = tel.has_data;
            char val[16], l2[24];

            // SFI on 60..250: below 90 low, 90-119 moderate, 120+ good.
            snprintf(val, sizeof(val), "%d", d.sfi);
            const Q sfi_q = d.sfi >= 120 ? Q_GOOD : (d.sfi >= 90 ? Q_FAIR : Q_POOR);
            const char* best = best_day_band(v);
            if (best) snprintf(l2, sizeof(l2), "day: good to %s", best);
            else snprintf(l2, sizeof(l2), "high bands closed");
            set_solar_row(0, val, sfi_q, (d.sfi - 60) / 190.f, ok,
                          sfi_q == Q_GOOD ? "GOOD" : (sfi_q == Q_FAIR ? "MODERATE" : "LOW"), l2);

            // K on 0..9: 0-2 quiet, 3-4 unsettled/active, 5+ storm.
            const bool k_ok = ok && d.k_index != solar::NONE;
            snprintf(val, sizeof(val), "%d", d.k_index);
            const Q k_q = d.k_index <= 2 ? Q_GOOD : (d.k_index <= 4 ? Q_FAIR : Q_POOR);
            set_solar_row(1, val, k_q, (d.k_index + 0.5f) / 9.f, k_ok,
                          k_q == Q_GOOD ? "QUIET" : (k_q == Q_FAIR ? "UNSETTLED" : "STORM"),
                          k_q == Q_GOOD ? "low noise" : (k_q == Q_FAIR ? "noisier low bands" : "HF degraded"));

            // A on 0..60: below 10 quiet, below 20 unsettled, below 30 active, 30+ storm.
            const bool a_ok = ok && d.a_index != solar::NONE;
            snprintf(val, sizeof(val), "%d", d.a_index);
            const Q a_q = d.a_index < 20 ? Q_GOOD : (d.a_index < 30 ? Q_FAIR : Q_POOR);
            const char* a_word =
                d.a_index < 10 ? "QUIET" : (d.a_index < 20 ? "UNSETTLED" : (d.a_index < 30 ? "ACTIVE" : "STORM"));
            set_solar_row(2, val, a_q, d.a_index / 60.f, a_ok, a_word, "24 h average");

            // X-ray class: A, B quiet | C small | M, X blackout risk on the sunlit side.
            const char c = d.xray[0];
            static const char CLASSES[] = "ABCMX";
            const char* at = ok && c ? strchr(CLASSES, c) : nullptr;
            const float mag = at ? strtof(d.xray + 1, nullptr) : 0;
            const float x_pos = at ? ((at - CLASSES) + (mag >= 9 ? 0.9f : mag / 10.f)) / 5.f : 0;
            const Q x_q = (c == 'M' || c == 'X') ? Q_POOR : (c == 'C' ? Q_FAIR : Q_GOOD);
            set_solar_row(3, d.xray, x_q, x_pos, at != nullptr,
                          x_q == Q_POOR ? "FLARE" : (x_q == Q_FAIR ? "SMALL FLARE" : "NO FLARE"),
                          x_q == Q_POOR ? "sunlit HF may fade" : "no blackout");

            // Noise "S2-S3": the first S digit, 0..9.
            const char* sp = strchr(d.noise, 'S');
            const int sn = sp && sp[1] >= '0' && sp[1] <= '9' ? sp[1] - '0' : -1;
            const Q n_q = sn <= 3 ? Q_GOOD : (sn <= 5 ? Q_FAIR : Q_POOR);
            set_solar_row(4, d.noise, n_q, (sn + 0.5f) / 9.f, ok && sn >= 0,
                          n_q == Q_GOOD ? "LOW" : (n_q == Q_FAIR ? "MODERATE" : "HIGH"),
                          n_q == Q_GOOD ? "weak signals OK" : (n_q == Q_FAIR ? "hard for QRP" : "weak sigs lost"));

            // Storm risk in the next hours from the solar wind and Bz (southward = negative).
            const bool w_ok = ok && d.solar_wind >= 0, b_ok = ok && !std::isnan(d.bz);
            int risk = 0;
            if ((w_ok && d.solar_wind > 600) || (b_ok && d.bz <= -10)) risk = 2;
            else if ((w_ok && d.solar_wind > 500) || (b_ok && d.bz <= -5)) risk = 1;
            char wb[24];
            snprintf(wb, sizeof(wb), "wind %.0f Bz %.1f", w_ok ? d.solar_wind : 0.f, b_ok ? d.bz : 0.f);
            const Q r_q = risk == 0 ? Q_GOOD : (risk == 1 ? Q_FAIR : Q_POOR);
            set_solar_row(5, risk == 0 ? "LOW" : (risk == 1 ? "MOD" : "HIGH"), r_q, (risk + 0.5f) / 3.f, w_ok && b_ok,
                          risk == 0 ? "NEXT HOURS OK" : (risk == 1 ? "WATCH" : "STORM LIKELY"), wb);

            char extra[64], ssn[8] = "--", aur[8] = "--";
            if (ok && d.sunspots != solar::NONE) snprintf(ssn, sizeof(ssn), "%d", d.sunspots);
            if (ok && d.aurora != solar::NONE) snprintf(aur, sizeof(aur), "%d", d.aurora);
            snprintf(extra, sizeof(extra), "SSN %s   AURORA %s   MUF %s", ssn, aur, ok && d.muf[0] ? d.muf : "--");
            lv_label_set_text(s.s_extra, extra);
        }

        // ---- GUIDE tab -------------------------------------------------------------------------

        // Plain ASCII: the fonts have no other glyphs (docs/UI_GUIDE.md).
        const char* const GUIDE_TEXT =
            "BANDS: each band rated for day and night (G good, F fair, P poor). The current column is "
            "underlined; both during greyline. On 6 m, Es? means sporadic-E season (daytime).\n\n"
            "SFI - solar flux. How high the bands go: below 90 stay on 20 m and lower, 90-120 opens "
            "15 m, above 120 opens 10 m in daytime.\n"
            "Portable: pick the highest band rated G for the time of day; it needs the least antenna.\n\n"
            "K - geomagnetic activity over the last 3 hours (0-9). 0-2 quiet, 3-4 noisier, 5 and up "
            "a storm: HF fades, polar paths close, the low bands suffer most.\n"
            "Portable: in a storm favour 40/30 m in daytime and expect shorter paths.\n\n"
            "A - the day's geomagnetic average. Confirms the trend of K: under 20 is fine.\n\n"
            "X-RAY - solar flares. A, B: quiet. C: small. M or X: the sunlit side of HF can black out "
            "for minutes to hours. The band fades; your radio is fine.\n"
            "Portable: a sudden silence at midday during an M/X flare is not your setup.\n\n"
            "NOISE - the expected noise floor from geomagnetic activity. S2-S3 is low; from S5 weak "
            "QRP signals are lost.\n\n"
            "STORM - the risk of a storm in the next hours, from the solar wind (above 500 km/s is "
            "elevated) and Bz (negative = southward; below -10 nT a storm is likely).\n"
            "Portable: check it before heading out for an activation.\n\n"
            "SSN (sunspots) tells much the same as SFI. AURORA matters for VHF near the poles. MUF is "
            "the measured highest usable frequency, when the source reports it.\n\n"
            "Data: hamqsl.com (N0NBH), updated every 3 hours, hourly in storms. The ratings are "
            "estimates for a typical 3000 km path; the manual explains the model.";

        void draw_guide_tab(lv_obj_t* body) {
            lv_obj_set_scrollable(body, true);
            lv_obj_set_scroll_dir(body, LV_DIR_VER);
            lv_obj_t* l = text(body, COLOR_TEXT_MAIN, 8, 4, &font_atkinson_14);
            lv_obj_set_width(l, SCREEN_W - 24);
            lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
            lv_label_set_text_static(l, GUIDE_TEXT);
        }

        // ---- Footer ----------------------------------------------------------------------------

        void draw_footer(const services::PropagationTelemetry& tel, const services::BandView& v) {
            char buf[48], hhmm[8];
            const uint32_t now = (uint32_t)time(nullptr);
            if (!v.time_valid) {
                snprintf(buf, sizeof(buf), "waiting for the clock");
            } else if (!v.next_sun_utc) {
                snprintf(buf, sizeof(buf), "%s, no sunrise/sunset", band_light_text(v, true));
            } else {
                band_local_hhmm(hhmm, sizeof(hhmm), v.next_sun_utc);
                snprintf(buf, sizeof(buf), "%s, %s at %s", band_light_text(v, true),
                         v.next_rising ? "sunrise" : "sunset", hhmm);
            }
            lv_label_set_text(s.sun, buf);

            const uint32_t unlock = PropagationManager::refresh_unlock_utc();
            const uint32_t retry = PropagationManager::next_retry_utc();
            bool warn = false;
            if (PropagationManager::is_fetching()) {
                snprintf(buf, sizeof(buf), "updating...");
            } else if (s.show_unlock && unlock) {
                band_local_hhmm(hhmm, sizeof(hhmm), unlock);
                snprintf(buf, sizeof(buf), "next at %s", hhmm);
            } else if (!tel.has_data && retry) {
                band_local_hhmm(hhmm, sizeof(hhmm), retry);
                snprintf(buf, sizeof(buf), "retry at %s", hhmm);
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
            // Background and icon opacity, not object opacity (that renders through a layer).
            const lv_opa_t opa = unlock ? LV_OPA_40 : LV_OPA_COVER;
            lv_obj_set_style_bg_opa(s.refresh_btn, opa, 0);
            lv_obj_set_style_text_opa(lv_obj_get_child(s.refresh_btn, 0), opa, 0);
        }

        void refresh(bool force) {
            const auto& tel = PropagationManager::get_telemetry();
            const auto& v = PropagationManager::get_view();
            if (!force && v.version == s.drawn_version) return;
            s.drawn_version = v.version;

            // The SOLAR tab label turns red in a storm, whichever tab is open.
            const ThemeToken solar_tab =
                v.storm ? COLOR_BAND_POOR : (active_tab == TAB_SOLAR ? COLOR_ACCENT_PRIMARY : COLOR_TEXT_MUTED);
            lv_obj_set_style_text_color(s.tab_lbl[TAB_SOLAR], theme_color(solar_tab), 0);
            if (active_tab == TAB_BANDS) refresh_bands_tab(v);
            if (active_tab == TAB_SOLAR) refresh_solar_tab(tel, v);
            draw_footer(tel, v);
        }

        void show_tab(Tab t) {
            active_tab = t;
            for (int i = 0; i < TAB_COUNT; i++) {
                const bool on = i == t;
                lv_obj_set_style_text_color(s.tab_lbl[i], theme_color(on ? COLOR_ACCENT_PRIMARY : COLOR_TEXT_MUTED), 0);
                lv_obj_set_style_border_width(s.tab_btn[i], on ? 2 : 0, 0);
            }
            lv_obj_clean(s.body);  // only the open tab's objects exist (less LVGL memory)
            lv_obj_scroll_to_y(s.body, 0, LV_ANIM_OFF);
            lv_obj_set_scrollable(s.body, false);
            if (t == TAB_BANDS) draw_bands_tab(s.body);
            if (t == TAB_SOLAR) draw_solar_tab(s.body);
            if (t == TAB_GUIDE) draw_guide_tab(s.body);
            refresh(true);
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

        // Tabs, styled like xOTA's POTA/SOTA tabs.
        static const char* const TAB_NAMES[TAB_COUNT] = {"BANDS", "SOLAR", "GUIDE"};
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

        // Footer: light and next sunrise/sunset, data age, refresh. Fixed widths, clipped: the two
        // texts can never overlap.
        rect(s.root, 0, FOOTER_Y, SCREEN_W, 1, COLOR_BORDER);
        s.sun = text(s.root, COLOR_TEXT_MAIN, 6, FOOTER_Y + 5);
        lv_obj_set_width(s.sun, SUN_W);
        lv_label_set_long_mode(s.sun, LV_LABEL_LONG_CLIP);
        s.age = text(s.root, COLOR_TEXT_MUTED, REFRESH_X - 6 - AGE_W, FOOTER_Y + 5);
        lv_obj_set_width(s.age, AGE_W);
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
                s.show_unlock = !PropagationManager::refresh_now();
                refresh(true);
            },
            LV_EVENT_CLICKED, nullptr);

        show_tab(active_tab);
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
