#pragma once
#include <WiFiClient.h>
#include "../core/crashlog.h"

namespace services {

    // Resolve `host` explicitly, then connect by IP with a bounded timeout. Use this instead of
    // WiFiClient::connect(hostname, port): on this Arduino-ESP32 core the hostname form was
    // correlated with ~1-in-4 silent interrupt-watchdog resets at boot (review 3.14); the
    // explicit DNS + connect(IP, port, timeout) path showed 0 resets.
    // `slot` receives breadcrumbs 101 (DNS start) .. 105 (connect failed) for crash diagnosis.
    bool connect_host(WiFiClient& client, const char* host, uint16_t port, uint32_t timeout_ms,
                      crashlog::Slot slot);

}  // namespace services
