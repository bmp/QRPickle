#include "setup_serial.h"
#include "setup_serial_proto.h"
#include "net_lock.h"
#include "web_server.h"
#include "../config/config.h"
#include "../config/config_json.h"
#include "../config/config_validation.h"
#include "../core/metadata.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <string.h>

namespace services {

    namespace {
        setup_proto::Proto proto;
        char line[200];  // the longest protocol line is "QRPSETUP DATA <seq> " + 160 B
        size_t line_len = 0;
        bool overlong = false;  // dropping the rest of a too-long line

        // Only these settings come over USB (the rest stays in the web console).
        const char* const KEYS[] = {"callsign", "grid", "lat", "lon", "latlon_set", "offset", "clock_12h",
                                    "unit_temp", "unit_pres", "unit_wind", "unit_dist", "ssid", "password",
                                    "apikey", "hamalert_pass", "aprs_pass"};

        void append_key(char* list, size_t len, const char* key) {
            if (strlen(list) + strlen(key) + 2 > len) return;
            if (*list) strcat(list, ",");
            strcat(list, key);
        }

        // Applies the JSON on the main loop through the web console's queue; replies with the
        // keys that changed (SAVED), the ones sanitize() put back (KEPT) and the admin password.
        void apply(const char* json, size_t len) {
            if (net_lock_busy()) {  // a download holds the network: TLS needs the heap (page retries)
                Serial.println("QRPSETUP ERR busy");
                return;
            }
            JsonDocument in;
            if (deserializeJson(in, json, len) || !in.is<JsonObject>()) {
                Serial.println("QRPSETUP ERR json");
                return;
            }
            JsonDocument allowed;
            for (const char* k : KEYS)
                if (!in[k].isNull()) allowed[k] = in[k];
            in.clear();

            const config::Config& live = config::get();
            auto* staged = new config::Config(live);
            config::from_json(*staged, allowed.as<JsonObjectConst>());
            config::sanitize(*staged, live);

            JsonDocument before, after;
            config::to_json(live, before.to<JsonObject>(), config::Secrets::Include);
            config::to_json(*staged, after.to<JsonObject>(), config::Secrets::Include);
            char saved[200] = "", kept[200] = "";
            int n_saved = 0;
            for (JsonPairConst kv : allowed.as<JsonObjectConst>()) {
                const char* k = kv.key().c_str();
                if (before[k] != after[k]) {
                    append_key(saved, sizeof(saved), k);
                    n_saved++;
                } else if (kv.value() != before[k] && !(kv.value().is<const char*>() && !*kv.value().as<const char*>())) {
                    append_key(kept, sizeof(kept), k);  // sent, but sanitize() kept the old value
                }
            }
            before.clear();
            after.clear();
            const bool restart = config::wifi_changed(live, *staged);
            web_server_stage_config(staged);  // applied and saved by web_server_update() this loop

            Serial.printf("[Setup] USB setup: saved %d fields\n", n_saved);
            Serial.printf("QRPSETUP SAVED %s KEPT %s ADMIN %s\n", *saved ? saved : "-", *kept ? kept : "-",
                          config::get().admin_password);
            if (restart) Serial.println("QRPSETUP RESTART");
        }

        void handle(const setup_proto::Result& r) {
            if (r.act == setup_proto::Act::Reply) {
                if (strcmp(r.reply, "QRPSETUP READY") == 0) Serial.printf("QRPSETUP READY %s\n", meta::FW_VERSION);
                else Serial.println(r.reply);
            } else if (r.act == setup_proto::Act::Apply) {
                apply(r.json, r.json_len);
                proto.done();
            }
        }
    }  // namespace

    void setup_serial_update() {
        handle(proto.tick(millis()));
        while (Serial.available()) {
            const char c = (char)Serial.read();
            if (c == '\r') continue;
            if (c == '\n') {
                if (!overlong) {
                    line[line_len] = '\0';
                    handle(proto.line(line, millis()));
                }
                line_len = 0;
                overlong = false;
            } else if (line_len < sizeof(line) - 1) {
                line[line_len++] = c;
            } else {
                overlong = true;  // not ours, or broken: dropped up to the next newline
            }
        }
    }

}  // namespace services
