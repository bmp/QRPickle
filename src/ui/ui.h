#pragma once

namespace ui {
    enum LocalPage {
        PAGE_DASHBOARD,
        PAGE_WEATHER,
        PAGE_NETWORK,
        PAGE_SETTINGS,
        PAGE_SPOTS,
        PAGE_XOTA,
        PAGE_APRS,
        PAGE_APRS_RADAR,
        PAGE_APRS_MSG,
        PAGE_BAND_COND,
        PAGE_HAMALERT,
        PAGE_CLOUD_OTA,
        PAGE_SATELLITES  // appended: the screenshot tool and /api/debug/screen use these numbers
    };

    void ui_init();
    void ui_navigate_local(LocalPage page);
    // After a theme change: recolours the shell built at boot (screen background, status bar, home
    // button, menu); without it they keep the boot theme until a restart. Pages are redrawn on open.
    void ui_refresh_theme();
    LocalPage ui_active_page();
    void display_update();

}  // namespace ui
