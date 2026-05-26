#include <Arduino.h>
#include "hw/display.h"
#include "hw/touch.h"
#include "hw/sensor.h"
#include "hw/led_rgb.h"
#include "config/config.h"
#include "core/timekeeper.h"
#include "core/lvgl_fs.h" // FIXED: Include the new bridge
#include "services/wifi_manager.h"
#include "services/web_server.h"
#include "services/display_manager.h"
#include "services/weather_manager.h"
#include "ui/ui.h"
#include "ui/fonts.h"

void setup() {
    hw::led_rgb::init();
    hw::led_rgb::set_state(hw::led_rgb::STATE_BOOT_HW);
    Serial.begin(115200);
    delay(500);
    Serial.println("\n--- QRPickle System Initializing (NVS Production Core) ---");
    Serial.printf("[Memory] Total Internal RAM: %u bytes\n", ESP.getHeapSize());
    Serial.printf("[Memory] Total PSRAM: %u bytes\n", ESP.getPsramSize());
    Serial.printf("[Memory] Free PSRAM: %u bytes\n", ESP.getFreePsram());
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

    // FIXED: Register the filesystem bridge directly to the core
    core::lvgl_fs_init();

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
    services::display_manager::update();
    services::weather_manager::update();
    delay(5);
}
