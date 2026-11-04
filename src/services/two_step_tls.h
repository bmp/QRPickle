#pragma once
#include "safe_client.h"

// HTTPS for Cloud OTA with certificate verification OUTSIDE the TLS handshake (2026-11-04).
//
// Why: verifying the chain inside the handshake needs the two 16 KB TLS buffers, the parsed CA
// bundle and RSA-4096 signature checks at the same time; on the CYD (no PSRAM) that ran the heap down
// and mbedTLS reported "X509 - Certificate verification failed" for almost every update check, while
// the same chain verified 15/15 after the session was closed (docs/TASKS.md, "Release blocker").
//
// How: (1) connect without in-handshake verification, copy the server's certificate chain, close;
// (2) verify the copy against the CA bundle and the hostname, with the TLS buffers freed;
// (3) connect again and accept only the exact certificate verified in (2) (SHA-256 of the DER).
// The handshake proves the server holds that certificate's private key, so this authenticates the
// server as normal verification does. A verified certificate is remembered per host (2 entries) so
// the next connection (redirect target, firmware download) needs one handshake.
// Not thread-safe: Cloud OTA's check and download never run at the same time (cloud_ota.cpp).
namespace services {

    class TwoStepTlsClient : public SafeTlsClient {
    public:
        explicit TwoStepTlsClient(const char* ca_bundle) : ca_bundle_(ca_bundle) { setInsecure(); }
        using SafeTlsClient::connect;
        int connect(const char* host, uint16_t port) override;
        int connect(const char* host, uint16_t port, int32_t timeout_ms) override;
        // Why the last connect failed verification ("" when it didn't).
        const char* verify_error() const { return err_; }

    private:
        bool session_leaf_sha256(uint8_t out[32]);
        bool fetch_and_verify(const char* host, uint16_t port, uint8_t leaf_sha[32]);
        const char* ca_bundle_;
        char err_[160] = "";
    };

}  // namespace services
