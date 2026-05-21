#ifndef TOUCH_H
#define TOUCH_H

// Hardware Abstraction Layer interface for the XPT2046 resistive touch controller.
// Initializes the dedicated touch SPI bus and registers the input tracking pointer device with the LVGL engine.
void touch_init();

#endif // TOUCH_H
