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

}  // namespace services
