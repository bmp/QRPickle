# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

QRPickle is ESP32 firmware for a ham-radio dashboard running on the "Cheap Yellow Display" (CYD: ESP32, 4MB flash, **no PSRAM**, ILI9341 320x240 + XPT2046 touch). Arduino framework on PlatformIO, UI in LVGL 9, plus an on-device web console served from LittleFS.

## Commands

```bash
pio run                        # build firmware (default env: cyd)
pio run -t buildfs             # build littlefs.bin from data/
pio run -t upload              # flash firmware over USB (needs device attached)
pio run -t uploadfs            # flash data/ (web UI, images) to LittleFS
pio device monitor             # serial log @ 115200
pio test -e native             # host-side unit tests (Unity)
pio test -e native -f test_parsers   # run a single test folder
```

Every build runs `extra_scripts` from `platformio.ini`. `scripts/release_copy.py` copies the built `firmware.bin`, `littlefs.bin`, `bootloader.bin` and `partitions.bin` into `release/`, which is **tracked in git**, so any build changes tracked files.

Current gaps (don't assume these work):
- `scripts/check_secrets.py` and `scripts/gzip_data.py` are empty (0 bytes), so they do nothing. `scripts/check_size.py` fails the build if static DRAM headroom is under 4KB or the image is over 95% of the OTA slot.
- Native unit tests live in `test/test_parsers` (SOTA cluster parser). The native env compiles only the host-safe sources listed in its `build_src_filter`. `test_filter` takes one pattern per line.
- `test/test_hw_led/` is a standalone on-device LED/TFT sketch, not a Unity test.
- There is no linter or formatter config.

## Device checks and memory budgets

- `python3 -I tools/device_check.py --cycles 5` (run with PlatformIO's Python: `~/.platformio/penv/bin/python`) resets the CYD over USB N times and reports per boot: WiFi, NTP, watchdog resets and panics. It exits 0 on PASS. Expected result: 0 watchdog resets (30/30 boots after the 3.14 fix).
- `tools/dram_report.py <map>` lists the largest static DRAM users. Static DRAM (`dram0_0_seg`, 124,580 B) is separate from heap; PlatformIO's "RAM %" does not show it. Build a map with `PLATFORMIO_BUILD_FLAGS='-Wl,-Map,${BUILD_DIR}/firmware.map' pio run`.
- **Changes to `include/lv_conf.h` need `pio run -t clean`.** The `LV_CONF_PATH` include isn't dependency-tracked, so LVGL won't rebuild otherwise.
- LVGL's 64KB pool is heap-allocated at `lv_init()` (`LV_MEM_POOL_ALLOC`). Don't add large static buffers; allocate them at init.

## Release process (important)

`.github/workflows/release.yml` runs on **every push to `main`**:
1. Reads `FW_VERSION` from `src/core/metadata.h`.
2. Force-pushes a git tag with that version.
3. Builds the firmware and filesystem.
4. Renders README.md to a PDF (pandoc + typst).
5. Publishes a GitHub Release using the last commit message as the changelog.

The device's Cloud OTA (`src/services/cloud_ota.cpp`) pulls releases from `meta::GITHUB_REPO`. So a push to `main` effectively ships firmware to users. Bump `FW_VERSION` for each release; otherwise the existing tag/release is overwritten. Don't push to `main` without explicit confirmation.

## Architecture

**Boot and main loop** (`src/main.cpp`). `setup()` initialises, in order: LED, config (NVS), sensors, display, LVGL filesystem bridge, touch, WiFi, fonts, UI, web server. `loop()` is a cooperative poll on core 1. It calls each subsystem's `update()`, then `delay(5)`. `ui::display_update()` drives `lv_tick_inc` and `lv_timer_handler` from that loop.

**Threading model.** LVGL isn't thread-safe and only runs in `loop()`. Long or blocking network work runs in FreeRTOS tasks (APRS, HamAlert, Cloud OTA check/flash, the LED engine, xOTA resume), and POTA/SOTA fetches are started with `fetch_async()`. Results reach the UI through a **static manager + dirty-flag** pattern. Managers in `src/services/` (e.g. `PotaManager`) expose `get_*()`, `is_dirty()` and `clear_dirty()`, and screens poll them from LVGL timers. Never call `lv_*` from a background task.

**Memory constraints drive the design.** There is no PSRAM, and a TLS handshake (mbedTLS) needs ~40KB of contiguous heap. Network fetches therefore:
- use `WiFiClientSecure::setInsecure()`,
- stream-parse JSON element by element with ArduinoJson,
- are staggered so that two TLS sessions never overlap.

Out-of-memory panics during HTTPS fetches and OTA are a recurring bug class (see git log).

**DNS:** never call `WiFi.hostByName()` or `WiFiClient::connect(hostname, …)` from tasks. The Arduino 2.0.17 implementation isn't thread-safe and caused boot-time watchdog resets (review 3.14). Use `services::connect_host()` (`src/services/net_connect.h`), which uses `getaddrinfo()`. After an abnormal reset, `[CRASHLOG]` lines at boot show each task's last breadcrumb (`src/core/crashlog.h`). When adding a network feature, check the free and max-alloc heap. Don't start a TLS request while another is in flight.

**Layers:**
- `src/hw/`: board drivers (display, touch, BME280 sensor, RGB status LED). `User_Setup.h` holds the TFT_eSPI pin config, force-included via `build_flags`. LED state meanings are documented in `docs/LEDColours.md`.
- `src/core/`: `metadata.h` (version and identity), `timekeeper`, and `lvgl_fs` (maps the LVGL filesystem to LittleFS so images in `data/img/*.bin` load at runtime).
- `src/config/`: a single `config::Config` struct persisted to NVS through `Preferences`. Read it with `config::get()`; mutate with `mutable_get()` then `save()`. **All credentials are entered at runtime** through the setup AP or web UI and stored in NVS, not compiled in: WiFi, the OpenWeather key, the APRS passcode and the HamAlert password. `config_validation` sanitises input. Profiles (`profile_manager`) are config snapshots stored as JSON under `/profiles` in LittleFS.
- `src/services/`: one manager per data source (weather, POTA, SOTA, DX cluster, HamAlert telnet, APRS-IS, propagation), plus wifi, web_server, OTA (manual upload), cloud_ota (GitHub pull) and display_manager (backlight, sleep, LDR auto-brightness).
- `src/ui/`: `ui.cpp` owns the persistent shell (status bar, sidebar, home button) and swaps page content into a single `view_container` via `ui_navigate_local(LocalPage)`. Each page lives in `screens/` with create/destroy lifecycle functions. Dashboard tiles live in `widgets/` and are sized by `WidgetSize`.

**Web console.** `data/www/{index.html,app.js,style.css}` is served from LittleFS; there is an inline fallback page in `web_server.cpp` if the filesystem is missing. The REST endpoints (`/api/config`, `/api/profiles/*`, `/api/aprs/*`, `/api/system/update`, `/api/cloud_ota/*`) are registered in `web_server_init()`. Changes to `data/` require `pio run -t buildfs` / `uploadfs`; they don't go out with a firmware flash.

**Flash layout** (`partitions.csv`): two 1.75MB OTA app slots (`app0`/`app1`) and a 448KB LittleFS partition at `0x390000`. The README and the web-flasher instructions depend on these offsets, so changing them breaks OTA for deployed devices.

## Assets

Generated assets are committed; regenerate them only when the sources change:
- `assets/img/*.png` → `data/img/*.bin` (LVGL 9 RGB565A8) via `scripts/png_to_bin_lvgl9.py`. Filenames must end in `_WxH.png`.
- Fonts: `assets/fonts/*.ttf` → `src/ui/fonts/*_raw.c` via `scripts/build_fonts.sh`. This needs `lv_font_conv` and prompts interactively before overwriting.

See `docs/AssetGenerationPipeline.md` for details.

## Secrets

`src/config/secrets_default.h` is gitignored and not referenced by the code. Never commit real credentials. Keep log output that prints config values masked (see `config::log_summary()`).
