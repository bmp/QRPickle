#include "net_lock.h"
#include "net_connect.h"
#include <WiFi.h>
#include <lwip/netdb.h>

namespace services {

    bool connect_host(WiFiClient& client, const char* host, uint16_t port, uint32_t timeout_ms,
                      crashlog::Slot slot) {
        NetLock lock(15000);
        if (!lock.held()) return false;
        // getaddrinfo() runs the lookup inside the lwIP thread. WiFi.hostByName() (Arduino-ESP32
        // 2.0.17) calls dns_gethostbyname() from the caller's task without the lwIP core lock and
        // registers a stack pointer as the callback target, which corrupted lwIP's DNS table when
        // several tasks resolved at once (review 3.14: silent INT_WDT / panic ~5 s later).
        crashlog::mark(slot, 100 + (1));
        struct addrinfo hints = {};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        struct addrinfo* res = nullptr;
        if (getaddrinfo(host, nullptr, &hints, &res) != 0 || !res) {
            crashlog::mark(slot, 100 + (2));
            Serial.printf("[Net] DNS failed for %s\n", host);
            return false;
        }
        IPAddress ip(((struct sockaddr_in*)res->ai_addr)->sin_addr.s_addr);
        freeaddrinfo(res);
        crashlog::mark(slot, 100 + (3));
        bool ok = client.connect(ip, port, timeout_ms);
        crashlog::mark(slot, 100 + (ok ? 4 : 5));
        return ok;
    }

}  // namespace services
