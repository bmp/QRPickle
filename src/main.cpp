#include <Arduino.h>
#include "hw/display.h"
#include "hw/touch.h"
#include "hw/sensor.h" // Include our new sensor driver
#include "ui/ui.h"
#include <lvgl.h>

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("--- FoxClock Booting Subsystems (Milestone 2) ---");

    // Start all core hardware subsystems
    display_init();
    touch_init();
    sensor_init(); // Initialize the BME280 interface

    // Launch UI shell engine layouts
    ui_init();

    Serial.println("--- HAL Execution Successful. UI Engine Spinning. ---");
}

void loop() {
    display_update();
    delay(5);
}
