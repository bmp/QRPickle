#pragma once
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include "net_connect.h"

// Drop-in replacements for WiFiClient / WiFiClientSecure whose connect(hostname, ...) resolves
// with services::resolve_host() (getaddrinfo, thread-safe) and then connects by IP.
//
// Why: on Arduino-ESP32 2.0.17 the stock clients resolve with WiFi.hostByName(), which calls
// lwIP's dns_gethostbyname() from the caller's task without the lwIP core lock. That races the
// DNS timer in the lwIP thread and corrupts its table: on 2026-10-10 opening xOTA (POTA fetch over
// HTTPClient) crashed in dns_tmr -> dns_send -> udp_sendto (LoadProhibited, address 0x15), and the
// same corruption left the network dead ("connected", no ping) without crashing. Review 3.14 fixed
// this for the raw telnet clients with connect_host(); HTTPClient goes through these overrides.
// TLS still uses the hostname for SNI and certificate verification.
namespace services {

    class SafeClient : public WiFiClient {
    public:
        using WiFiClient::connect;
        int connect(const char* host, uint16_t port) override { return connect(host, port, 30000); }
        int connect(const char* host, uint16_t port, int32_t timeout_ms) override {
            IPAddress ip;
            return resolve_host(host, ip) ? WiFiClient::connect(ip, port, timeout_ms) : 0;
        }
    };

    class SafeTlsClient : public WiFiClientSecure {
    public:
        using WiFiClientSecure::connect;
        int connect(const char* host, uint16_t port) override {
            IPAddress ip;
            if (!resolve_host(host, ip)) return 0;
            // Same as WiFiClientSecure::connect(host, port), minus the unsafe hostByName().
            return WiFiClientSecure::connect(ip, port, host, _CA_cert, _cert, _private_key);
        }
        int connect(const char* host, uint16_t port, int32_t timeout_ms) override {
            _timeout = timeout_ms;
            return connect(host, port);
        }
    };

}  // namespace services
