#include "screen_cycle.h"
#include "sidebar.h"
#include "status_bar.h"
#include "ui.h"
#include "screens/xota.h"
#include "../config/config.h"
#include "../services/display_manager.h"
#include "../services/dx_manager.h"
#include <Arduino.h>
#include <lvgl.h>

namespace ui {
    namespace cycle {

        static bool user_paused = false;
        static State current = State::OFF;
        static uint32_t last_switch_ms = 0;
        static int index = -1;  // the screen shown last by the cycle

        static void show(Screen s) {
            switch (s) {
                case DASHBOARD:  ui_navigate_local(PAGE_DASHBOARD); break;
                case BAND:       ui_navigate_local(PAGE_BAND_COND); break;
                case WEATHER:    ui_navigate_local(PAGE_WEATHER); break;
                case DX:         ui_navigate_local(PAGE_SPOTS); break;
                case POTA:       xota_open_for_cycle(false); break;
                case SOTA:       xota_open_for_cycle(true); break;
                case APRS_RADAR: ui_navigate_local(PAGE_APRS_RADAR); break;
                default:         break;
            }
        }

        static State compute() {
            const auto& cfg = config::get();
            if (!cfg.cycle_enabled) return State::OFF;
            if (user_paused) return State::PAUSED_USER;
            const bool busy = services::display_manager::is_sleeping() || ui_active_page() == PAGE_SETTINGS ||
                              sidebar_is_visible() || ui_active_page() == PAGE_APRS_MSG;
            if (busy || lv_display_get_inactive_time(nullptr) < RESUME_AFTER_MS) return State::PAUSED_TOUCH;
            return State::RUNNING;
        }

        static void tick(lv_timer_t*) {
            // The DX cluster stays connected while it is cycled: read its socket here as well, so
            // spots don't pile up in the network buffers while another screen is shown.
            if (keeps_dx_connected() && ui_active_page() != PAGE_SPOTS) services::DxManager::update();
            const State s = compute();
            if (s != current) {
                current = s;
                status_bar_set_cycle(static_cast<int>(s));
                if (s == State::RUNNING) last_switch_ms = millis();  // a full interval on the screen in view
            }
            if (s != State::RUNNING) return;
            const auto& cfg = config::get();
            if (millis() - last_switch_ms < (uint32_t)cfg.cycle_seconds * 1000) return;
            for (int step = 1; step <= SCREEN_COUNT; step++) {  // next chosen screen
                const int next = (index + step) % SCREEN_COUNT;
                if (cfg.cycle_screens & (1u << next)) {
                    index = next;
                    show(static_cast<Screen>(next));
                    lv_mem_monitor_t mon;
                    lv_mem_monitor(&mon);
                    Serial.printf("[Cycle] screen %d, LVGL pool %u%% used (peak %u B, largest free %u B)\n", next,
                                  mon.used_pct, (unsigned)mon.max_used, (unsigned)mon.free_biggest_size);
                    break;
                }
            }
            last_switch_ms = millis();
        }

        void init() {
            lv_timer_create(tick, 500, nullptr);
            status_bar_set_cycle(static_cast<int>(State::OFF));
        }

        State state() { return current; }

        void toggle_pause() {
            user_paused = !user_paused;
            current = compute();
            status_bar_set_cycle(static_cast<int>(current));
            last_switch_ms = millis();
        }

        bool keeps_dx_connected() {
            const auto& cfg = config::get();
            return cfg.cycle_enabled && (cfg.cycle_screens & (1u << DX));
        }

    }  // namespace cycle
}  // namespace ui
