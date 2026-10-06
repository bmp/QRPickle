#include "web_server.h"
#include "profile_manager.h"  
#include "ota_manager.h"  
#include "display_manager.h"
#include "cloud_ota.h"
#include "aprs_manager.h"
#include "dx_manager.h"        
#include "hamalert_manager.h"  
#include "../config/config.h"
#include "../config/config_validation.h"
#include "json_copy.h"
#include <atomic>
#include "../core/metadata.h"
#include "../hw/sensor.h"
#include "../ui/status_bar.h"
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Arduino.h>
#include <Update.h>  
#include <esp_arduino_version.h>

static AsyncWebServer server(80);
using services::copy_str;
using services::copy_secret;

static std::atomic<bool> flag_trigger_reboot{false};
static std::atomic<bool> flag_trigger_ui_refresh{false};
static std::atomic<bool> flag_trigger_ota_flash{false};
static unsigned long reboot_timer_mark = 0;

// Config/profile changes are validated in the AsyncTCP task but applied on the main loop,
// so no task ever reads a half-written config (review 1.6).
static std::atomic<config::Config*> pending_config{nullptr};
static char pending_profile[25];
static std::atomic<bool> pending_profile_flag{false};

// Digest auth on every route, user "admin" (review 1.2).
static bool authorized(AsyncWebServerRequest* r) {
    return r->authenticate("admin", config::get().admin_password);
}
#define REQUIRE_AUTH(req) do { if (!authorized(req)) { (req)->requestAuthentication(); return; } } while (0)
// For POST routes with a body handler: the body callback runs first and checks auth itself;
// this completion callback only answers unauthenticated requests.
static void auth_gate(AsyncWebServerRequest* r) { if (!authorized(r)) r->requestAuthentication(); }

static void queue_config(config::Config* staged) {
    config::sanitize(*staged, config::get());
    delete pending_config.exchange(staged);
}

const char fallback_html[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>QRPickle Recovery Panel</title>
    <style>
        body { background: #0F0000; color: #FF3333; font-family: Verdana, Geneva, sans-serif; padding: 20px; text-align: center; }
        .box { border: 2px dashed #FF0000; padding: 20px; max-width: 450px; margin: 40px auto; background: #050000; border-radius: 8px; }
        h1 { font-family: Georgia, serif; color: #FF5555; text-align: center; }
        label { display: block; margin: 15px 0 5px; text-align: left; font-weight: bold; color: #FF8888; }
        input { width: 100%; padding: 10px; background: #1A0000; border: 1px solid #550000; color: #FF3333; box-sizing: border-box; }
        button { width: 100%; padding: 12px; margin-top: 25px; background: #FF0000; color: #000; font-weight: bold; cursor: pointer; }
    </style>
</head>
<body>
    <div class="box">
        <h1>[ FILESYSTEM FAULT ]</h1>
        <p>LittleFS partition failed to mount. Local safety fallback active.</p>
        <form action="/save-basic" method="POST">
            <label>CALLSIGN:</label> <input type="text" name="callsign" required>
            <label>GRID SQUARE:</label> <input type="text" name="grid" required>
            <label>UTC OFFSET (Hours):</label> <input type="number" name="offset" step="0.5" value="5.5" required>
            <label>WIFI SSID:</label> <input type="text" name="ssid" required>
            <label>WIFI PASSWORD:</label> <input type="password" name="pass" required>
            <button type="submit">EMERGENCY CORE FLASHDUMP</button>
        </form>
    </div>
</body>
</html>
)rawhtml";

void web_server_init() {
    // Called from setup() and again whenever the setup AP comes up; register routes only once.
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    WiFi.setSleep(false);
    if (LittleFS.begin()) {
        if (!LittleFS.exists("/profiles")) LittleFS.mkdir("/profiles");
    }

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        REQUIRE_AUTH(request);
        if (LittleFS.exists("/www/index.html")) request->send(LittleFS, "/www/index.html", "text/html");
        else request->send(200, "text/html", fallback_html);
    });

    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        REQUIRE_AUTH(request);
        if (LittleFS.exists("/www/style.css")) request->send(LittleFS, "/www/style.css", "text/css");
        else request->send(404, "text/plain", "CSS Missing");
    });

    server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        REQUIRE_AUTH(request);
        if (LittleFS.exists("/www/app.js")) request->send(LittleFS, "/www/app.js", "application/javascript");
        else request->send(404, "text/plain", "JS Missing");
    });

    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        REQUIRE_AUTH(request);
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        JsonDocument doc;
        doc["uptime"] = millis() / 1000;
        doc["heap"] = ESP.getFreeHeap();
        doc["rssi"] = WiFi.isConnected() ? WiFi.RSSI() : 0;
        doc["ip"] = WiFi.localIP().toString();
        doc["temp"] = sensor_get_temp();
        doc["humidity"] = sensor_get_humidity();
        doc["pressure"] = sensor_get_pressure();
        doc["fw_name"] = meta::FW_NAME;
        doc["fw_version"] = meta::FW_VERSION;
        doc["author_call"] = meta::AUTHOR_CALL;
        char lvgl_ver[16];
        snprintf(lvgl_ver, sizeof(lvgl_ver), "v%d.%d.%d", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
        doc["ver_lvgl"] = lvgl_ver;
        doc["ver_json"] = "v" ARDUINOJSON_VERSION;
        doc["ver_core"] = "v" + String(ESP_ARDUINO_VERSION_MAJOR) + "." + String(ESP_ARDUINO_VERSION_MINOR) + "." + String(ESP_ARDUINO_VERSION_PATCH);
        doc["ver_idf"]  = String(esp_get_idf_version());
        serializeJson(doc, *response);
        request->send(response);
    });

    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        REQUIRE_AUTH(request);
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        JsonDocument doc;
        const auto& c = config::get();
        doc["fw_name"] = meta::FW_NAME;
        doc["fw_version"] = meta::FW_VERSION;
        doc["author_call"] = meta::AUTHOR_CALL;

        doc["callsign"] = c.callsign;
        doc["grid"] = c.grid;
        doc["ssid"] = c.wifi_ssid;
        // Secrets are never returned (review 1.3); the UI shows "saved" and sends "" to keep them.
        doc["password"] = "";      doc["password_set"] = c.wifi_password[0] != '\0';
        doc["apikey"] = "";        doc["apikey_set"] = c.openweather_api_key[0] != '\0';
        doc["lat"] = c.lat;
        doc["lon"] = c.lon;
        doc["offset"] = (float)c.tz_offset_hh / 2.0f;
        doc["brightness"] = c.brightness;
        doc["auto_bright"] = c.auto_brightness;
        doc["theme_id"] = c.theme_id;
        doc["timeout"] = c.screen_timeout_min;
        doc["fc_slots"] = c.forecast_slots;  
        
        doc["dx_url_p"]  = c.dx_url_primary;
        doc["dx_port_p"] = c.dx_port_primary;
        doc["dx_url_s"]  = c.dx_url_secondary;
        doc["dx_port_s"] = c.dx_port_secondary;
        
        doc["aprs_en"]   = c.aprs_enabled;
        doc["aprs_pass"] = "";     doc["aprs_pass_set"] = c.aprs_passcode[0] != '\0';
        doc["aprs_ssid"] = c.aprs_ssid;
        doc["aprs_cmt"]  = c.aprs_comment;
        doc["aprs_icn"]  = c.aprs_icon;

        JsonArray mac_arr = doc["aprs_macros"].to<JsonArray>();
        for(int i=0; i<5; i++) mac_arr.add(c.aprs_macros[i]);

        doc["hamalert_pass"] = ""; doc["hamalert_pass_set"] = c.hamalert_password[0] != '\0';

        serializeJson(doc, *response);
        request->send(response);
    });

    server.on("/api/profiles/get", HTTP_GET, [](AsyncWebServerRequest *request) {
        REQUIRE_AUTH(request);
        if (request->hasParam("name") && config::is_valid_profile_name(request->getParam("name")->value().c_str())) {
            String name = request->getParam("name")->value();
            services::profile_manager::ProfileData p_data;
            if (services::profile_manager::read_profile(name.c_str(), p_data)) {
                AsyncResponseStream *response = request->beginResponseStream("application/json");
                JsonDocument doc;
                doc["callsign"] = p_data.callsign;
                doc["grid"] = p_data.grid;
                doc["ssid"] = p_data.wifi_ssid;
                doc["password"] = "";  doc["password_set"] = p_data.wifi_password[0] != '\0';
                doc["apikey"] = "";    doc["apikey_set"] = p_data.openweather_api_key[0] != '\0';
                doc["lat"] = p_data.lat;
                doc["lon"] = p_data.lon;
                doc["offset"] = (float)p_data.tz_offset_hh / 2.0f;
                doc["brightness"] = p_data.brightness;
                doc["auto_bright"] = config::get().auto_brightness;
                doc["theme_id"] = p_data.theme_id;
                doc["timeout"] = p_data.screen_timeout_min; 

                doc["aprs_en"]   = p_data.aprs_enabled;
                doc["aprs_pass"] = ""; doc["aprs_pass_set"] = p_data.aprs_passcode[0] != '\0';
                doc["aprs_ssid"] = p_data.aprs_ssid;
                doc["aprs_cmt"]  = p_data.aprs_comment;
                doc["aprs_icn"]  = p_data.aprs_icon;

                serializeJson(doc, *response);
                request->send(response);
                return;
            }
        }
        request->send(404, "application/json", "{\"status\":\"not_found\"}");
    });

    server.on("/api/config/save", HTTP_POST, auth_gate, nullptr,
             [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                 if (!authorized(request)) return;
                 JsonDocument doc;
                 DeserializationError err = deserializeJson(doc, data, len);
                 if (!err) {
                     auto* c = new config::Config(config::get());
                     copy_str(c->callsign, doc["callsign"]);
                     copy_str(c->grid, doc["grid"]);
                     copy_str(c->wifi_ssid, doc["ssid"]);
                     copy_secret(c->wifi_password, doc["password"]);
                     copy_secret(c->openweather_api_key, doc["apikey"]);
                     if (doc["lat"].is<float>())        c->lat = doc["lat"].as<float>();
                     if (doc["lon"].is<float>())        c->lon = doc["lon"].as<float>();
                     if (doc["brightness"].is<int>())   c->brightness = (uint8_t)constrain(doc["brightness"].as<int>(), 0, 255);
                     if (doc["auto_bright"].is<bool>()) c->auto_brightness = doc["auto_bright"].as<bool>();
                     if (doc["theme_id"].is<int>())     c->theme_id = (uint8_t)constrain(doc["theme_id"].as<int>(), 0, 255);
                     if (doc["offset"].is<float>())     c->tz_offset_hh = (int8_t)constrain((int)(doc["offset"].as<float>() * 2.0f), -128, 127);
                     if (doc["timeout"].is<int>())      c->screen_timeout_min = (uint8_t)constrain(doc["timeout"].as<int>(), 0, 255);
                     if (doc["fc_slots"].is<int>())     c->forecast_slots = (uint8_t)doc["fc_slots"].as<int>();
                     copy_str(c->dx_url_primary, doc["dx_url_p"]);
                     if (doc["dx_port_p"].is<int>())    c->dx_port_primary = (uint16_t)doc["dx_port_p"].as<int>();
                     copy_str(c->dx_url_secondary, doc["dx_url_s"]);
                     if (doc["dx_port_s"].is<int>())    c->dx_port_secondary = (uint16_t)doc["dx_port_s"].as<int>();
                     if (doc["aprs_en"].is<bool>())     c->aprs_enabled = doc["aprs_en"].as<bool>();
                     copy_secret(c->aprs_passcode, doc["aprs_pass"]);
                     if (doc["aprs_ssid"].is<int>())    c->aprs_ssid = (int8_t)constrain(doc["aprs_ssid"].as<int>(), -1, 99);
                     copy_str(c->aprs_comment, doc["aprs_cmt"]);
                     copy_str(c->aprs_icon, doc["aprs_icn"]);
                     JsonArrayConst mac_arr = doc["aprs_macros"].as<JsonArrayConst>();
                     for (size_t i = 0; i < 5 && i < mac_arr.size(); i++) copy_str(c->aprs_macros[i], mac_arr[i]);
                     copy_secret(c->hamalert_password, doc["hamalert_pass"]);
                     copy_secret(c->admin_password, doc["admin_pw"]);  // sanitize() rejects < 8 chars
                     queue_config(c);
                     request->send(200, "application/json", "{\"status\":\"success\"}");
                 } else {
                     request->send(400, "application/json", "{\"status\":\"malformed\"}");
                 }
             });

    server.on("/api/profiles", HTTP_GET, [](AsyncWebServerRequest *request) {
        REQUIRE_AUTH(request);
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        JsonDocument doc;
        JsonArray array = doc.to<JsonArray>();
        auto list = services::profile_manager::get_profile_list();
        for (const auto& name : list) array.add(name);
        serializeJson(doc, *response);
        request->send(response);
    });

    server.on("/api/profiles/save", HTTP_POST, auth_gate, nullptr,
             [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                 if (!authorized(request)) return;
                 JsonDocument doc;
                 DeserializationError err = deserializeJson(doc, data, len);
                 if (!err && doc["name"].is<const char*>() && config::is_valid_profile_name(doc["name"].as<const char*>()) && !doc["config"].isNull()) {
                     String p_name = doc["name"].as<String>();
                     if (services::profile_manager::save_profile_from_json(p_name.c_str(), doc["config"])) {
                         request->send(200, "application/json", "{\"status\":\"success\"}");
                         return;
                     }
                 }
                 request->send(400, "application/json", "{\"status\":\"failed\"}");
             });

    server.on("/api/profiles/load", HTTP_POST, [](AsyncWebServerRequest *request) {
        REQUIRE_AUTH(request);
        if (request->hasParam("name")) {
            String name = request->getParam("name")->value();
            services::profile_manager::ProfileData probe;
            if (config::is_valid_profile_name(name.c_str()) && services::profile_manager::read_profile(name.c_str(), probe)) {
                strncpy(pending_profile, name.c_str(), sizeof(pending_profile) - 1);
                pending_profile[sizeof(pending_profile) - 1] = '\0';
                pending_profile_flag = true;  // applied on the main loop
                request->send(200, "application/json", "{\"status\":\"success\"}");
                return;
            }
        }
        request->send(404, "application/json", "{\"status\":\"not_found\"}");
    });

    server.on("/api/aprs/send", HTTP_POST, auth_gate, nullptr,
             [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                 if (!authorized(request)) return;
                 JsonDocument doc;
                 DeserializationError err = deserializeJson(doc, data, len);

                 if (!err && doc["target"].is<const char*>() && doc["message"].is<const char*>()) {
                     String target = doc["target"].as<String>();
                     String message = doc["message"].as<String>();
                     services::AprsManager::send_message(target.c_str(), message.c_str(), false);
                     request->send(200, "application/json", "{\"status\":\"success\"}");
                 } else {
                     request->send(400, "application/json", "{\"status\":\"failed\",\"error\":\"Invalid JSON payload\"}");
                 }
             }
    );

    server.on("/api/aprs/messages", HTTP_GET, [](AsyncWebServerRequest *request) {
        REQUIRE_AUTH(request);
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();

        const auto* msgs = services::AprsManager::get_messages();
        size_t count = services::AprsManager::get_message_count();

        for(size_t i = 0; i < count; i++) {
            JsonObject obj = arr.add<JsonObject>();
            obj["from"] = msgs[i].from;
            obj["text"] = msgs[i].text;
        }

        serializeJson(doc, *response);
        request->send(response);
    });

    server.on("/api/about", HTTP_GET, [](AsyncWebServerRequest *request) {
        REQUIRE_AUTH(request);
        if (LittleFS.exists("/about.txt")) request->send(LittleFS, "/about.txt", "text/plain");
        else request->send(200, "text/plain", "QRPickle Tactical Platform.\nNo custom about.txt document found on filesystem.");
    });

    server.on("/save-basic", HTTP_POST, [](AsyncWebServerRequest *request) {
        REQUIRE_AUTH(request);
        auto* c = new config::Config(config::get());
        auto param = [&](const char* k) -> const char* {
            return request->hasParam(k, true) ? request->getParam(k, true)->value().c_str() : nullptr;
        };
        if (const char* v = param("callsign")) { strncpy(c->callsign, v, sizeof(c->callsign) - 1); }
        if (const char* v = param("grid"))     { strncpy(c->grid, v, sizeof(c->grid) - 1); }
        if (const char* v = param("ssid"))     { strncpy(c->wifi_ssid, v, sizeof(c->wifi_ssid) - 1); }
        if (const char* v = param("pass"))     { if (v[0]) strncpy(c->wifi_password, v, sizeof(c->wifi_password) - 1); }
        if (const char* v = param("offset"))   { c->tz_offset_hh = (int8_t)constrain((int)(atof(v) * 2.0f), -128, 127); }
        queue_config(c);
        request->send(200, "text/html", "<h3>Basic Config Committed. Rebooting...</h3>");
        flag_trigger_reboot = true;
        reboot_timer_mark = millis();
    });

    server.on("/api/system/reboot", HTTP_POST, [](AsyncWebServerRequest *request) {
        REQUIRE_AUTH(request);
        request->send(200, "application/json", "{\"status\":\"rebooting\"}");
        flag_trigger_reboot = true;
        reboot_timer_mark = millis();
    });

    server.on("/api/system/update", HTTP_POST,  
        [](AsyncWebServerRequest *request) {
            REQUIRE_AUTH(request);
            bool failed = Update.hasError();
            if (!failed) {
                request->send(200, "application/json", "{\"status\":\"success\"}");
                flag_trigger_reboot = true;
                reboot_timer_mark = millis();
            } else {
                char err_json[128];
                snprintf(err_json, sizeof(err_json), "{\"status\":\"failed\",\"error\":\"%s\"}", services::ota_manager::get_error_string());
                request->send(400, "application/json", err_json);
            }
        },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            if (!authorized(request)) return;  // never write flash for an unauthenticated upload
            if (index == 0) {
                services::ota_manager::UpdateType type = services::ota_manager::UPDATE_TYPE_UNKNOWN;
                if (request->hasParam("target")) {
                    String target = request->getParam("target")->value();
                    if (target == "firmware") type = services::ota_manager::UPDATE_TYPE_FIRMWARE;
                    else if (target == "filesystem") type = services::ota_manager::UPDATE_TYPE_FILESYSTEM;
                }
                if (!services::ota_manager::begin(type)) {
                    request->send(400, "application/json", "{\"status\":\"failed\",\"error\":\"INIT_FAILED\"}");
                    return;
                }
            }
            if (len > 0) {
                if (!services::ota_manager::write_chunk(data, len)) {
                    services::ota_manager::abort();
                    return;
                }
            }
            if (final) {
                if (!services::ota_manager::end()) Serial.println("[OTA CRITICAL] Error validating terminal binary block.");
            }
        }
    );

    server.on("/api/cloud_ota/check", HTTP_GET, [](AsyncWebServerRequest *request) {
        REQUIRE_AUTH(request);
        if (request->hasParam("force") && request->getParam("force")->value() == "true") {
            services::cloud_ota::force_update_check();
        }

        AsyncResponseStream *response = request->beginResponseStream("application/json");
        JsonDocument doc;

        auto info = services::cloud_ota::get_release_info();
        doc["available"] = info.update_available;
        doc["latest_ver"] = info.latest_version;
        doc["notes"] = info.release_notes;
        doc["local_ver"] = meta::FW_VERSION;

        serializeJson(doc, *response);
        request->send(response);
    });

    server.on("/api/cloud_ota/flash", HTTP_POST, [](AsyncWebServerRequest *request) {
        REQUIRE_AUTH(request);
        // Just send success back to the browser and flip the execution flag.
        // We MUST exit this async callback context before trying to kill the server!
        request->send(200, "application/json", "{\"status\":\"flashing\"}");
        flag_trigger_ota_flash = true; 
    });

    // Defence in depth for review 1.1: no remote scripts, no requests to other hosts.
    DefaultHeaders::Instance().addHeader("Content-Security-Policy",
        "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; "
        "img-src 'self' data:; connect-src 'self'; form-action 'self'; frame-ancestors 'none'");
    server.begin();
}

void web_server_stop() { server.end(); }

void web_server_update() {
    if (config::Config* staged = pending_config.exchange(nullptr)) {
        config::mutable_get() = *staged;
        delete staged;
        config::save();
        flag_trigger_ui_refresh = true;
    }
    if (pending_profile_flag.exchange(false)) {
        if (services::profile_manager::apply_profile_to_live(pending_profile)) flag_trigger_ui_refresh = true;
    }
    if (flag_trigger_ui_refresh) {
        flag_trigger_ui_refresh = false;
        ui::status_bar_refresh_theme();
        services::display_manager::set_brightness(config::get().brightness);  
    }
    
    if (flag_trigger_reboot && (millis() - reboot_timer_mark > 1500)) {
        ESP.restart();
    }

    // THE INFINITE STASIS TRAP
    // This executes safely inside the main core loop.
    if (flag_trigger_ota_flash) {
        flag_trigger_ota_flash = false;
        
        Serial.println("[SYSTEM-LOCKDOWN] Main loop intercepted! Securing network state...");
        
        // 1. Destroy the background task stacks (recovers ~15KB RAM)
        services::DxManager::stop();
        services::HamAlertManager::stop();
        services::AprsManager::stop();
        delay(1000); 

        // 2. Shut down the web server safely from the main thread
        Serial.println("[SYSTEM-LOCKDOWN] Terminating asynchronous web server listener...");
        web_server_stop(); 
        delay(1500); // Allow LwIP to finalize socket closure
        
        // 3. Dispatch the OTA worker
        Serial.println("[SYSTEM-LOCKDOWN] All background activity halted. Commencing OTA flash...");
        services::cloud_ota::execute_firmware_flash();
        
        // 4. Trap the main loop forever!
        // This physically prevents weather_manager, timekeeper, or UI from waking up
        // and opening new HTTPS streams while the OTA worker finishes.
        Serial.println("[SYSTEM-LOCKDOWN] Device entering Stasis. Awaiting auto-reboot...");
        while (true) {
            delay(100); // Feed the watchdog timer safely
        }
    }
}