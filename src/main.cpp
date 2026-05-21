#include <Arduino.h>
#include "hw/display.h"
#include "hw/touch.h"
#include "hw/sensor.h"
#include "config/config.h"
#include "core/timekeeper.h"
#include "services/wifi_manager.h"
#include "services/web_server.h"
#include "ui/ui.h"
#include "ui/fonts.h"

void setup() {
    Serial.begin(115200);
    delay(500); // Give the serial monitor extra time to stabilize
    Serial.println("\n--- FoxClock System Initializing (NVS Production Core) ---");
    Serial.flush();

    config::load();
    config::log_summary();
    Serial.flush();

    Serial.println("[Boot Check] Initializing Sensors...");
    Serial.flush();
    sensor_init();
    Serial.println("[Boot Check] Sensors OK.");
    Serial.flush();

    Serial.println("[Boot Check] Initializing Display Driver...");
    Serial.flush();
    display_init();
    Serial.println("[Boot Check] Display Driver OK.");
    Serial.flush();

    Serial.println("[Boot Check] Initializing Touch Controller...");
    Serial.flush();
    touch_init();
    Serial.println("[Boot Check] Touch Controller OK.");
    Serial.flush();

    Serial.println("[Boot Check] Launching Network Stack...");
    Serial.flush();
    wifi_manager_init();

    fonts_init();
    ui::ui_init();
    web_server_init();

    Serial.println("--- All operational tasks successfully scheduled ---");
    Serial.flush();
}

void loop() {
    wifi_manager_update();
    timekeeper_update();
    ui::display_update();
    web_server_update();
    delay(5);
}
