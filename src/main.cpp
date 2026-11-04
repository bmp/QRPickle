#include "services/ota_manager.h"
#include "core/crashlog.h"
#include "core/mem_stats.h"
#include <Arduino.h>
#include <esp_task_wdt.h>
#include "hw/display.h"
#include "hw/touch.h"
#include "hw/sensor.h"
#include "hw/led_rgb.h"
#include "config/config.h"
#include "core/timekeeper.h"
#include "core/lvgl_fs.h"
#include "services/wifi_manager.h"
#include "services/web_server.h"
#include "services/display_manager.h"
#include "services/weather_manager.h"
#include "services/prop_manager.h"
#include "services/space_manager.h"
#include "ui/ui.h"
#include "ui/fonts.h"
#ifdef QRP_SCREEN_TOOLS
#include "core/screen_tools.h"
#endif

#ifdef QRP_HEAP_TRACE  // spike: free 8-bit heap after each start-up step
#include <esp_heap_caps.h>
static void heap_step(const char* what) {
    Serial.printf("[HEAP] %-12s free %6u  largest %6u  internal %6u\n", what,
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}
#define HEAP_STEP(w) heap_step(w)
#else
#define HEAP_STEP(w)
#endif

void setup() {
    hw::led_rgb::init();
    hw::led_rgb::set_state(hw::led_rgb::STATE_BOOT_HW);
    Serial.begin(115200);
    HEAP_STEP("start");
    delay(500);
    crashlog::report_previous();
    services::ota_manager::rollback_boot_check();  // may reboot into the previous image
#ifdef QRP_TEST_CRASH_AT_BOOT
    // Test-only build (never released): proves the OTA rollback guard. See ota_manager.h and docs/RELEASING.md.
    Serial.println("[TEST] QRP_TEST_CRASH_AT_BOOT: crashing in 3 s");
    delay(3000);
    abort();
#endif
    Serial.println("\n--- QRPickle System Initializing (NVS Production Core) ---");
    Serial.printf("[Memory] Total Internal RAM: %u bytes\n", ESP.getHeapSize());
    Serial.printf("[Memory] Total PSRAM: %u bytes\n", ESP.getPsramSize());
    Serial.printf("[Memory] Free PSRAM: %u bytes\n", ESP.getFreePsram());
    Serial.flush();

    config::load();
    HEAP_STEP("config");
    config::log_summary();
    Serial.flush();
    services::SpaceManager::init();
    HEAP_STEP("space");

    Serial.println("[Boot Check] Initializing Sensors...");
    Serial.flush();
    sensor_init();
    HEAP_STEP("sensor");
    Serial.println("[Boot Check] Sensors OK.");
    Serial.flush();

    Serial.println("[Boot Check] Initializing Display Driver...");
    Serial.flush();
    display_init();
    HEAP_STEP("display");

    core::lvgl_fs_init();
    HEAP_STEP("lvgl_fs");

    Serial.println("[Boot Check] Display Driver OK.");
    Serial.flush();

    Serial.println("[Boot Check] Initializing Touch Controller...");
    Serial.flush();
    touch_init();
    HEAP_STEP("touch");
    Serial.println("[Boot Check] Touch Controller OK.");
    Serial.flush();

    Serial.println("[Boot Check] Launching Network Stack...");
    Serial.flush();
    wifi_manager_init();
    HEAP_STEP("wifi");

    fonts_init();
    HEAP_STEP("fonts");
    ui::ui_init();
    HEAP_STEP("ui");
    web_server_init();
    HEAP_STEP("web");

    Serial.println("--- All operational tasks successfully scheduled ---");
    Serial.flush();

    // Force the LED state to OFF after the boot sequence is complete
    // This instantly kills the stuck "breathing cyan" timekeeper loop
    // (unless the setup hotspot is up: its amber stays)
    hw::led_rgb::set_state(hw::led_rgb::boot_done_state(hw::led_rgb::STATE_OFF, wifi_manager_setup_hotspot()));

    // Watch the main loop: a hang now reboots after 30 s with a backtrace and [CRASHLOG]
    // breadcrumbs instead of freezing silently (found with the DX screen, 2026-10-09).
    // 30 s leaves room for the slowest legitimate loop work (a weather HTTPS fetch).
#if ESP_IDF_VERSION_MAJOR >= 5
    // ESP-IDF 5: the task watchdog is already running (Arduino core 3); set 30 s and panic.
    const esp_task_wdt_config_t wdt = {.timeout_ms = 30000, .idle_core_mask = 0, .trigger_panic = true};
    esp_task_wdt_reconfigure(&wdt);
#else
    esp_task_wdt_init(30, true);
#endif
    enableLoopWDT();
}

void loop() {
    crashlog::mark(crashlog::SLOT_LOOP, 1); wifi_manager_update();
    crashlog::mark(crashlog::SLOT_LOOP, 2); timekeeper_update();
    crashlog::mark(crashlog::SLOT_LOOP, 3); ui::display_update();
    crashlog::mark(crashlog::SLOT_LOOP, 4); web_server_update();
    crashlog::mark(crashlog::SLOT_LOOP, 5); services::display_manager::update();
    crashlog::mark(crashlog::SLOT_LOOP, 6); services::weather_manager::update();
    crashlog::mark(crashlog::SLOT_LOOP, 8);
    services::PropagationManager::update();
    services::SpaceManager::update();
    mem_stats::update();
#ifdef QRP_SCREEN_TOOLS
    screen_tools::update();
#endif
    services::ota_manager::mark_healthy_if_ready(wifi_manager_is_connected());
    crashlog::mark(crashlog::SLOT_LOOP, 7); delay(5);
}