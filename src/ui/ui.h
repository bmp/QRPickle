#pragma once

namespace ui {
    enum LocalPage {
        PAGE_DASHBOARD,
        PAGE_WEATHER,
        PAGE_NETWORK,
        PAGE_SETTINGS
    };

    void ui_init();
    void ui_navigate_local(LocalPage page);
    void display_update();

}  // namespace ui
