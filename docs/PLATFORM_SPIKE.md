# Platform spike: Arduino core 3 / ESP-IDF 5 (2026-11-04)

Question: should QRPickle move from espressif32 6.13.0 (Arduino core 2.0.17, ESP-IDF 4.4, GCC 8.4)
to pioarduino (Arduino core 3.3.12, ESP-IDF 5.5.5, GCC 14.2)? Branch `spike/v0.3-pioarduino`, env
`cyd-p3`. Measured on the CYD; the device was restored from a verified full flash backup afterwards.

## Results

| | v0.2.6 (core 2.0.17 / IDF 4.4) | core 3 stock | core 3 tuned (dynamic TLS buffers, no Bluetooth) |
|---|---|---|---|
| Builds | yes | yes, after 2 small fixes | yes (framework rebuilt, `custom_sdkconfig`) |
| Firmware | 1,580,336 B (86.1 %) | 1,728,272 B (**94.2 %**) | 1,692,080 B (92.2 %) |
| Static DRAM | 64,348 B | 68,036 B | 65,536 B |
| Boots with the IDF 4.4 bootloader (OTA upload) | - | **yes**; the trial-boot guard works (1 crash, then healthy) | yes |
| Free heap a few minutes after boot | ~73 KB | ~47 KB (min 52 B) | **944 B** (largest block 276 B): starved |
| First TLS session at boot | ok | "X509 - Allocation of memory failed" | same |
| Cloud OTA update check | fails (release blocker, docs/TASKS.md) | postponed: 37 KB free, block 26.6 KB (needs 40 / 18 KB) | not reached |

Fixes needed to build (committed, version-guarded so v0.2.x still builds): mbedTLS 3 renamed
`mbedtls_sha256_*_ret`; IDF 5 configures the task watchdog with `esp_task_wdt_reconfigure()`;
`cloud_ota.cpp` includes `WiFi.h` itself.

## Reading

- The new platform works on the hardware and is reachable by OTA from v0.2.x (no USB needed), so a
  port is possible in principle.
- But stock core 3 leaves ~26 KB less heap, which this no-PSRAM device doesn't have: TLS fails at
  boot and the update check can't start. The tuned build (dynamic buffers) ran the heap to zero
  within minutes: dynamic buffers fragment the heap here instead of saving it.
- Flash rises to 92-94 % of the slot (limit 95 %), with no room for v0.4 (GPS) without trimming.

## Recommendation

Stay on core 2.0.17 / IDF 4.4 for now. A port would be its own project: find where core 3 spends the
extra heap (WiFi/lwIP buffer settings, task stacks, AsyncTCP/ESPAsyncWebServer on core 3), try
static instead of dynamic TLS buffers with smaller `MBEDTLS_SSL_IN_CONTENT_LEN`, and trim flash (the
framework's unused components). The Cloud OTA blocker needs a fix on the current platform first.

## Tooling notes

- pioarduino needs Python <= 3.14 (PlatformIO's own environment had moved to 3.15) and PlatformIO
  Core >= 6.2.0 on the PATH (the hybrid compile calls `pio` again).
- Use a separate `PLATFORMIO_CORE_DIR`: the first attempt in the shared `~/.platformio` replaced the
  shared esptool package and broke the release build until `intelhex` was added to the pipx
  PlatformIO environment.
