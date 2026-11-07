#include "services/ota_manager.h"
#include "core/crashlog.h"
#include "core/mem_stats.h"
#include <Arduino.h>
#include <esp_task_wdt.h>
#include <esp_heap_caps.h>
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
#include "services/cloud_ota.h"
#include "services/net_lock.h"
#include "services/aprs_manager.h"
#include "ui/ui.h"
#include "ui/fonts.h"
#ifdef QRP_SCREEN_TOOLS
#include "core/screen_tools.h"
#endif

// Free 8-bit heap after each start-up step, printed once at boot (docs/MEMORY_AUDIT.md).
static void heap_step(const char* what) {
    Serial.printf("[HEAP] %-8s free %6u  largest %6u\n", what, (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

void setup() {
    hw::led_rgb::init();
    hw::led_rgb::set_state(hw::led_rgb::STATE_BOOT_HW);
    Serial.begin(115200);
    heap_step("start");
    delay(500);
    crashlog::report_previous();
    services::net_lock_boot_report();
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
    config::log_summary();
    Serial.flush();
    services::SpaceManager::init();

    Serial.println("[Boot Check] Initializing Sensors...");
    Serial.flush();
    sensor_init();
    heap_step("sensor");
    Serial.println("[Boot Check] Sensors OK.");
    Serial.flush();

    Serial.println("[Boot Check] Initializing Display Driver...");
    Serial.flush();
    display_init();
    heap_step("display");

    core::lvgl_fs_init();
    heap_step("fs");

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
    heap_step("wifi");

    fonts_init();
    ui::ui_init();
    heap_step("ui");
    web_server_init();
    heap_step("web");

    Serial.println("--- All operational tasks successfully scheduled ---");
    Serial.flush();

    // Force the LED state to OFF after the boot sequence is complete
    // This instantly kills the stuck "breathing cyan" timekeeper loop
    // (unless the setup hotspot is up: its amber stays)
    hw::led_rgb::set_state(hw::led_rgb::boot_done_state(hw::led_rgb::STATE_OFF, wifi_manager_setup_hotspot()));

    // Watch the main loop: a hang now reboots after 30 s with a backtrace and [CRASHLOG]
    // breadcrumbs instead of freezing silently (found with the DX screen, 2026-10-09).
    // 30 s leaves room for the slowest legitimate loop work (a weather HTTPS fetch).
    esp_task_wdt_init(30, true);
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
    services::net_lock_watch();  // a download that never returns: warn, then restart (lock_watch.h)
    services::PropagationManager::update();
    services::SpaceManager::update();
    mem_stats::update();
#ifdef QRP_TEST_NETLOCK_HOLD  // test builds only (docs/TESTING.md): a task holds NetLock for 45 s (or
    // QRP_TEST_NETLOCK_HOLD_S) at
    // 2 min while weather is due; the main loop must not wait for it (30 s loop watchdog). Never release.
    {
        static bool started = false;
        if (!started && millis() > 120000) {
            started = true;
            xTaskCreate([](void*) {
                {
                    services::NetLock lock;
#ifndef QRP_TEST_NETLOCK_HOLD_S
#define QRP_TEST_NETLOCK_HOLD_S 45  // 200: past the lock watch's 180 s (lock_watch.h)
#endif
                    Serial.printf("[Test] NetLock held: %d (%d s)\n", lock.held(), QRP_TEST_NETLOCK_HOLD_S);
                    vTaskDelay(pdMS_TO_TICKS(QRP_TEST_NETLOCK_HOLD_S * 1000));
                }
                Serial.println("[Test] NetLock released");
                vTaskDelete(nullptr);
            },
                        "netlock_hold", 3072, nullptr, 1, nullptr);
            vTaskDelay(pdMS_TO_TICKS(200));
            services::weather_manager::force_refresh();
        }
    }
#endif
#ifdef QRP_STRESS  // test builds only (docs/TESTING.md, "Stress run"): downloads 10-20x as often; never release
    {
        // Every minute after the boot rush, one of: weather (every 3 min), satellite feed (every
        // 15 min, the screen button's limit), solar (manual limit), update check (every 10 min),
        // and an APRS-IS reconnect every 10 min.
        static uint32_t last_min = 0;
        const uint32_t m = millis() / 60000;
        if (m >= 3 && m != last_min) {
            last_min = m;
            if (m % 3 == 0) services::weather_manager::force_refresh();
            if (m % 15 == 1) services::SpaceManager::refresh_now();
            if (m % 10 == 2) services::PropagationManager::refresh_now();
            if (m % 10 == 7) services::cloud_ota::force_update_check();
            if (m % 10 == 5) services::AprsManager::stop();  // APRS-IS reconnect: off for a minute
            if (m % 10 == 6) services::AprsManager::start();
        }
    }
#endif
#ifdef QRP_SCREEN_TOOLS
    screen_tools::update();
#endif
    services::ota_manager::mark_healthy_if_ready(wifi_manager_is_connected());
    crashlog::mark(crashlog::SLOT_LOOP, 7); delay(5);
}