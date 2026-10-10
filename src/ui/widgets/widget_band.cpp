#include "widget_band.h"
#include "../band_badge.h"
#include "../fonts.h"
#include "../theme.h"
#include "../ui.h"
#include "../../config/config.h"
#include "../../services/prop_manager.h"
#include <cstdio>
#include <time.h>

namespace ui {

    using services::PropagationManager;
    namespace bands = services::bands;

    // Dashboard band tile (docs/UI_GUIDE.md): solar summary, the band groups chosen in the web
    // console (config::band_groups) rated for day and night, and the data age. Tapping it opens the
    // band-conditions screen.
    namespace {
        constexpr int W = 154, H = 212;
        constexpr int COL_DAY_X = 74, COL_NIGHT_X = 114, COL_W = 30;

        struct Tile {
            lv_timer_t* timer;
            lv_obj_t* solar;
            lv_obj_t* light;
            lv_obj_t* hdr_day;
            lv_obj_t* hdr_night;
            lv_obj_t* now_bar;
            lv_obj_t* footer;
            int rows;
            uint8_t group[bands::GROUP_COUNT];
            BandBadge day[bands::GROUP_COUNT];
            BandBadge night[bands::GROUP_COUNT];
            uint32_t drawn_version;
        };
        Tile t;

        lv_obj_t* text(lv_obj_t* parent, const lv_font_t* font, ThemeToken color, int x, int y) {
            lv_obj_t* l = lv_label_create(parent);
            lv_obj_set_style_text_font(l, font, 0);
            lv_obj_set_style_text_color(l, theme_color(color), 0);
            lv_obj_set_pos(l, x, y);
            lv_label_set_text(l, "");
            return l;
        }

        void refresh() {
            const auto& tel = PropagationManager::get_telemetry();
            const auto& v = PropagationManager::get_view();
            if (v.version == t.drawn_version) return;
            t.drawn_version = v.version;

            char buf[40];
            const auto& d = tel.solar;
            if (tel.has_data) snprintf(buf, sizeof(buf), "SFI %d K %d A %d", d.sfi, d.k_index, d.a_index);
            else snprintf(buf, sizeof(buf), "SFI -- K -- A --");
            lv_label_set_text(t.solar, buf);
            lv_obj_set_style_text_color(t.solar, theme_color(v.storm ? COLOR_BAND_POOR : COLOR_TEXT_MAIN), 0);
            lv_label_set_text(t.light, band_light_text(v, true));
            lv_obj_align(t.light, LV_ALIGN_TOP_RIGHT, -10, 8);

            // The "now" column: underlined header, full-colour badges; the other column is dimmed.
            const int now = band_now_column(v);
            lv_obj_set_hidden(t.now_bar, now < 0);
            if (now >= 0) lv_obj_set_x(t.now_bar, now == 0 ? COL_DAY_X : COL_NIGHT_X);
            for (int i = 0; i < t.rows; i++) {
                band_badge_set(t.day[i], v.group_day[t.group[i]], now == 1);
                band_badge_set(t.night[i], v.group_night[t.group[i]], now == 0);
            }

            const time_t clock = time(nullptr);
            const uint32_t age = tel.has_data && (uint32_t)clock > tel.fetched_utc ? (uint32_t)clock - tel.fetched_utc : 0;
            if (tel.has_data) {
                char age_buf[16];
                band_age_text(age_buf, sizeof(age_buf), age);
                snprintf(buf, sizeof(buf), "%s %s", tel.own_source ? "own source" : "hamqsl.com", age_buf);
            } else if (const uint32_t retry = PropagationManager::next_retry_utc()) {
                char hhmm[8];
                band_local_hhmm(hhmm, sizeof(hhmm), retry);
                snprintf(buf, sizeof(buf), "Failed, retry %s", hhmm);
            } else {
                snprintf(buf, sizeof(buf), "Waiting for data...");
            }
            lv_label_set_text(t.footer, buf);
            const bool warn = age > 6 * 3600 || (!tel.has_data && tel.failed_utc);
            lv_obj_set_style_text_color(t.footer, theme_color(warn ? COLOR_BAND_FAIR : COLOR_TEXT_MUTED), 0);
        }
    }  // namespace

    lv_obj_t* widget_band_create(lv_obj_t* parent, WidgetSize size) {
        lv_obj_t* widget = lv_obj_create(parent);
        lv_obj_set_scrollable(widget, false);
        lv_obj_set_style_bg_color(widget, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_color(widget, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(widget, 1, 0);
        lv_obj_set_style_radius(widget, 6, 0);
        lv_obj_set_style_pad_all(widget, 0, 0);
        if (size != WIDGET_SIZE_HALF_VERT) return widget;  // the only size the dashboard uses

        lv_obj_set_size(widget, W, H);
        lv_obj_set_clickable(widget, true);
        lv_obj_add_event_cb(widget, [](lv_event_t*) { ui_navigate_local(PAGE_BAND_COND); }, LV_EVENT_CLICKED, nullptr);

        t = Tile{};
        t.solar = text(widget, &font_jetbrains_10, COLOR_TEXT_MAIN, 10, 8);
        t.light = text(widget, &font_jetbrains_10, COLOR_ACCENT_PRIMARY, 0, 8);

        lv_obj_t* hdr = text(widget, &font_jetbrains_10, COLOR_TEXT_MUTED, 10, 30);
        lv_label_set_text(hdr, "BAND");
        t.hdr_day = text(widget, &font_jetbrains_10, COLOR_TEXT_MUTED, COL_DAY_X + 6, 30);
        lv_label_set_text(t.hdr_day, "DAY");
        t.hdr_night = text(widget, &font_jetbrains_10, COLOR_TEXT_MUTED, COL_NIGHT_X, 30);
        lv_label_set_text(t.hdr_night, "NIGHT");
        t.now_bar = lv_obj_create(widget);
        lv_obj_set_size(t.now_bar, COL_W, 2);
        lv_obj_set_pos(t.now_bar, COL_DAY_X, 44);
        lv_obj_set_style_bg_color(t.now_bar, theme_color(COLOR_ACCENT_PRIMARY), 0);
        lv_obj_set_style_border_width(t.now_bar, 0, 0);
        lv_obj_set_style_radius(t.now_bar, 0, 0);

        // Rows for the chosen groups, spread over the space between header and footer.
        const uint8_t mask = config::get().band_groups;
        for (int g = 0; g < bands::GROUP_COUNT; g++)
            if (mask & (1u << g)) t.group[t.rows++] = (uint8_t)g;
        if (t.rows == 0) t.group[t.rows++] = 0;  // sanitize() prevents this; stay safe
        constexpr int TOP = 52, BOTTOM = 188;
        int pitch = (BOTTOM - TOP) / t.rows;
        if (pitch > 40) pitch = 40;
        const int badge_h = pitch - 8 > 22 ? 22 : pitch - 8;
        const lv_font_t* font = t.rows <= 3 ? &font_jetbrains_14 : &font_jetbrains_10;
        for (int i = 0; i < t.rows; i++) {
            const int y = TOP + i * pitch;
            lv_obj_t* name = text(widget, font, COLOR_TEXT_MAIN, 10, y + (badge_h - lv_font_get_line_height(font)) / 2);
            lv_label_set_text(name, bands::GROUP_NAMES[t.group[i]]);
            t.day[i] = band_badge_create(widget, COL_DAY_X, y, COL_W, badge_h);
            t.night[i] = band_badge_create(widget, COL_NIGHT_X, y, COL_W, badge_h);
            lv_obj_set_style_text_font(t.day[i].label, font, 0);
            lv_obj_set_style_text_font(t.night[i].label, font, 0);
        }

        t.footer = text(widget, &font_jetbrains_10, COLOR_TEXT_MUTED, 10, 194);

        t.drawn_version = PropagationManager::get_view().version - 1;
        refresh();
        t.timer = lv_timer_create([](lv_timer_t*) { refresh(); }, 2000, nullptr);
        lv_obj_add_event_cb(
            widget,
            [](lv_event_t*) {
                if (t.timer) lv_timer_delete(t.timer);
                t = Tile{};
            },
            LV_EVENT_DELETE, nullptr);
        return widget;
    }

}  // namespace ui
