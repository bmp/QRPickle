#include "aprs_parse.h"
#include "net_connect.h"
#include "../core/crashlog.h"
#include "aprs_manager.h"
#include "../config/config.h"
#include <atomic>
#include "../core/metadata.h" // Pulls dynamic version
#include "../hw/sensor.h"
#include "../hw/led_rgb.h" 
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <math.h>
#include <strings.h>
#include "prop_manager.h"

namespace services {

    AprsStation* AprsManager::stations = nullptr;
    size_t AprsManager::station_count = 0;
    AprsMessage* AprsManager::messages = nullptr;
    size_t AprsManager::message_count = 0;
    
    bool AprsManager::connected = false;
    bool AprsManager::dirty = false;
    bool AprsManager::msg_dirty = false;
    bool AprsManager::running = false;

    // Set before the task is created, cleared by the task as its last action.
    static std::atomic<bool> task_alive{false};
    
    uint32_t AprsManager::tx_count = 0;
    uint32_t AprsManager::last_tx_time = 0;
    static uint32_t loop_last_beacon_millis = 0;

    // Outgoing packets (messages and acks) from any task; drained by the APRS task (review 3.7).
    static QueueHandle_t tx_queue = nullptr;
    static const size_t TX_LEN = 160;

    void AprsManager::start() {
        if (running || task_alive || !config::get().aprs_enabled) return;  // previous task may still be exiting
        
        if (!stations) stations = (AprsStation*)calloc(30, sizeof(AprsStation));
        if (!messages) messages = (AprsMessage*)calloc(10, sizeof(AprsMessage));
        if (!tx_queue) tx_queue = xQueueCreate(4, TX_LEN);
        
        if (!stations || !messages || !tx_queue) return;

        running = true;
        task_alive = true;
        if (xTaskCreate(task_loop, "aprs_task", 10240, NULL, 1, NULL) != pdPASS) {
            running = false;
            task_alive = false;
            Serial.println("[APRS] Task creation failed (heap).");
        }
    }

    void AprsManager::stop() {
        running = false;
        connected = false;
    }

    bool AprsManager::is_stopped() { return !task_alive; }

    const AprsStation* AprsManager::get_stations() { return stations; }
    size_t AprsManager::get_station_count() { return station_count; }
    bool AprsManager::is_connected() { return connected; }
    bool AprsManager::is_dirty() { return dirty; }
    void AprsManager::clear_dirty() { dirty = false; }

    const AprsMessage* AprsManager::get_messages() { return messages; }
    size_t AprsManager::get_message_count() { return message_count; }
    bool AprsManager::is_msg_dirty() { return msg_dirty; }
    void AprsManager::clear_msg_dirty() { msg_dirty = false; }

    void AprsManager::trigger_manual_beacon() {
        loop_last_beacon_millis = 0;  
    }

    void AprsManager::send_message(const char* target, const char* message, bool silent) {
        auto& cfg = config::get();
        if (!connected || !tx_queue) return;
        if (strlen(target) == 0 || strlen(message) == 0) return;

        char src_call[20];
        if (cfg.aprs_ssid == 0) snprintf(src_call, sizeof(src_call), "%s", cfg.callsign);
        else snprintf(src_call, sizeof(src_call), "%s-%d", cfg.callsign, cfg.aprs_ssid);
        for (int i = 0; src_call[i]; i++) src_call[i] = toupper(src_call[i]);

        char padded_target[10] = "         ";
        for (size_t i = 0; i < 9 && target[i] != '\0'; i++) {
            padded_target[i] = toupper(target[i]);
        }

        char packet[TX_LEN];
        snprintf(packet, sizeof(packet), "%s>APRS,TCPIP*::%s:%s\r\n", src_call, padded_target, message);
        if (xQueueSend(tx_queue, packet, 0) != pdTRUE) {
            Serial.println("[APRS-TX] Queue full; message dropped.");
            return;
        }
        Serial.printf("[APRS-TX] Queued for network stream: %s", packet);

        if (!silent) {
            if (message_count < 10) {
                strncpy(messages[message_count].from, target, 15);
                snprintf(messages[message_count].text, 63, "[TX] %s", message);
                message_count++;
            } else {
                for(int i=0; i<9; i++) messages[i] = messages[i+1];
                strncpy(messages[9].from, target, 15);
                snprintf(messages[9].text, 63, "[TX] %s", message);
            }
            msg_dirty = true;
        }
    }

    void AprsManager::sort_stations(bool ascending) {
        if (station_count < 2 || !stations) return;
        
        for (size_t i = 0; i < station_count - 1; i++) {
            for (size_t j = 0; j < station_count - i - 1; j++) {
                bool swap_needed = ascending ?  
                    (stations[j].distance_km > stations[j+1].distance_km) :  
                    (stations[j].distance_km < stations[j+1].distance_km);
                
                if (swap_needed) {
                    AprsStation temp = stations[j];
                    stations[j] = stations[j+1];
                    stations[j+1] = temp;
                }
            }
        }
        dirty = true;
    }

    void AprsManager::calc_distance_bearing(float lat1, float lon1, float lat2, float lon2, float& out_dist, int& out_bearing) {
        float R = 6371.0f;  
        float dLat = (lat2 - lat1) * PI / 180.0f;
        float dLon = (lon2 - lon1) * PI / 180.0f;
        float rLat1 = lat1 * PI / 180.0f;
        float rLat2 = lat2 * PI / 180.0f;

        float a = sin(dLat/2) * sin(dLat/2) + cos(rLat1) * cos(rLat2) * sin(dLon/2) * sin(dLon/2);
        float c = 2 * atan2(sqrt(a), sqrt(1-a));
        out_dist = R * c;

        float y = sin(dLon) * cos(rLat2);
        float x = cos(rLat1) * sin(rLat2) - sin(rLat1) * cos(rLat2) * cos(dLon);
        float brg = atan2(y, x) * 180.0f / PI;
        out_bearing = ((int)brg + 360) % 360;
    }

    void AprsManager::get_cardinal(int bearing, char* out_str) {
        const char* dirs[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
        strcpy(out_str, dirs[((bearing + 11) % 360) / 22]);
    }

    void AprsManager::convert_decimal_to_aprs(float lat, float lon, char* out_str, char& table, char& symbol) {
        auto& cfg = config::get();
        table = (strlen(cfg.aprs_icon) >= 2) ? cfg.aprs_icon[0] : '/';
        symbol = (strlen(cfg.aprs_icon) >= 2) ? cfg.aprs_icon[1] : '[';

        char lat_dir = (lat >= 0) ? 'N' : 'S';
        char lon_dir = (lon >= 0) ? 'E' : 'W';
        lat = fabs(lat); lon = fabs(lon);

        // Whole hundredths of a minute, so 59.996' carries into the degrees instead of printing an
        // invalid "60.00". Config validation keeps lat/lon within +/-90 / +/-180.
        long lat_h = lroundf(fminf(lat, 90.0f) * 6000.0f);
        long lon_h = lroundf(fminf(lon, 180.0f) * 6000.0f);
        int n = snprintf(out_str, 24, "%02ld%02ld.%02ld%c%c%03ld%02ld.%02ld%c%c",
                         lat_h / 6000, (lat_h % 6000) / 100, lat_h % 100, lat_dir, table,
                         lon_h / 6000, (lon_h % 6000) / 100, lon_h % 100, lon_dir, symbol);
        if (n < 0 || n >= 24) out_str[0] = '\0';  // cannot happen for valid coordinates
    }

    void AprsManager::get_current_payload(char* buf, size_t max_len) {
        auto& cfg = config::get();
        float t = sensor_get_temp();
        float h = sensor_get_humidity();
        float p = sensor_get_pressure();
        
        snprintf(buf, max_len, "%s | T:%.1fC H:%.0f%% P:%.0fhPa", cfg.aprs_comment, t, h, p);
    }

    void AprsManager::update_or_add_station(const char* call, float lat, float lon, const char* table_char, const char* symbol_char, const char* cmt) {
        auto& cfg = config::get();
        float dist; int brg;
        calc_distance_bearing(cfg.lat, cfg.lon, lat, lon, dist, brg);

        int target_idx = -1;
        for (size_t i = 0; i < station_count; i++) {
            if (strcmp(stations[i].callsign, call) == 0) {
                target_idx = i;
                break;
            }
        }

        if (target_idx == -1) {
            if (station_count < 30) {
                target_idx = station_count++;
            } else {
                uint32_t oldest = 0xFFFFFFFF;
                for (size_t i = 0; i < 30; i++) {
                    if (stations[i].last_seen_millis < oldest) {
                        oldest = stations[i].last_seen_millis;
                        target_idx = i;
                    }
                }
            }
        }

        if (target_idx != -1) {
            strncpy(stations[target_idx].callsign, call, sizeof(stations[0].callsign)-1);
            stations[target_idx].lat = lat;
            stations[target_idx].lon = lon;
            stations[target_idx].distance_km = dist;
            stations[target_idx].bearing_deg = brg;
            stations[target_idx].last_seen_millis = millis();
            
            if (symbol_char[0] == '[' && table_char[0] == '/')     strncpy(stations[target_idx].type, "Runner", 15);
            else if (symbol_char[0] == '-' && table_char[0] == '/') strncpy(stations[target_idx].type, "Fixed Base", 15);
            else if (symbol_char[0] == '&' && table_char[0] == 'I') strncpy(stations[target_idx].type, "Gateway", 15);
            else if (symbol_char[0] == 'Y' && table_char[0] == '\\') strncpy(stations[target_idx].type, "Radios/APRS", 15);
            else if (symbol_char[0] == ';' && table_char[0] == '/')  strncpy(stations[target_idx].type, "Portable", 15);
            else if (symbol_char[0] == 'F' && table_char[0] == '\\') strncpy(stations[target_idx].type, "Field Day", 15);
            else if (symbol_char[0] == ';' && table_char[0] == '\\') strncpy(stations[target_idx].type, "Park/Picnic", 15);
            else strncpy(stations[target_idx].type, "Station", 15);

            if (cmt) {
                strncpy(stations[target_idx].comment, cmt, sizeof(stations[0].comment)-1);
                stations[target_idx].comment[sizeof(stations[0].comment)-1] = '\0';
                char* end = strchr(stations[target_idx].comment, '\r'); if(end) *end = '\0';
                end = strchr(stations[target_idx].comment, '\n'); if(end) *end = '\0';
            }
            dirty = true;
            hw::led_rgb::trigger_traffic_pulse();  // data ingress (docs/LEDColours.md)
        }
    }

    void AprsManager::parse_uncompressed_position(const char* call, const char* info) {
        float lat, lon;
        char table, symbol;
        if (!aprs::parse_uncompressed_latlon(info, lat, lon, table, symbol)) return;  // compressed/garbage
        char table_char[2] = {table, '\0'};
        char symbol_char[2] = {symbol, '\0'};
        update_or_add_station(call, lat, lon, table_char, symbol_char, info + 20);
    }

    void AprsManager::parse_incoming_message(const char* call, const char* info) {
        auto& cfg = config::get();
        if (strlen(info) < 11 || info[0] != ':') return;

        char rx_target[10];
        memcpy(rx_target, info + 1, 9);
        rx_target[9] = '\0';
        for (int i = 8; i >= 0; i--) {
            if (rx_target[i] == ' ') rx_target[i] = '\0';
            else break;
        }

        if (!aprs::addressed_to(rx_target, cfg.callsign)) return;

        if (info[10] == ':') {
            char msg_body[64];
            strncpy(msg_body, info + 11, 63);
            msg_body[63] = '\0';

            char* ack_start = strrchr(msg_body, '{');
            if (ack_start) {
                *ack_start = '\0'; 
                char ack_id[10];
                strncpy(ack_id, ack_start + 1, 9);
                ack_id[9] = '\0';

                char ack_payload[16];
                snprintf(ack_payload, sizeof(ack_payload), "ack%s", ack_id);
                send_message(call, ack_payload, true);
            }

            if (strncasecmp(msg_body, "ack", 3) == 0) return;

            if (message_count < 10) {
                strncpy(messages[message_count].from, call, 15);
                strncpy(messages[message_count].text, msg_body, 63);
                message_count++;
            } else {
                for(int i=0; i<9; i++) messages[i] = messages[i+1];
                strncpy(messages[9].from, call, 15);
                strncpy(messages[9].text, msg_body, 63);
            }
            msg_dirty = true;

            hw::led_rgb::trigger_priority_strobe();
        }
    }

    void AprsManager::process_line(char* line) {
        services::PropagationManager::parse_cluster_line(line);

        char* colon = strchr(line, ':');
        if (!colon) return;
        
        *colon = '\0';  
        char* header = line;
        char* info = colon + 1;
        
        char* call_end = strchr(header, '>');
        if (!call_end) return;
        *call_end = '\0';
        char* callsign = header;

        if (info[0] == ':') {
            parse_incoming_message(callsign, info);
        } else if (info[0] == '!' || info[0] == '=' || info[0] == '@' || info[0] == '/') {
            const char* coord_start = info;
            if (info[0] == '@' || info[0] == '/') {
                if (strlen(info) > 8) coord_start = info + 7;  
            }
            parse_uncompressed_position(callsign, coord_start);
        }
    }

    void AprsManager::task_loop(void* param) {
        WiFiClient client;
        char buffer[256];
        size_t buf_idx = 0;

        while (running) {
            auto& cfg = config::get();
            if (!WiFi.isConnected() || strlen(cfg.aprs_passcode) == 0) {
                vTaskDelay(pdMS_TO_TICKS(4000));
                continue;
            }

            if (!client.connected()) {
                connected = false;
                crashlog::mark(crashlog::SLOT_APRS, 2);
                if (connect_host(client, "rotate.aprs.net", 14580, 3000, crashlog::SLOT_APRS)) {
                    char login[128];
                    char src_call[20];
                    if (cfg.aprs_ssid == 0) snprintf(src_call, sizeof(src_call), "%s", cfg.callsign);
                    else snprintf(src_call, sizeof(src_call), "%s-%d", cfg.callsign, cfg.aprs_ssid);

                    // DYNAMIC VERSION INSERTION
                    snprintf(login, sizeof(login), "user %s pass %s vers %s %s filter r/%.2f/%.2f/50 p/%s\r\n",
                             src_call, cfg.aprs_passcode, meta::FW_NAME, meta::FW_VERSION, cfg.lat, cfg.lon, cfg.callsign);
                             
                    crashlog::mark(crashlog::SLOT_APRS, 3); client.print(login);
                    connected = true;
                    loop_last_beacon_millis = 0;  
                } else {
                    vTaskDelay(pdMS_TO_TICKS(5000));
                    continue;
                }
            }

            if (millis() - loop_last_beacon_millis > (30 * 60 * 1000) || loop_last_beacon_millis == 0) {
                char coord_str[24]; char table_c; char symbol_c;
                convert_decimal_to_aprs(cfg.lat, cfg.lon, coord_str, table_c, symbol_c);
                
                char dynamic_cmt[70];
                get_current_payload(dynamic_cmt, sizeof(dynamic_cmt));

                char src_call[20];
                if (cfg.aprs_ssid == 0) snprintf(src_call, sizeof(src_call), "%s", cfg.callsign);
                else snprintf(src_call, sizeof(src_call), "%s-%d", cfg.callsign, cfg.aprs_ssid);

                char beacon[160];
                snprintf(beacon, sizeof(beacon), "%s>APRS,TCPIP*:=%s%s\r\n",  
                         src_call, coord_str, dynamic_cmt);
                
                crashlog::mark(crashlog::SLOT_APRS, 4); client.print(beacon);
                tx_count++;
                last_tx_time = millis();
                loop_last_beacon_millis = millis();
                if (loop_last_beacon_millis == 0) loop_last_beacon_millis = 1;
            }

            char out[TX_LEN];
            while (connected && xQueueReceive(tx_queue, out, 0) == pdTRUE) {
                client.print(out);
                client.flush();
                Serial.printf("[APRS-TX] Sent: %s", out);
            }

            while (client.available() && running) {
                char c = client.read();
                if (c == '\n') {
                    buffer[buf_idx] = '\0';
                    if (buf_idx > 0 && buffer[buf_idx - 1] == '\r') buffer[buf_idx - 1] = '\0';
                    if (buffer[0] != '#') { crashlog::mark(crashlog::SLOT_APRS, 7); process_line(buffer); }
                    buf_idx = 0;
                } else if (buf_idx < sizeof(buffer) - 1) {
                    buffer[buf_idx++] = c;
                }
            }
            crashlog::mark(crashlog::SLOT_APRS, 8); vTaskDelay(pdMS_TO_TICKS(30));
        }
        client.stop();
        connected = false;
        task_alive = false;
        vTaskDelete(NULL);
    }
} // namespace services