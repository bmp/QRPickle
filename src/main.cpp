#include <Arduino.h>
#include "hw/display.h"
#include "hw/touch.h"
#include "hw/sensor.h"
#include "services/wifi_manager.h"
#include "core/timekeeper.h" // Include our brand new core clock worker
#include "ui/ui.h"
#include <lvgl.h>

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("--- FoxClock Booting Subsystems (Milestone 3) ---");

    // Start hardware drivers
    display_init();
    touch_init();
    sensor_init();

    // Start background core engines and network services
    wifi_manager_init();
    timekeeper_init();

    // Launch UI shell engine layouts
    ui_init();

    Serial.println("--- HAL Execution Successful. All Engines Operating. ---");
}

void loop() {
    display_update();
    wifi_manager_update();
    timekeeper_update(); // Process internal timekeeping ticks continuously

    delay(5);
}
