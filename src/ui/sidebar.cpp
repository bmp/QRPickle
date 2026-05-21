#include "sidebar.h"
#include "theme.h"
#include <lvgl.h>

namespace ui {

    static lv_obj_t* parent_obj = nullptr;
    static lv_obj_t* panel = nullptr;
    static bool visible = false;
    static SidebarCallbacks callbacks{};

    struct Item { const char* label; DestScreen dest; };
    static const Item ITEMS[] = {
        {"Dashboard",   DEST_DASHBOARD},
        {"Settings",    DEST_SETTINGS},
        {"Network",     DEST_NETWORK},
        {"Spots (DX)",  DEST_SPOTS},
        {"POTA",        DEST_POTA},
        {"Propagation", DEST_PROP},
        {"Weather",     DEST_WEATHER},
    };
    static constexpr int ITEM_COUNT = sizeof(ITEMS) / sizeof(ITEMS[0]);

    static void item_clicked(lv_event_t* e) {
        DestScreen dest = (DestScreen)(intptr_t)lv_event_get_user_data(e);
        sidebar_hide();
        if (callbacks.on_select) callbacks.on_select(dest);
    }

    static void build_panel() {
        panel = lv_obj_create(parent_obj);
        lv_obj_set_size(panel, 140, 240);
        lv_obj_set_pos(panel, -140, 0);
        lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_border_width(panel, 1, 0);
        lv_obj_set_style_radius(panel, 0, 0);
        lv_obj_set_style_pad_all(panel, 4, 0);
        lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);

        // Map Dynamic Theme Tokens directly to container scaffolding
        lv_obj_set_style_bg_color(panel, theme_color(COLOR_BG_BAR), 0);
        lv_obj_set_style_border_color(panel, theme_color(COLOR_BORDER), 0);

        for (int i = 0; i < ITEM_COUNT; i++) {
            lv_obj_t* row = lv_label_create(panel);
            lv_label_set_text(row, ITEMS[i].label);
            lv_obj_set_style_text_font(row, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(row, theme_color(COLOR_TEXT_MAIN), 0);
            lv_obj_set_width(row, lv_pct(100));
            lv_obj_set_style_pad_all(row, 8, 0);
            lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(row, item_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)ITEMS[i].dest);
        }
    }

    void sidebar_init(lv_obj_t* parent, SidebarCallbacks cb) {
        parent_obj = parent;
        callbacks = cb;
        build_panel();
        visible = false;
    }

    void sidebar_show() {
        if (!panel) return;
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(panel);
        lv_obj_set_pos(panel, 0, 0);
        visible = true;
    }

    void sidebar_hide() {
        if (!panel) return;
        lv_obj_set_pos(panel, -140, 0);
        lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
        visible = false;
    }

    bool sidebar_is_visible() { return visible; }
    void sidebar_toggle() { visible ? sidebar_hide() : sidebar_show(); }

    void sidebar_rebuild() {
        if (panel) { lv_obj_delete(panel); panel = nullptr; }
        build_panel();
        visible = false;
    }
}
