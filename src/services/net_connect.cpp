#include "net_lock.h"
#include "net_connect.h"
#include "addr_try.h"
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
        IPAddress ips[MAX_HOST_ADDRS];
        const int n = resolve_host_all(host, ips, MAX_HOST_ADDRS);
        if (n == 0) {
            crashlog::mark(slot, 100 + (2));
            return false;
        }
        crashlog::mark(slot, 100 + (3));
        const bool ok = connect_any(n, [&](int i) {
                            if (client.connect(ips[i], port, timeout_ms)) return true;
                            client.stop();
                            if (i + 1 < n) log_next_address(host, ips[i], ips[i + 1]);
                            return false;
                        }) >= 0;
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

    int resolve_host_all(const char* host, IPAddress* out, int max) {
        int n = 0;
#ifdef QRP_TEST_BAD_FIRST_ADDR  // test builds only: an unreachable first address proves the fallback
        if (n < max) out[n++] = IPAddress(10, 255, 255, 1);
#endif
        struct addrinfo hints = {};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        struct addrinfo* res = nullptr;
        if (getaddrinfo(host, nullptr, &hints, &res) != 0 || !res) {
            Serial.printf("[Net] DNS failed for %s\n", host);
            return 0;
        }
        for (struct addrinfo* a = res; a && n < max; a = a->ai_next)
            out[n++] = IPAddress(((struct sockaddr_in*)a->ai_addr)->sin_addr.s_addr);
        freeaddrinfo(res);
        return n;
    }

    bool resolve_host(const char* host, IPAddress& out) { return resolve_host_all(host, &out, 1) == 1; }

    void log_next_address(const char* host, const IPAddress& failed, const IPAddress& next) {
        Serial.printf("[Net] %s: %s didn't answer, trying %s\n", host, failed.toString().c_str(), next.toString().c_str());
    }

}  // namespace services
