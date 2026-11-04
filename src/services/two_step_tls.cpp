#include "two_step_tls.h"
#include <mbedtls/sha256.h>
#include <mbedtls/version.h>
#include <mbedtls/x509_crt.h>
#include <string.h>

namespace services {

    namespace {
        // Verified leaf certificates by host (the OTA hosts: <owner>.github.io and the Pages domain).
        struct Pin {
            char host[64];
            uint8_t sha[32];
        };
        Pin pins[2] = {};
        int next_pin = 0;

        const Pin* find_pin(const char* host) {
            for (const Pin& p : pins)
                if (p.host[0] && strcmp(p.host, host) == 0) return &p;
            return nullptr;
        }

        void remember(const char* host, const uint8_t sha[32]) {
            Pin* p = const_cast<Pin*>(find_pin(host));
            if (!p) {
                p = &pins[next_pin];
                next_pin = (next_pin + 1) % 2;
            }
            strncpy(p->host, host, sizeof(p->host) - 1);
            p->host[sizeof(p->host) - 1] = '\0';
            memcpy(p->sha, sha, 32);
        }

        void sha256_der(const mbedtls_x509_crt* c, uint8_t out[32]) {
#if MBEDTLS_VERSION_MAJOR >= 3
            mbedtls_sha256(c->raw.p, c->raw.len, out, 0);
#else
            mbedtls_sha256_ret(c->raw.p, c->raw.len, out, 0);
#endif
        }
    }  // namespace

    bool TwoStepTlsClient::session_leaf_sha256(uint8_t out[32]) {
        const mbedtls_x509_crt* leaf = sslclient ? mbedtls_ssl_get_peer_cert(&sslclient->ssl_ctx) : nullptr;
        if (!leaf) return false;
        sha256_der(leaf, out);
        return true;
    }

    // Step 1 and 2: copy the chain from a session, close it, verify the copy with the buffers freed.
    bool TwoStepTlsClient::fetch_and_verify(const char* host, uint16_t port, uint8_t leaf_sha[32]) {
        mbedtls_x509_crt chain;
        mbedtls_x509_crt_init(&chain);
        if (!SafeTlsClient::connect(host, port)) {
            snprintf(err_, sizeof(err_), "connect failed");
            return false;
        }
        const mbedtls_x509_crt* peer = mbedtls_ssl_get_peer_cert(&sslclient->ssl_ctx);
        for (const mbedtls_x509_crt* c = peer; c; c = c->next) mbedtls_x509_crt_parse_der(&chain, c->raw.p, c->raw.len);
        stop();  // frees the TLS buffers before the verification

        bool ok = false;
        if (!chain.raw.p) {
            snprintf(err_, sizeof(err_), "no certificate from the server");
        } else {
            mbedtls_x509_crt ca;
            mbedtls_x509_crt_init(&ca);
            const int parsed = mbedtls_x509_crt_parse(&ca, (const unsigned char*)ca_bundle_, strlen(ca_bundle_) + 1);
            uint32_t flags = 0;
            const int r = parsed < 0 ? parsed : mbedtls_x509_crt_verify(&chain, &ca, nullptr, host, &flags, nullptr, nullptr);
            if (r == 0) {
                sha256_der(&chain, leaf_sha);
                ok = true;
            } else {
                char info[80] = "";
                mbedtls_x509_crt_verify_info(info, sizeof(info), "", flags);
                char* nl = strchr(info, '\n');
                if (nl) *nl = '\0';
                snprintf(err_, sizeof(err_), "certificate not trusted (%d, flags 0x%lx%s%s)", r, (unsigned long)flags,
                         info[0] ? ": " : "", info);
            }
            mbedtls_x509_crt_free(&ca);
        }
        mbedtls_x509_crt_free(&chain);
        return ok;
    }

    int TwoStepTlsClient::connect(const char* host, uint16_t port) {
        err_[0] = '\0';
        uint8_t want[32];
        const Pin* pin = find_pin(host);
        for (int attempt = 0; attempt < 2; attempt++) {
            if (pin) {
                memcpy(want, pin->sha, 32);
            } else if (!fetch_and_verify(host, port, want)) {
                return 0;
            }
            // Step 3: the session we use must present exactly the verified certificate.
            if (!SafeTlsClient::connect(host, port)) {
                snprintf(err_, sizeof(err_), "connect failed");
                return 0;
            }
            uint8_t got[32];
            if (session_leaf_sha256(got) && memcmp(got, want, 32) == 0) {
                remember(host, want);
                return 1;
            }
            stop();
            pin = nullptr;  // certificate renewed (or another server): verify again, once
            snprintf(err_, sizeof(err_), "certificate changed between connections");
        }
        return 0;
    }

    int TwoStepTlsClient::connect(const char* host, uint16_t port, int32_t timeout_ms) {
        _timeout = timeout_ms;
        return connect(host, port);
    }

}  // namespace services
