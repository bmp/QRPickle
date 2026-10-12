#pragma once
#include <stdint.h>

namespace services {

    // One network session at a time: DNS + connect, or a whole HTTP(S) request.
    // - Serialises TLS handshakes, which each need ~40 KB of contiguous heap (review 3.4).
    // - Serialises HTTPClient's internal WiFi.hostByName(), which is not thread-safe on
    //   Arduino-ESP32 2.0.17 (review 3.14 residual).
    // Recursive, so a holder may call connect_host(). Check held() before using the network.
    class NetLock {
    public:
        explicit NetLock(uint32_t timeout_ms = 20000);
        ~NetLock();
        bool held() const { return held_; }
        NetLock(const NetLock&) = delete;
        NetLock& operator=(const NetLock&) = delete;
    private:
        bool held_;
    };

    // TLS needs ~40 KB in total from the byte-addressable heap, its biggest piece a ~17 KB receive
    // buffer (docs/MEMORY.md). Call after taking NetLock (and the quiet window): waits up to
    // wait_ms for that much to be free and returns false, with a log line naming `who`, when it
    // isn't, so the caller postpones instead of failing inside mbedTLS.
    constexpr uint32_t TLS_MIN_FREE = 40000;
    constexpr uint32_t TLS_MIN_BLOCK = 18000;
    bool tls_heap_ready(const char* who, uint32_t wait_ms = 5000);

}  // namespace services
