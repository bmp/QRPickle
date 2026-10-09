// On-device VISUAL check of the status LED (not a Unity test; nothing builds it automatically).
// It shows each state with the firmware's own pattern code (src/hw/led_pattern), so what you see
// is what the firmware shows. The automated test of the same rules is test/test_led (host).
// Build it as a temporary sketch, e.g. copy it over src/main.cpp in a scratch branch.
#include <Arduino.h>
#include <TFT_eSPI.h> // Uses your project's configured display driver configuration
#include "../../src/hw/led_pattern.h"
#include "../../src/hw/led_pattern.cpp"

// Hardwired Cheap Yellow Display (CYD) Tri-Color LED Pins
#define LED_PIN_R  4
#define LED_PIN_G  16
#define LED_PIN_B  17

TFT_eSPI tft = TFT_eSPI();

// COMMON ANODE CONSTRAINT HELPER MATRICES
// Because 255 is OFF and 0 is FULL BRIGHTNESS, we write our logic inverted:
void setRGB(uint8_t red, uint8_t green, uint8_t blue) {
    analogWrite(LED_PIN_R, hw::led_rgb::pwm_duty(red));
    analogWrite(LED_PIN_G, hw::led_rgb::pwm_duty(green));
    analogWrite(LED_PIN_B, hw::led_rgb::pwm_duty(blue));
}

// Runs one documented state for `ms`, with optional strobe/pulse, using the firmware's rules.
void show(hw::led_rgb::LEDState state, uint32_t ms, bool strobe = false, bool pulse = false) {
    const uint32_t start = millis();
    while (millis() - start < ms) {
        const int32_t age = (int32_t)(millis() - start);
        hw::led_rgb::Rgb c = hw::led_rgb::pattern_color(state, millis(), strobe ? age : -1, pulse ? age : -1);
        setRGB(c.r, c.g, c.b);
        delay(5);
    }
}

// Clear screen and draw updated test description text banner blocks securely
void updateDisplayStatus(const char* testNumber, const char* stateName, const char* colorProfile) {
    tft.fillScreen(TFT_BLACK);
    
    tft.setTextColor(TFT_GOLD, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("QRPickle Hardware LED Test", 10, 20);
    
    // Draw the horizontal separator line directly
    tft.drawFastHLine(10, 45, 300, TFT_DARKGREY);
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString(testNumber, 10, 70);
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("System State:", 10, 110);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString(stateName, 150, 110);
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("LED Target:", 10, 150);
    tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
    tft.drawString(colorProfile, 150, 150);
    
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString("Observing terminal line output telemetry log channels...", 10, 210);
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n--- Launching QRPickle RGB Verification Array ---");

    // Initialize the physical pins
    pinMode(LED_PIN_R, OUTPUT);
    pinMode(LED_PIN_G, OUTPUT);
    pinMode(LED_PIN_B, OUTPUT);
    
    // Start with the hardware elements turned completely OFF (Common Anode high logic level)
    setRGB(0, 0, 0);

    // Boot up the native screen parameters
    tft.begin();
    tft.setRotation(1); // Landscape profile mirroring configuration alignment mapping
    tft.fillScreen(TFT_BLACK);
}

void loop() {
    using namespace hw::led_rgb;
    struct Step { const char* n; const char* name; const char* colour; LEDState s; uint32_t ms; bool strobe; bool pulse; };
    const Step steps[] = {
        {"1 / 7", "HW Initialization",  "Solid Amber",          STATE_BOOT_HW,    4000, false, false},
        {"2 / 7", "Network Search",     "Solid Blue",           STATE_BOOT_WIFI,  4000, false, false},
        {"3 / 7", "Services Sync",      "Off (disabled)",       STATE_BOOT_SYNC,  2000, false, false},
        {"4 / 7", "Dashboard Launch",   "Dim Green (1 s)",      STATE_BOOT_READY, 1000, false, false},
        {"5 / 7", "Wi-Fi Disconnected", "Off (disabled)",       STATE_WIFI_LOST,  2000, false, false},
        {"6 / 7", "Live Processing",    "Faint Cyan (30 ms)",   STATE_OFF,          30, false, true},
        {"7 / 7", "System Fault",       "Triple Red Flash",     STATE_FAULT,      6000, false, false},
        {"Bonus", "APRS/HamAlert",      "White/Magenta Strobe", STATE_OFF,        3000, true,  false},
    };
    for (const Step& st : steps) {
        Serial.printf("[LED test] %s %s: %s\n", st.n, st.name, st.colour);
        updateDisplayStatus(st.n, st.name, st.colour);
        show(st.s, st.ms, st.strobe, st.pulse);
        setRGB(0, 0, 0);
        delay(1500);
    }
}
