#pragma once
#include <stdint.h>

// What the status LED shows, as a pure function of state and time (no hardware access), so the
// patterns documented in docs/LEDColours.md can be unit-tested on the host (test/test_led).
namespace hw {
    namespace led_rgb {

        // Operational Status and Diagnostic System States
        enum LEDState {
            STATE_OFF,
            STATE_BOOT_HW,       // Solid Amber (Core Verification)
            STATE_BOOT_WIFI,     // Solid Blue (Network Search)
            STATE_BOOT_SYNC,     // Off (the Cyan breathing was disabled)
            STATE_BOOT_READY,    // Dim Green for 1 s, then Off
            STATE_WIFI_LOST,     // Off (the Magenta breathing was disabled)
            STATE_FAULT          // Rhythmic Triple Red Flash Loop
        };

        struct Rgb { uint8_t r, g, b; };

        constexpr uint32_t STROBE_MS = 3000;  // priority strobe (APRS message / HamAlert)
        constexpr uint32_t PULSE_MS = 30;     // traffic pulse

        // Satellite pass alert (services/space_manager): an ISS pass with an SSTV event or school
        // contact is coming (SOON: purple double blink every 10 s) or under way (NOW: slow purple breathing).
        enum class PassAlert : uint8_t { NONE, SOON, NOW };
        constexpr uint32_t PASS_SOON_PERIOD_MS = 10000;
        constexpr uint32_t PASS_NOW_PERIOD_MS = 4000;

        // Colour for `state` at `now_ms`. An age is the time since that effect was triggered,
        // or -1 when it isn't active; the strobe overrides the pulse, which overrides the pass
        // alert, which overrides the state.
        Rgb pattern_color(LEDState state, uint32_t now_ms, int32_t strobe_age_ms, int32_t pulse_age_ms,
                          PassAlert pass = PassAlert::NONE);

        // The CYD's RGB LED is common anode: a channel is fully on at PWM duty 0 and off at 255.
        constexpr uint8_t pwm_duty(uint8_t level) { return (uint8_t)(255 - level); }

    }
}
