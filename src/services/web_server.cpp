#include "web_server.h"
#include "../core/config_manager.h"
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Arduino.h>

// Instantiate the web engine listening on the standard HTTP port 80
static AsyncWebServer server(80);
static bool flag_trigger_reboot = false;
static unsigned long reboot_timer_mark = 0;

// High-contrast CSS embedded directly inside the HTML page string token
const char setup_html[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>FoxClock Tactical Setup</title>
    <style>
        body { background: #0A0A0A; color: #33FF33; font-family: monospace; padding: 20px; }
        .box { border: 1px solid #FFB000; padding: 20px; max-width: 400px; margin: auto; background: #141414; }
        h2 { color: #FFB000; text-align: center; margin-top: 0; }
        label { display: block; margin: 15px 0 5px; color: #00FFFF; }
        input, select { width: 100%; padding: 10px; background: #1C1C1C; border: 1px solid #555; color: #fff; box-sizing: border-box; font-family: monospace; }
        input:focus, select:focus { border-color: #FFB000; outline: none; }
        button { width: 100%; padding: 12px; margin-top: 20px; background: #FFB000; border: none; color: #0A0A0A; font-weight: bold; cursor: pointer; font-family: monospace; }
        button:hover { background: #E09E00; }
    </style>
</head>
<body>
    <div class="box">
        <h2>FOXCLOCK SYSTEM SETUP</h2>
        <form action="/save" method="POST">
            <label>LOCAL NETWORK SSID (Wi-Fi Name):</label>
            <input type="text" name="ssid" placeholder="Enter Network Name" required maxlength="31">

            <label>NETWORK SECURITY KEY (Password):</label>
            <input type="password" name="pass" placeholder="Enter Password" required maxlength="63">

            <label>GEOGRAPHIC TIMEZONE OFFSET (Hours from UTC):</label>
            <input type="number" name="offset" step="0.5" min="-12" max="14" value="5.5" placeholder="e.g. 5.5 for IST, -8 for PST" required>

            <button type="submit">COMMIT PARAMETERS TO FLASH</button>
        </form>
    </div>
</body>
</html>
)rawhtml";

void web_server_init() {
    Serial.println("[WebUI] Binding asynchronous route handlers to socket engine...");

    // Endpoint 1: Serve the configuration matrix root web form page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", setup_html);
    });

    // Endpoint 2: Process incoming configuration credentials post payloads
    server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {
        sys_config_t * cfg = config_get_runtime();

        // Safely map and slice incoming fields straight into our active LittleFS configuration cache
        if (request->hasParam("ssid", true)) {
            strncpy(cfg->wifi_ssid, request->getParam("ssid", true)->value().c_str(), sizeof(cfg->wifi_ssid) - 1);
        }
        if (request->hasParam("pass", true)) {
            strncpy(cfg->wifi_secret, request->getParam("pass", true)->value().c_str(), sizeof(cfg->wifi_secret) - 1);
        }
        if (request->hasParam("offset", true)) {
          cfg->timezone_offset = request->getParam("offset", true)->value().toFloat(); // The Fix!
        }

        // Commit configuration variables down to raw physical flash memory
        config_manager_save();

        // Serve a responsive success completion confirmation message string back to the user
        request->send(200, "text/html", "<h3>PARAMETERS INGESTED SUCCESSFUL! FoxClock is executing reboot...</h3>");

        // Set flags to trigger a delayed hardware reset (gives TCP sockets time to finish transmission)
        flag_trigger_reboot = true;
        reboot_timer_mark = millis();
    });

    server.begin();
    Serial.println("[WebUI] Server listening securely on port 80.");
}

void web_server_stop() {
    server.end();
    Serial.println("[WebUI] Server sockets successfully taken offline.");
}

void web_server_update() {
    // Pro-Tier Web Pattern: Enforce a deliberate 2000ms delay block before resetting the CPU
    // This allows the web response packets to cleanly slip clear of the antenna buffer loops first!
    if (flag_trigger_reboot && (millis() - reboot_timer_mark > 2000)) {
        Serial.println("[WebUI] Reboot cooldown frame completed. Executing CPU Reset vector...");
        ESP.restart();
    }
}
