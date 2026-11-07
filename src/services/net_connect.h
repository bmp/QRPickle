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

    // Thread-safe DNS (getaddrinfo, inside the lwIP thread). Never use WiFi.hostByName() or a
    // client's connect(hostname, ...) from a task: see SafeClient / SafeTlsClient (safe_client.h).
    bool resolve_host(const char* host, IPAddress& out);
    // All of `host`'s IPv4 addresses (up to `max`, MAX_HOST_ADDRS in addr_try.h), in DNS order; 0 when
    // the lookup failed. Callers try them in turn (connect_any()): one address of a host can be
    // unreachable from a network while the others answer.
    int resolve_host_all(const char* host, IPAddress* out, int max);
    // Logs that `ip` of `host` didn't answer and the next one is tried.
    void log_next_address(const char* host, const IPAddress& failed, const IPAddress& next);
    // SO_KEEPALIVE with a ~90 s dead-peer timeout (connect_host() calls it).
    void enable_keepalive(WiFiClient& client);

}  // namespace services
