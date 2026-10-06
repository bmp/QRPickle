# Plan F: Network Robustness (review 3.3–3.10, 3.14 residual)

- [x] NetLock around all network sessions (TLS heap + HTTPClient DNS race).
- [x] Weather over HTTPS under the lock (20/20 OK on the device).
- [x] POTA fetch in its own task; xOTA status dot reflects it.
- [x] APRS: pure, native-tested position/addressee parsing (fixes compressed positions, the E/W symbol off-by-one, prefix callsign match); 4-deep TX queue.
- [x] HamAlert: login confirmed by greeting (10/10 on the device), stack 4096.
- [x] Device: 10 boots, 0 watchdog resets, 0 network-busy skips, 0 DNS failures. Native tests: 17/17.
- Open: DX connect on the UI thread (needs DX → task); see review 3.3.
