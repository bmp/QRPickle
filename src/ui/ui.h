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
        PAGE_APRS_MSG
    };

    void ui_init();
    void ui_navigate_local(LocalPage page);
    void display_update();

}  // namespace ui
