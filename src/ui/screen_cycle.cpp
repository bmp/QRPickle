#include "screen_cycle.h"
#include "screens/splash.h"
#include "sidebar.h"
#include "status_bar.h"
#include "ui.h"
#include "screens/xota.h"
#include "../config/config.h"
#include "../config/config_validation.h"
#include "../services/display_manager.h"
#include "../services/dx_manager.h"
#include "../services/pota_manager.h"
#include "../services/sota_manager.h"
#include <Arduino.h>
#include <lvgl.h>

namespace ui {
    namespace cycle {

        static bool user_paused = false;
        static State current = State::OFF;
        static uint32_t last_switch_ms = 0;
        static int index = -1;  // the position in the cycling order shown last

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
                              sidebar_is_visible() || ui_active_page() == PAGE_APRS_MSG ||
                              splash_preview_active();
            if (busy || lv_display_get_inactive_time(nullptr) < RESUME_AFTER_MS) return State::PAUSED_TOUCH;
            return State::RUNNING;
        }

        static void tick(lv_timer_t*) {
            // The DX cluster stays connected while it is cycled: read its socket here as well, so
            // spots don't pile up in the network buffers while another screen is shown.
            // When the cycle stops keeping it (cycling off, DX unticked, another profile) while the DX
            // screen isn't open, close it: nobody reads the socket any more and the incoming spots
            // held ~20 KB of WiFi buffers within 4 minutes (2026-11-06).
            static bool kept = false;
            const bool keep = keeps_dx_connected();
            if (keep && ui_active_page() != PAGE_SPOTS) services::DxManager::update();
            if (kept && !keep && ui_active_page() != PAGE_SPOTS) services::DxManager::stop();
            kept = keep;
            const State s = compute();
            if (s != current) {
                current = s;
                status_bar_set_cycle(static_cast<int>(s));
                if (s == State::RUNNING) last_switch_ms = millis();  // a full interval on the screen in view
            }
            if (s != State::RUNNING) return;
            const auto& cfg = config::get();
            if (millis() - last_switch_ms < (uint32_t)cfg.cycle_seconds * 1000) return;
            uint8_t order[SCREEN_COUNT];  // the web console's order (sanitised; default as a fallback)
            if (!config::parse_cycle_order(cfg.cycle_order, order))
                for (int i = 0; i < SCREEN_COUNT; i++) order[i] = (uint8_t)i;
            const int pos = config::cycle_next(order, cfg.cycle_screens, index);
            if (pos >= 0) {
                index = pos;
                show(static_cast<Screen>(order[pos]));
                lv_mem_monitor_t mon;
                lv_mem_monitor(&mon);
                Serial.printf("[Cycle] screen %d, LVGL pool %u%% used (peak %u B, largest free %u B)\n", order[pos],
                              mon.used_pct, (unsigned)mon.max_used, (unsigned)mon.free_biggest_size);
            }
            last_switch_ms = millis();
        }

        // Frees the spot lists no screen shows (services/list_release.h): xOTA shows POTA and SOTA,
        // the Spots page shows DX; a list stays while cycling includes its screen.
        static void release_tick(lv_timer_t*) {
            const auto& cfg = config::get();
            const auto page = ui_active_page();
            const bool xota = page == PAGE_XOTA, spots = page == PAGE_SPOTS;
            const auto cycles = [&](Screen s) { return cfg.cycle_enabled && (cfg.cycle_screens & (1u << s)); };
            if (services::PotaManager::release_spots(xota, cycles(POTA))) Serial.println("[Mem] POTA list freed");
            if (services::SotaManager::release_spots(xota, cycles(SOTA))) Serial.println("[Mem] SOTA list freed");
            if (services::DxManager::release_spots(spots, keeps_dx_connected())) Serial.println("[Mem] DX list freed");
        }

        void init() {
            lv_timer_create(tick, 500, nullptr);
            lv_timer_create(release_tick, 5000, nullptr);
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
