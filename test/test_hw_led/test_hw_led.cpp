#include <Arduino.h>
#include <TFT_eSPI.h> // Uses your project's configured display driver configuration

// Hardwired Cheap Yellow Display (CYD) Tri-Color LED Pins
#define LED_PIN_R  4
#define LED_PIN_G  16
#define LED_PIN_B  17

TFT_eSPI tft = TFT_eSPI();

// COMMON ANODE CONSTRAINT HELPER MATRICES
// Because 255 is OFF and 0 is FULL BRIGHTNESS, we write our logic inverted:
void setRGB(uint8_t red, uint8_t green, uint8_t blue) {
    analogWrite(LED_PIN_R, 255 - red);
    analogWrite(LED_PIN_G, 255 - green);
    analogWrite(LED_PIN_B, 255 - blue);
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
    // ----------------------------------------------------
    // STAGE 1: Solid Amber (Hardware Core/Display Init)
    // ----------------------------------------------------
    Serial.println("[Test Matrix] Executing Stage 1: Solid Amber (HW Boot Check)");
    updateDisplayStatus("Test 1 / 7", "HW Initialization", "Solid Amber");
    setRGB(180, 45, 0); // Mixed elements: heavy red, light green, no blue
    delay(4000);

    // ----------------------------------------------------
    // STAGE 2: Solid Blue (Network Association)
    // ----------------------------------------------------
    Serial.println("[Test Matrix] Executing Stage 2: Solid Blue (Wi-Fi Scan/Auth)");
    updateDisplayStatus("Test 2 / 7", "Network Search", "Solid Blue");
    setRGB(0, 0, 150); // Steady dim blue configuration alignment
    delay(4000);

    // ----------------------------------------------------
    // STAGE 3: Pulsing Cyan (SNTP / Sync Activity Tracking)
    // ----------------------------------------------------
    Serial.println("[Test Matrix] Executing Stage 3: Pulsing Cyan (SNTP / API Fetch)");
    updateDisplayStatus("Test 3 / 7", "Services Sync Loop", "Breathing Cyan");
    
    // Execute a mock 4-second breathing fade calculation loop
    for (int cycle = 0; cycle < 2; cycle++) {
        // Fade Up
        for (int b = 10; b <= 120; b += 2) {
            setRGB(0, b, b); // Mix green and blue matching bounds
            delay(15);
        }
        // Fade Down
        for (int b = 120; b >= 10; b -= 2) {
            setRGB(0, b, b);
            delay(15);
        }
    }

    // ----------------------------------------------------
    // STAGE 4: Faint Pass Green (System Ready/Operational Indicator)
    // ----------------------------------------------------
    Serial.println("[Test Matrix] Executing Stage 4: Dim Green (Operational Confirmation)");
    updateDisplayStatus("Test 4 / 7", "Dashboard Launch", "Dim Green (1 Sec)");
    setRGB(0, 25, 0); // Very low intensity green element logic step
    delay(1000);
    setRGB(0, 0, 0);  // Dark idle mode state transition tracking
    delay(1500);

    // ----------------------------------------------------
    // STAGE 5: Breathing Red/Magenta (Post-Boot Lost Link Interface)
    // ----------------------------------------------------
    Serial.println("[Test Matrix] Executing Stage 5: Breathing Red/Magenta (Link Lost Warning)");
    updateDisplayStatus("Test 5 / 7", "Wi-Fi Disconnected", "Breathing Magenta");
    
    for (int cycle = 0; cycle < 2; cycle++) {
        for (int b = 10; b <= 100; b += 2) {
            setRGB(b, 0, b / 2); // Mix Red with light blue trace parameters
            delay(20);
        }
        for (int b = 100; b >= 10; b -= 2) {
            setRGB(b, 0, b / 2);
            delay(20);
        }
    }

    // ----------------------------------------------------
    // STAGE 6: Crisp Data Traffic Pulse (Dim Traffic Ingress Node)
    // ----------------------------------------------------
    Serial.println("[Test Matrix] Executing Stage 6: Crisp Dim Cyan Pulse (Data Ingress)");
    updateDisplayStatus("Test 6 / 7", "Live Processing", "3x Data Packet Pulses");
    
    for (int pulses = 0; pulses < 3; pulses++) {
        setRGB(0, 40, 60); // Faint Cyan hue profile parameters
        delay(30);         // Ultra fast high speed clean dynamic burst duration
        setRGB(0, 0, 0);   // Return to standby matrix configuration immediately
        delay(1000);       // Inter-packet arrival spacing wait
    }

    // ----------------------------------------------------
    // STAGE 7: System Fault Warning (Triple Red Lockout)
    // ----------------------------------------------------
    Serial.println("[Test Matrix] Executing Stage 7: Triple Red Flash (Critical Alarm)");
    updateDisplayStatus("Test 7 / 7", "System Fault Alarm", "Triple Red Strobe");
    
    for (int warningLoop = 0; warningLoop < 2; warningLoop++) {
        for (int flashCount = 0; flashCount < 3; flashCount++) {
            setRGB(200, 0, 0); // Sharp intense bright red notification alert
            delay(100);
            setRGB(0, 0, 0);
            delay(100);
        }
        delay(1500); // Rhythmic sequencing inter-burst delay step
    }

    // ----------------------------------------------------
    // BONUS STAGE: High Priority Inbound Flash (HamAlert/APRS Strobe)
    // ----------------------------------------------------
    Serial.println("[Test Matrix] Executing Bonus: White/Magenta Strobe (High-Priority RX Message)");
    updateDisplayStatus("Bonus Test", "APRS/HamAlert Inbound", "White & Magenta Strobe");
    
    for (int strobeCount = 0; strobeCount < 12; strobeCount++) {
        setRGB(120, 120, 120); // Balances channels for clean crisp White array mapping
        delay(125);
        setRGB(150, 0, 150);   // Intense sharp Magenta hue step
        delay(125);
    }
    setRGB(0, 0, 0);
    delay(2000);
}