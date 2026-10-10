#pragma once
#include <lvgl.h>
#include <stddef.h>
#include <stdint.h>
#include "../services/prop_manager.h"

// Shared pieces of the band tile (widgets/widget_band) and the band-conditions screen
// (screens/band_cond): rating badges, the day/night column highlight and status texts.
namespace ui {

    struct BandBadge {
        lv_obj_t* box;
        lv_obj_t* label;
    };

    // A rounded badge with a centred label (font_jetbrains_10), placed at x/y in parent.
    BandBadge band_badge_create(lv_obj_t* parent, int x, int y, int w, int h);
    // G/F/P in the theme's band colours, "Es?" for sporadic-E, "-" closed, "--" unknown. A dimmed
    // badge belongs to the column that isn't "now" (day vs night).
    void band_badge_set(const BandBadge& b, services::bands::Rating r, bool dimmed);

    // Which rating column is "now": 0 = day, 1 = night, -1 = both (greyline or clock not set).
    int band_now_column(const services::BandView& v);

    // "DAY", "NIGHT", "GREYLINE" ("GREY" when short; "--" without a clock).
    const char* band_light_text(const services::BandView& v, bool short_form = false);
    // "just now", "25m ago", "3h ago"; age in seconds.
    void band_age_text(char* out, size_t len, uint32_t age_s);
    // Local "HH:MM" (config time zone) of a UTC time.
    void band_local_hhmm(char* out, size_t len, uint32_t utc);

}  // namespace ui
