#ifndef DISPLAY_H
#define DISPLAY_H

// Hardware Abstraction Layer interface for the CYD display panel.
// Handles display subsystem startup, configuration routing, and periodic redraw cycles.

// Initializes the TFT display screen, backlight pin, and maps the layout render buffers into the core LVGL framework
void display_init();

// Services the LVGL internal timer task handler and increments systemic tick timings inside the main runtime loop

// The screen's SPI bus object: touch chips that share the bus (XPT2046 on the R boards) use it too.
class SPIClass;
SPIClass& display_spi();

// Waits until the last pixel transfer has finished (no-op: transfers are blocking).
void display_wait_idle();
#ifdef QRP_BENCH
// Test builds only: logs full-screen redraw times and memory 20 s after boot, three times.
void display_bench_start();
#endif

#endif // DISPLAY_H
