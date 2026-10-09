# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

QRPickle is ESP32 firmware for a ham-radio dashboard running on the "Cheap Yellow Display" (CYD: ESP32, 4MB flash, **no PSRAM**, ILI9341 320x240 + XPT2046 touch). Arduino framework on PlatformIO, UI in LVGL 9, plus an on-device web console served from LittleFS.

## Commands

```bash
pio run                        # build firmware (default env: cyd, CORE_DEBUG_LEVEL=1)
pio run -e cyd-debug -t upload # verbose framework logs (level 4); never release (docs/DEBUGGING.md)
pio run -t buildfs             # build littlefs.bin from data/
pio run -t upload              # flash firmware over USB (needs device attached)
pio run -t uploadfs            # flash data/ (web UI, images) to LittleFS
pio device monitor             # serial log @ 115200
pio test -e native             # host-side unit tests (Unity): test_parsers + test_config
pio test -e native -f test_config    # run a single test folder
```

Every build runs `extra_scripts` from `platformio.ini`. `scripts/release_copy.py` copies the built binaries into `release/` for convenience. That folder is **gitignored**: published binaries come only from CI on GitHub Releases.

Build guards and tests:
- `scripts/check_secrets.py` (pre-build) fails the build on credential-like literals in `src/`, `include/` and `data/`.
- `scripts/check_licenses.py` (pre-build, also standalone) fails the build when a library, platform version, font, image or file with a foreign copyright line isn't recorded in `THIRD_PARTY_NOTICES.md`, `assets/img/SOURCES.md` or `docs/pics/third-party/README.md`. It warns while image entries are UNCONFIRMED.
- `scripts/gzip_data.py` (pre-build) builds the LittleFS image from a staged copy of `data/` with `www/*.html|js|css` gzipped (56KB → 13KB); the server sends the `.gz` files with `Content-Encoding: gzip`. Edit the plain files in `data/`; never commit `.gz`.
- `scripts/check_size.py` (post-build) fails if static DRAM headroom is under 4KB or the image is over 95% of the OTA slot.
- Native unit tests (host): `test/test_parsers` (SOTA cluster, APRS parsing, version compare) and `test/test_config` (validation, profile names, the JSON field table). The native env compiles only the host-safe sources in `build_src_filter`; tests may `#include` other host-safe `.cpp` files directly. `test_filter` takes one pattern per line.
- `pio check -e cyd --severity=high --severity=medium` (cppcheck) must report no defects. CI (`.github/workflows/ci.yml`) runs all of the above.

Current gaps (don't assume these work):
- `test/test_hw_led/` is a standalone on-device LED/TFT sketch, not a Unity test.
- There is no linter or formatter config.

## Device checks and memory budgets

- `python3 -I tools/device_check.py --cycles 5` (run with PlatformIO's Python: `~/.platformio/penv/bin/python`) resets the CYD over USB N times and reports per boot: WiFi, NTP, watchdog resets and panics. It exits 0 on PASS. Expected result: 0 watchdog resets (30/30 boots after the 3.14 fix).
- `tools/dram_report.py <map>` lists the largest static DRAM users. Static DRAM (`dram0_0_seg`, 124,580 B) is separate from heap; PlatformIO's "RAM %" does not show it. Build a map with `PLATFORMIO_BUILD_FLAGS='-Wl,-Map,${BUILD_DIR}/firmware.map' pio run`.
- **Changes to `include/lv_conf.h` need `pio run -t clean`.** The `LV_CONF_PATH` include isn't dependency-tracked, so LVGL won't rebuild otherwise.
- LVGL's 64KB pool is heap-allocated at `lv_init()` (`LV_MEM_POOL_ALLOC`). Don't add large static buffers; allocate them at init.

## OTA

- Web uploads (`/api/system/update?target=firmware|filesystem`) and Cloud OTA both use the partition sizes.
- After a firmware update the **trial-boot guard** keeps the previous slot. A new image that doesn't reach 60s with WiFi within 3 boots is rolled back (`ota_manager.h`).
- Test builds can use `-DQRP_TEST_CRASH_AT_BOOT` (never release these).
- Cloud OTA reads `https://<owner>.github.io/<repo>/ota/ota.json` (version, notes, sha256) and downloads `firmware.bin` from the same place. GitHub's release download host is unreachable with this mbedTLS (review 2.9). The SHA-256 is mandatory; TLS isn't certificate-checked yet (2.11). Test builds can override the base URL with `-DQRP_OTA_BASE_URL=\"http://...\"` (also export it for `-t upload`, which rebuilds).
- `scripts/make_pages_site.py` builds the Pages site (Cloud OTA files + ESP Web Tools installer from `web-installer/index.html`); CI runs it on each release.

## Release process (important)

Releases happen **only** when a `v*` tag matching `FW_VERSION` (`src/core/metadata.h`) is pushed. CI checks the tag, runs the tests, builds, publishes `firmware.bin`, `firmware.bin.sha256` and `littlefs.bin` to the Release, and deploys GitHub Pages (`ota/`, `install/`). Pushing to `main` does not release; `ci.yml` checks every push and PR. Never push tags without explicit confirmation. Full procedure: `docs/RELEASING.md`.

## Architecture

**Boot and main loop** (`src/main.cpp`). `setup()` initialises, in order: LED, config (NVS), sensors, display, LVGL filesystem bridge, touch, WiFi, fonts, UI, web server. `loop()` is a cooperative poll on core 1. It calls each subsystem's `update()`, then `delay(5)`. `ui::display_update()` drives `lv_tick_inc` and `lv_timer_handler` from that loop.

**Threading model.** LVGL isn't thread-safe and only runs in `loop()`. Long or blocking network work runs in FreeRTOS tasks (APRS, HamAlert, Cloud OTA check/flash, the LED engine, xOTA resume), and POTA/SOTA fetches are started with `fetch_async()`. Results reach the UI through a **static manager + dirty-flag** pattern. Managers in `src/services/` (e.g. `PotaManager`) expose `get_*()`, `is_dirty()` and `clear_dirty()`, and screens poll them from LVGL timers. Never call `lv_*` from a background task.

**Memory constraints drive the design.** There is no PSRAM, and a TLS handshake (mbedTLS) needs ~40KB of contiguous heap. Network fetches therefore:
- use `WiFiClientSecure::setInsecure()`,
- stream-parse JSON element by element with ArduinoJson,
- are staggered so that two TLS sessions never overlap.

Out-of-memory panics during HTTPS fetches and OTA are a recurring bug class (see git log).

**Network sessions:** wrap every DNS+connect or HTTP(S) request in `services::NetLock` (`net_lock.h`); one session at a time, which keeps TLS heap use and lookups serialised. `connect_host()` takes it already.

**DNS:** never call `WiFi.hostByName()` or `WiFiClient::connect(hostname, …)` from tasks. The Arduino 2.0.17 implementation isn't thread-safe and caused boot-time watchdog resets (review 3.14). Use `services::connect_host()` (`src/services/net_connect.h`), which uses `getaddrinfo()`. After an abnormal reset, `[CRASHLOG]` lines at boot show each task's last breadcrumb (`src/core/crashlog.h`). When adding a network feature, check the free and max-alloc heap. Don't start a TLS request while another is in flight.

**Layers:**
- `src/hw/`: board drivers (display, touch, BME280 sensor, RGB status LED). `User_Setup.h` holds the TFT_eSPI pin config, force-included via `build_flags`. LED state meanings are documented in `docs/LEDColours.md`.
- `src/core/`: `metadata.h` (version and identity), `timekeeper`, and `lvgl_fs` (maps the LVGL filesystem to LittleFS so images in `data/img/*.bin` load at runtime).
- `src/config/`: a single `config::Config` struct persisted to NVS through `Preferences`. Read it with `config::get()`; mutate with `mutable_get()` then `save()`. **All credentials are entered at runtime** through the setup AP or web UI and stored in NVS, not compiled in: WiFi, the OpenWeather key, the APRS passcode and the HamAlert password. `config_validation` sanitises input. `config_json` is the one JSON field table shared by `/api/config` and profiles: add a new setting there (and to the NVS table in `config.cpp`) and both pick it up. Profiles (`profile_manager`) hold every setting except the admin password, as JSON under `/profiles` in LittleFS; writing `littlefs.bin` erases them (the web console has backup/restore, without secrets).
- `src/services/`: one manager per data source (weather, POTA, SOTA, DX cluster, HamAlert telnet, APRS-IS, propagation), plus wifi, web_server, OTA (manual upload), cloud_ota (GitHub pull) and display_manager (backlight, sleep, LDR auto-brightness).
- `src/ui/`: `ui.cpp` owns the persistent shell (status bar, sidebar, home button) and swaps page content into a single `view_container` via `ui_navigate_local(LocalPage)`. Each page lives in `screens/` with create/destroy lifecycle functions. Dashboard tiles live in `widgets/` and are sized by `WidgetSize`.

**Web console security.**
- Every route requires Digest auth: `REQUIRE_AUTH` for plain routes; `auth_gate` plus an `authorized()` check in the body/upload callback for POST bodies.
- Never return secrets from the API; send `*_set` flags instead.
- Parse JSON strings with `copy_str`/`copy_secret` (`src/services/json_copy.h`).
- Validate with `config::sanitize()`.
- Apply config changes on the main loop via `pending_config`.
- Test with `QRP_ADMIN_PW=… tools/web_security_check.sh <ip>`.

**Web console.** `data/www/{index.html,app.js,style.css}` is served from LittleFS; there is an inline fallback page in `web_server.cpp` if the filesystem is missing. The REST endpoints (`/api/config`, `/api/profiles/*`, `/api/aprs/*`, `/api/system/update`, `/api/cloud_ota/*`) are registered in `web_server_init()`. Changes to `data/` require `pio run -t buildfs` / `uploadfs`; they don't go out with a firmware flash.

**Flash layout** (`partitions.csv`): two 1.75MB OTA app slots (`app0`/`app1`) and a 448KB LittleFS partition at `0x390000`. The README and the web-flasher instructions depend on these offsets, so changing them breaks OTA for deployed devices.

## Assets

Generated assets are committed; regenerate them only when the sources change:
- `assets/img/*.png` → `data/img/*.bin` (LVGL 9 RGB565A8) via `scripts/png_to_bin_lvgl9.py`. Filenames must end in `_WxH.png`.
- Fonts: `assets/fonts/*.ttf` → `src/ui/fonts/*_raw.c` via `scripts/build_fonts.sh`. This needs `lv_font_conv` and prompts interactively before overwriting. It prepends the OFL copyright header to each generated file. New third-party components go in `THIRD_PARTY_NOTICES.md` (the release zip ships it with the OFL texts).

See `docs/AssetGenerationPipeline.md` for details.

## Secrets

`src/config/secrets_default.h` is gitignored and not referenced by the code. Never commit real credentials. Keep log output that prints config values masked (see `config::log_summary()`).
