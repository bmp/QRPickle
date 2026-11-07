#pragma once
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include "net_connect.h"
#include "addr_try.h"

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
            IPAddress ips[MAX_HOST_ADDRS];
            const int n = resolve_host_all(host, ips, MAX_HOST_ADDRS);
            return connect_any(n, [&](int i) {
                       if (WiFiClient::connect(ips[i], port, timeout_ms)) return true;
                       WiFiClient::stop();
                       if (i + 1 < n) log_next_address(host, ips[i], ips[i + 1]);
                       return false;
                   }) >= 0;
        }
    };

    class SafeTlsClient : public WiFiClientSecure {
    public:
        IPAddress last_ip;  // the address the last connect() resolved (0.0.0.0: DNS failed); for logs
        using WiFiClientSecure::connect;
        int connect(const char* host, uint16_t port) override {
            last_ip = IPAddress();
            IPAddress ips[MAX_HOST_ADDRS];
            const int n = resolve_host_all(host, ips, MAX_HOST_ADDRS);
            // Same as WiFiClientSecure::connect(host, port), minus the unsafe hostByName(), and trying
            // each address in turn (addr_try.h).
            return connect_any(n, [&](int i) {
                       last_ip = ips[i];
                       if (WiFiClientSecure::connect(ips[i], port, host, _CA_cert, _cert, _private_key)) return true;
                       WiFiClientSecure::stop();
                       if (i + 1 < n) log_next_address(host, ips[i], ips[i + 1]);
                       return false;
                   }) >= 0;
        }
        int connect(const char* host, uint16_t port, int32_t timeout_ms) override {
            _timeout = timeout_ms;
            return connect(host, port);
        }
    };

}  // namespace services
