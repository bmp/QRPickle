#include <Arduino.h>
#include "hw/display.h"
#include "hw/touch.h"
#include "hw/sensor.h"
#include "core/config_manager.h" // Mount the file storage engine
#include "core/timekeeper.h"
#include "services/wifi_manager.h"
#include "services/web_server.h" // Add the include handler
#include "ui/ui.h"
#include <lvgl.h>

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("--- FoxClock Booting Subsystems (Milestone 4) ---");

    // 1. Initialize data layer storage configurations first
    config_manager_init();

    // 2. Start physical hardware layers
    display_init();
    touch_init();
    sensor_init();

    // 3. Kick off background network services and clock tasks
    wifi_manager_init();
    timekeeper_init();

    // 4. Launch visual UI engine shell
    ui_init();

    Serial.println("--- HAL Execution Successful. All Cores Online. ---");
}

void loop() {
    display_update();
    wifi_manager_update();
    timekeeper_update();
    web_server_update();
    delay(5);
}
