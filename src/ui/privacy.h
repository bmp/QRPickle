#pragma once

// Privacy on screens that get photographed and shared (docs/UI_GUIDE.md, "Privacy"). Secrets are
// masked in every build (tap to reveal); screenshot builds (-DQRP_SCREEN_TOOLS, never released)
// also replace identifying details with fixed demo values.
namespace ui {
    namespace privacy {

#ifdef QRP_SCREEN_TOOLS
        constexpr bool SCREENSHOT_BUILD = true;
#else
        constexpr bool SCREENSHOT_BUILD = false;
#endif
        constexpr const char* MASK = "********";

        inline const char* ssid(const char* real) { return SCREENSHOT_BUILD ? "HomeWiFi" : real; }
        inline const char* ip(const char* real) { return SCREENSHOT_BUILD ? "192.168.1.50" : real; }
        inline const char* mac(const char* real) { return SCREENSHOT_BUILD ? "AA:BB:CC:DD:EE:FF" : real; }
        // Distances and bearings to other APRS stations reveal the station's position.
        constexpr bool HIDE_POSITIONS = SCREENSHOT_BUILD;

    }  // namespace privacy
}  // namespace ui
