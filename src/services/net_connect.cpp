#include "net_lock.h"
#include "net_connect.h"
#include <WiFi.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>

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
        IPAddress ip;
        if (!resolve_host(host, ip)) {
            crashlog::mark(slot, 100 + (2));
            return false;
        }
        crashlog::mark(slot, 100 + (3));
        bool ok = client.connect(ip, port, timeout_ms);
        crashlog::mark(slot, 100 + (ok ? 4 : 5));
        if (ok) enable_keepalive(client);
        return ok;
    }

    // TCP keepalive: a peer that vanished (router restart, NAT timeout) is found after ~90 s of
    // silence (60 s idle, then 3 probes 10 s apart) and the socket reports disconnected, so the
    // service reconnects instead of waiting on a dead link (review F2, v0.3.0).
    void enable_keepalive(WiFiClient& client) {
        const int fd = client.fd();
        if (fd < 0) return;
        int on = 1, idle = 60, intvl = 10, cnt = 3;
        setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
        setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
        setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
        setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt));
    }

    bool resolve_host(const char* host, IPAddress& out) {
        struct addrinfo hints = {};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        struct addrinfo* res = nullptr;
        if (getaddrinfo(host, nullptr, &hints, &res) != 0 || !res) {
            Serial.printf("[Net] DNS failed for %s\n", host);
            return false;
        }
        out = IPAddress(((struct sockaddr_in*)res->ai_addr)->sin_addr.s_addr);
        freeaddrinfo(res);
        return true;
    }

}  // namespace services
