#ifndef DISPLAY_H
#define DISPLAY_H

// Hardware Abstraction Layer interface for the CYD display panel.
// Handles display subsystem startup, configuration routing, and periodic redraw cycles.

// Initializes the TFT display screen, backlight pin, and maps the layout render buffers into the core LVGL framework
void display_init();

// Services the LVGL internal timer task handler and increments systemic tick timings inside the main runtime loop
void display_update();

#endif // DISPLAY_H
