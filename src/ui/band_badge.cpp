#include "band_badge.h"
#include "fonts.h"
#include "theme.h"
#include "../config/config.h"
#include <cstdio>
#include <time.h>

namespace ui {

    using services::bands::Rating;

    BandBadge band_badge_create(lv_obj_t* parent, int x, int y, int w, int h) {
        BandBadge b;
        b.box = lv_obj_create(parent);
        lv_obj_set_size(b.box, w, h);
        lv_obj_set_pos(b.box, x, y);
        lv_obj_set_style_radius(b.box, 3, 0);
        lv_obj_set_style_border_width(b.box, 0, 0);
        lv_obj_set_style_pad_all(b.box, 0, 0);
        lv_obj_set_scrollable(b.box, false);
        lv_obj_set_clickable(b.box, false);
        lv_obj_set_event_bubble(b.box, true);

        b.label = lv_label_create(b.box);
        lv_obj_set_style_text_font(b.label, &font_jetbrains_10, 0);
        lv_obj_center(b.label);
        return b;
    }

    // The theme's text-on-accent colour, or its main text colour when that reads better on this
    // badge (E-Ink Light: white text on the light-grey "poor" badge was unreadable).
    static lv_color_t readable_on(lv_color_t bg) {
        const lv_color_t a = theme_color(COLOR_TEXT_ON_ACCENT), m = theme_color(COLOR_TEXT_MAIN);
        const int lb = lv_color_luminance(bg);
        const int da = lv_color_luminance(a) - lb, dm = lv_color_luminance(m) - lb;
        return (da < 0 ? -da : da) >= (dm < 0 ? -dm : dm) ? a : m;
    }

    void band_badge_set(const BandBadge& b, Rating r, bool dimmed) {
        ThemeToken bg = COLOR_BG_BUTTON, fg = COLOR_TEXT_MUTED;
        const char* text = "--";
        switch (r) {
            case Rating::GOOD:
                bg = COLOR_BAND_GOOD;
                fg = COLOR_TEXT_ON_ACCENT;
                text = "G";
                break;
            case Rating::FAIR:
                bg = COLOR_BAND_FAIR;
                fg = COLOR_TEXT_ON_ACCENT;
                text = "F";
                break;
            case Rating::POOR:
                bg = COLOR_BAND_POOR;
                fg = COLOR_TEXT_ON_ACCENT;
                text = "P";
                break;
            case Rating::ES_POSSIBLE:
                bg = COLOR_ACCENT_SECONDARY;
                fg = COLOR_TEXT_ON_ACCENT;
                text = "Es?";
                break;
            case Rating::CLOSED:  text = "-"; break;  // like "unknown", but a dash
            case Rating::UNKNOWN: break;
        }
        lv_obj_set_style_bg_color(b.box, theme_color(bg), 0);
        lv_obj_set_style_text_color(b.label, fg == COLOR_TEXT_ON_ACCENT ? readable_on(theme_color(bg)) : theme_color(fg), 0);
        lv_label_set_text(b.label, text);
        lv_obj_center(b.label);
        // 60 %: still readable in the dark single-colour themes (Field Red, Terminal Green).
        lv_obj_set_style_opa(b.box, dimmed ? LV_OPA_60 : LV_OPA_COVER, 0);
    }

    int band_now_column(const services::BandView& v) {
        if (!v.time_valid) return -1;
        switch (v.light) {
            case services::sun::Light::DAY:   return 0;
            case services::sun::Light::NIGHT: return 1;
            default:                          return -1;
        }
    }

    const char* band_light_text(const services::BandView& v, bool short_form) {
        if (!v.time_valid) return "--";
        switch (v.light) {
            case services::sun::Light::DAY:   return "DAY";
            case services::sun::Light::NIGHT: return "NIGHT";
            default:                          return short_form ? "GREY" : "GREYLINE";
        }
    }

    void band_age_text(char* out, size_t len, uint32_t age_s) {
        if (age_s < 120) snprintf(out, len, "just now");
        else if (age_s < 2 * 3600) snprintf(out, len, "%lum ago", (unsigned long)(age_s / 60));
        else snprintf(out, len, "%luh ago", (unsigned long)(age_s / 3600));
    }

    void band_local_hhmm(char* out, size_t len, uint32_t utc) {
        time_t local = (time_t)utc + (time_t)config::get().tz_offset_hh * 1800;
        struct tm tm_l;
        gmtime_r(&local, &tm_l);
        snprintf(out, len, "%02d:%02d", tm_l.tm_hour, tm_l.tm_min);
    }

}  // namespace ui
