#include "web_server.h"
#include "profile_manager.h" 
#include "ota_manager.h" 
#include "display_manager.h"
#include "cloud_ota.h"
#include "aprs_manager.h"
#include "../config/config.h"
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
static bool flag_trigger_reboot = false;
static bool flag_trigger_ui_refresh = false;
static unsigned long reboot_timer_mark = 0;

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
    WiFi.setSleep(false);
    if (LittleFS.begin()) {
        if (!LittleFS.exists("/profiles")) LittleFS.mkdir("/profiles");
    }

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (LittleFS.exists("/www/index.html")) request->send(LittleFS, "/www/index.html", "text/html");
        else request->send(200, "text/html", fallback_html);
    });

    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (LittleFS.exists("/www/style.css")) request->send(LittleFS, "/www/style.css", "text/css");
        else request->send(404, "text/plain", "CSS Missing");
    });

    server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (LittleFS.exists("/www/app.js")) request->send(LittleFS, "/www/app.js", "application/javascript");
        else request->send(404, "text/plain", "JS Missing");
    });

    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
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
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        JsonDocument doc;
        const auto& c = config::get();
        doc["fw_name"] = meta::FW_NAME;
        doc["fw_version"] = meta::FW_VERSION;
        doc["author_call"] = meta::AUTHOR_CALL;

        doc["callsign"] = c.callsign;
        doc["grid"] = c.grid;
        doc["ssid"] = c.wifi_ssid;
        doc["password"] = c.wifi_password;
        doc["apikey"] = c.openweather_api_key;
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
        doc["aprs_pass"] = c.aprs_passcode;
        doc["aprs_ssid"] = c.aprs_ssid;
        doc["aprs_cmt"]  = c.aprs_comment;
        doc["aprs_icn"]  = c.aprs_icon;

        JsonArray mac_arr = doc["aprs_macros"].to<JsonArray>();
        for(int i=0; i<5; i++) mac_arr.add(c.aprs_macros[i]);

        doc["hamalert_pass"] = c.hamalert_password;

        serializeJson(doc, *response);
        request->send(response);
    });

    server.on("/api/profiles/get", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam("name")) {
            String name = request->getParam("name")->value();
            services::profile_manager::ProfileData p_data;
            if (services::profile_manager::read_profile(name.c_str(), p_data)) {
                AsyncResponseStream *response = request->beginResponseStream("application/json");
                JsonDocument doc;
                doc["callsign"] = p_data.callsign;
                doc["grid"] = p_data.grid;
                doc["ssid"] = p_data.wifi_ssid;
                doc["password"] = p_data.wifi_password;
                doc["apikey"] = p_data.openweather_api_key;
                doc["lat"] = p_data.lat;
                doc["lon"] = p_data.lon;
                doc["offset"] = (float)p_data.tz_offset_hh / 2.0f;
                doc["brightness"] = p_data.brightness;
                doc["auto_bright"] = config::get().auto_brightness;
                doc["theme_id"] = p_data.theme_id;
                doc["timeout"] = p_data.screen_timeout_min; 

                doc["aprs_en"]   = p_data.aprs_enabled;
                doc["aprs_pass"] = p_data.aprs_passcode;
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

    server.on("/api/config/save", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr,
              [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                  JsonDocument doc;
                  DeserializationError err = deserializeJson(doc, data, len);
                  if (!err) {
                      auto& c = config::mutable_get(); 
                      if (!doc["callsign"].isNull())   strncpy(c.callsign, doc["callsign"], sizeof(c.callsign)-1);
                      if (!doc["grid"].isNull())       strncpy(c.grid, doc["grid"], sizeof(c.grid)-1);
                      if (!doc["ssid"].isNull())       strncpy(c.wifi_ssid, doc["ssid"], sizeof(c.wifi_ssid)-1);
                      if (!doc["password"].isNull())   strncpy(c.wifi_password, doc["password"], sizeof(c.wifi_password)-1);
                      if (!doc["apikey"].isNull())     strncpy(c.openweather_api_key, doc["apikey"], sizeof(c.openweather_api_key)-1);
                      if (!doc["lat"].isNull())        c.lat = doc["lat"].as<float>(); 
                      if (!doc["lon"].isNull())        c.lon = doc["lon"].as<float>(); 
                      if (!doc["brightness"].isNull()) c.brightness = doc["brightness"].as<uint8_t>();
                      if (!doc["auto_bright"].isNull()) {
                          config::mutable_get().auto_brightness = doc["auto_bright"].as<bool>();
                      }
                      if (!doc["theme_id"].isNull())   c.theme_id = doc["theme_id"].as<uint8_t>();
                      if (!doc["offset"].isNull())     c.tz_offset_hh = (int8_t)(doc["offset"].as<float>() * 2.0f);
                      if (!doc["timeout"].isNull())    c.screen_timeout_min = doc["timeout"].as<uint8_t>(); 
                      if (!doc["fc_slots"].isNull())   c.forecast_slots = doc["fc_slots"].as<uint8_t>(); 
                      
                      if (!doc["dx_url_p"].isNull())  strncpy(c.dx_url_primary, doc["dx_url_p"], sizeof(c.dx_url_primary)-1);
                      if (!doc["dx_port_p"].isNull()) c.dx_port_primary = doc["dx_port_p"].as<uint16_t>();
                      if (!doc["dx_url_s"].isNull())  strncpy(c.dx_url_secondary, doc["dx_url_s"], sizeof(c.dx_url_secondary)-1);
                      if (!doc["dx_port_s"].isNull()) c.dx_port_secondary = doc["dx_port_s"].as<uint16_t>();

                      if (!doc["aprs_en"].isNull())    c.aprs_enabled = doc["aprs_en"].as<bool>();
                      if (!doc["aprs_pass"].isNull())  strncpy(c.aprs_passcode, doc["aprs_pass"], sizeof(c.aprs_passcode)-1);
                      if (!doc["aprs_ssid"].isNull())  c.aprs_ssid = doc["aprs_ssid"].as<int8_t>();
                      if (!doc["aprs_cmt"].isNull())   strncpy(c.aprs_comment, doc["aprs_cmt"], sizeof(c.aprs_comment)-1);
                      if (!doc["aprs_icn"].isNull())   strncpy(c.aprs_icon, doc["aprs_icn"], sizeof(c.aprs_icon)-1);

                      if (!doc["aprs_macros"].isNull()) {
                          JsonArray mac_arr = doc["aprs_macros"].as<JsonArray>();
                          for(int i=0; i<5 && i<mac_arr.size(); i++) {
                              strncpy(c.aprs_macros[i], mac_arr[i].as<const char*>(), 63);
                          }
                      }

                      if (!doc["hamalert_pass"].isNull()) strncpy(c.hamalert_password, doc["hamalert_pass"], sizeof(c.hamalert_password)-1);

                      config::save();
                      flag_trigger_ui_refresh = true;
                      request->send(200, "application/json", "{\"status\":\"success\"}");
                  } else {
                      request->send(400, "application/json", "{\"status\":\"malformed\"}");
                  }
              });

    server.on("/api/profiles", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        JsonDocument doc;
        JsonArray array = doc.to<JsonArray>();
        auto list = services::profile_manager::get_profile_list();
        for (const auto& name : list) array.add(name);
        serializeJson(doc, *response);
        request->send(response);
    });

    server.on("/api/profiles/save", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr,
              [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                  JsonDocument doc;
                  DeserializationError err = deserializeJson(doc, data, len);
                  if (!err && !doc["name"].isNull() && !doc["config"].isNull()) {
                      String p_name = doc["name"].as<String>();
                      if (services::profile_manager::save_profile_from_json(p_name.c_str(), doc["config"])) {
                          request->send(200, "application/json", "{\"status\":\"success\"}");
                          return;
                      }
                  }
                  request->send(400, "application/json", "{\"status\":\"failed\"}");
              });

    server.on("/api/profiles/load", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("name")) {
            String name = request->getParam("name")->value();
            if (services::profile_manager::apply_profile_to_live(name.c_str())) {
                flag_trigger_ui_refresh = true; 
                request->send(200, "application/json", "{\"status\":\"success\"}");
                return;
            }
        }
        request->send(404, "application/json", "{\"status\":\"not_found\"}");
    });

    server.on("/api/aprs/send", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr,
              [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                  JsonDocument doc;
                  DeserializationError err = deserializeJson(doc, data, len);

                  if (!err && !doc["target"].isNull() && !doc["message"].isNull()) {
                      String target = doc["target"].as<String>();
                      String message = doc["message"].as<String>();

                      // Route directly to the backend engine (false = not a silent ACK, so it shows on UI)
                      services::AprsManager::send_message(target.c_str(), message.c_str(), false);

                      request->send(200, "application/json", "{\"status\":\"success\"}");
                  } else {
                      request->send(400, "application/json", "{\"status\":\"failed\",\"error\":\"Invalid JSON payload\"}");
                  }
              }
    );

    server.on("/api/aprs/messages", HTTP_GET, [](AsyncWebServerRequest *request) {
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
        if (LittleFS.exists("/about.txt")) request->send(LittleFS, "/about.txt", "text/plain");
        else request->send(200, "text/plain", "QRPickle Tactical Platform.\nNo custom about.txt document found on filesystem.");
    });

    server.on("/save-basic", HTTP_POST, [](AsyncWebServerRequest *request) {
        auto& c = config::mutable_get();
        if (request->hasParam("callsign", true)) strncpy(c.callsign, request->getParam("callsign", true)->value().c_str(), sizeof(c.callsign)-1);
        if (request->hasParam("grid", true))     strncpy(c.grid, request->getParam("grid", true)->value().c_str(), sizeof(c.grid)-1);
        if (request->hasParam("ssid", true))     strncpy(c.wifi_ssid, request->getParam("ssid", true)->value().c_str(), sizeof(c.wifi_ssid)-1);
        if (request->hasParam("pass", true))     strncpy(c.wifi_password, request->getParam("pass", true)->value().c_str(), sizeof(c.wifi_password)-1);
        if (request->hasParam("offset", true))   c.tz_offset_hh = (int8_t)(request->getParam("offset", true)->value().toFloat() * 2.0f);
        config::save();
        request->send(200, "text/html", "<h3>Basic Config Committed. Rebooting...</h3>");
        flag_trigger_reboot = true;
        reboot_timer_mark = millis();
    });

    server.on("/api/system/reboot", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "{\"status\":\"rebooting\"}");
        flag_trigger_reboot = true;
        reboot_timer_mark = millis();
    });

    server.on("/api/system/update", HTTP_POST, 
        [](AsyncWebServerRequest *request) {
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
        request->send(200, "application/json", "{\"status\":\"flashing\"}");
        services::cloud_ota::execute_firmware_flash();
    });

    server.begin();
}

void web_server_stop() { server.end(); }
void web_server_update() {
    if (flag_trigger_ui_refresh) {
        flag_trigger_ui_refresh = false;
        ui::status_bar_refresh_theme();
        services::display_manager::set_brightness(config::get().brightness); 
    }
    if (flag_trigger_reboot && (millis() - reboot_timer_mark > 1500)) {
        ESP.restart();
    }
}
