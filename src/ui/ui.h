#ifndef UI_H
#define UI_H

// Core page identifiers for our navigation system
enum ui_page_t {
    PAGE_CLOCK,
    PAGE_WEATHER,
    PAGE_SETTINGS
};

// Initializes the layout engine and triggers the boot splash screen
void ui_init();

// Central routing manager to transition cleanly between distinct operational views
void ui_navigate_to(ui_page_t target_page);

#endif // UI_H
