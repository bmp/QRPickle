#include "sidebar.h"
#include "theme.h"
#include "fonts.h"
#include <lvgl.h>

namespace ui {

    static lv_obj_t* menu_scr = nullptr; 
    static SidebarCallbacks callbacks{};

    struct Item { const char* label; DestScreen dest; };
    static const Item ITEMS[] = {
        {"Dashboard",   DEST_DASHBOARD},
        {"Weather",     DEST_WEATHER},
        {"Network",     DEST_NETWORK},
        {"Settings",    DEST_SETTINGS},
        {"DX Cluster",  DEST_SPOTS},
        {"xOTA",        DEST_XOTA},
        {"APRS",        DEST_APRS},// FIXED: Now routes to unified module
        {"Propagation", DEST_PROP},
        {"HamAlert",    DEST_HAMALERT},
        {"Cloud OTA",   DEST_CLOUD_OTA}
    };
    static constexpr int ITEM_COUNT = sizeof(ITEMS) / sizeof(ITEMS[0]);

    static void item_clicked(lv_event_t* e) {
        DestScreen dest = (DestScreen)(intptr_t)lv_event_get_user_data(e);
        sidebar_hide(); 
        if (callbacks.on_select) callbacks.on_select(dest);
    }

    static void close_clicked(lv_event_t* e) {
        sidebar_hide();
    }

    static void build_menu() {
        if (menu_scr) {
            lv_obj_delete(menu_scr);
            menu_scr = nullptr;
        }

        menu_scr = lv_obj_create(lv_layer_top());
        lv_obj_set_size(menu_scr, 320, 240);
        lv_obj_set_style_bg_color(menu_scr, theme_color(COLOR_BG_APP), 0);
        lv_obj_set_style_border_width(menu_scr, 0, 0);
        lv_obj_set_style_pad_all(menu_scr, 0, 0);
        lv_obj_clear_flag(menu_scr, LV_OBJ_FLAG_SCROLLABLE);
        
        lv_obj_add_flag(menu_scr, LV_OBJ_FLAG_HIDDEN);

        // --- Header Bar ---
        lv_obj_t* header = lv_obj_create(menu_scr);
        lv_obj_set_size(header, 320, 30);
        lv_obj_set_style_bg_color(header, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_width(header, 0, 0);
        lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(header, theme_color(COLOR_BORDER), 0);
        lv_obj_set_style_border_width(header, 1, 0);
        lv_obj_set_style_pad_all(header, 0, 0);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* title = lv_label_create(header);
        lv_label_set_text(title, "MAIN MENU");
        lv_obj_set_style_text_font(title, &font_jetbrains_14, 0);
        lv_obj_set_style_text_color(title, theme_color(COLOR_TEXT_MAIN), 0);
        lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);

        // Close Button (X)
        lv_obj_t* close_btn = lv_button_create(header);
        lv_obj_set_size(close_btn, 40, 30);
        lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_style_bg_color(close_btn, theme_color(COLOR_BG_PANEL), 0);
        lv_obj_set_style_border_width(close_btn, 0, 0);
        lv_obj_set_style_radius(close_btn, 0, 0);
        lv_obj_add_event_cb(close_btn, close_clicked, LV_EVENT_CLICKED, nullptr);
        
        lv_obj_t* close_lbl = lv_label_create(close_btn);
        lv_label_set_text(close_lbl, "X");
        lv_obj_set_style_text_font(close_lbl, &font_jetbrains_14, 0);
        lv_obj_set_style_text_color(close_lbl, theme_color(COLOR_TEXT_MUTED), 0);
        lv_obj_center(close_lbl);

        // --- Auto-Scaling Button Grid ---
        lv_obj_t* grid = lv_obj_create(menu_scr);
        lv_obj_set_size(grid, 320, 210); 
        lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(grid, 0, 0);
        
        int cols = 2;
        int rows = (ITEM_COUNT + cols - 1) / cols; 
        int grid_pad = 8;
        int grid_gap = 8;
        
        int usable_w = 320 - (grid_pad * 2);
        int usable_h = 210 - (grid_pad * 2);
        int btn_w = (usable_w - (grid_gap * (cols - 1))) / cols;
        int btn_h = (usable_h - (grid_gap * (rows - 1))) / rows;

        lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_all(grid, grid_pad, 0);
        lv_obj_set_style_pad_row(grid, grid_gap, 0);
        lv_obj_set_style_pad_column(grid, grid_gap, 0);
        lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE); 

        for (int i = 0; i < ITEM_COUNT; i++) {
            lv_obj_t* btn = lv_button_create(grid);
            lv_obj_set_size(btn, btn_w, btn_h);
            
            lv_obj_set_style_bg_color(btn, theme_color(COLOR_BG_PANEL), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(btn, theme_color(COLOR_ACCENT_PRIMARY), LV_PART_MAIN | LV_STATE_PRESSED); 
            
            lv_obj_set_style_border_color(btn, theme_color(COLOR_BORDER), 0);
            lv_obj_set_style_border_width(btn, 1, 0);
            lv_obj_set_style_radius(btn, 4, 0);
            lv_obj_add_event_cb(btn, item_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)ITEMS[i].dest);

            lv_obj_t* lbl = lv_label_create(btn);
            lv_label_set_text(lbl, ITEMS[i].label);
            lv_obj_set_style_text_font(lbl, &font_jetbrains_14, 0);
            
            lv_obj_set_style_text_color(lbl, theme_color(COLOR_TEXT_MAIN), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(lbl, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_PRESSED); 
            lv_obj_center(lbl);
        }
    }

    void sidebar_init(lv_obj_t* parent, SidebarCallbacks cb) {
        callbacks = cb;
        build_menu();
    }

    void sidebar_show() {
        if (menu_scr) {
            lv_obj_clear_flag(menu_scr, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(menu_scr); 
        }
    }

    void sidebar_hide() {
        if (menu_scr) {
            lv_obj_add_flag(menu_scr, LV_OBJ_FLAG_HIDDEN);
        }
    }

    bool sidebar_is_visible() { 
        return menu_scr && !lv_obj_has_flag(menu_scr, LV_OBJ_FLAG_HIDDEN); 
    }
    
    void sidebar_toggle() { 
        sidebar_is_visible() ? sidebar_hide() : sidebar_show(); 
    }

    void sidebar_rebuild() { 
        build_menu(); 
    }
}
