# Testing

What is tested where, and what isn't (audit 2026-11-04). CI (`.github/workflows/ci.yml`) runs
everything in the first table on every push and pull request.

## Automated (host, CI)

| Test | Covers | Run |
|---|---|---|
| `test/test_parsers` (Unity, native) | SOTA cluster, APRS, HamAlert and solar parsers; version compare; solar fetch schedule and manual refresh limit; grid/sun; band ratings; satellite feed parsing, passes and alerts; feed retry backoff; custom logo header check; DX status line and row columns | `pio test -e native -f test_parsers` |
| `test/test_config` | Config validation (`sanitize`), profile names, the JSON field table (round trips, secrets masked, `_tail`, `_clear`), cycling settings and order, satellite settings | `pio test -e native -f test_config` |
| `test/test_led` | Status LED colours and timings, the setup-hotspot amber | `pio test -e native -f test_led` |
| `scripts/test_source_tables.py` | Every setting in both the NVS and the JSON table, NVS keys (unique, at most 15 characters); every theme defines every base colour | `python3 -m unittest scripts/test_source_tables.py` |
| `test/js/logo_test.mjs`, `scripts/test_png_to_bin_lvgl9.py` | The web console's logo converter and the Python image converter (same bytes, same rounding, LittleFS blocks) | `node test/js/logo_test.mjs` |
| `test/js/install_plan_test.mjs`, `scripts/test_make_pages_site.py` | Installer modes and addresses, overlap/size/protected-area checks, the erase question in every manifest; the site builder's offsets, manifests and sizes | `node test/js/install_plan_test.mjs` |
| `tools/space_feed/test_build_feed.py` | Feed builder: categories, ARISS parsing, the source list (validation, fallbacks, credit, own events) | `python3 -m unittest discover -s tools/space_feed` |
| Build guards (pre/post-build scripts) | Secrets in sources, licences, LVGL widgets/symbols, static DRAM and image size | `pio run -e cyd` |
| `pio check`, `ruff`, `scripts/check_format.sh` | cppcheck (high/medium), Python lint, C/C++ format of changed lines | see CLAUDE.md |

## On a device (tools, run by hand)

| Tool | Covers |
|---|---|
| `tools/web_security_check.sh` | Digest auth on every route (incl. logos), secrets never returned, tails at most 2 characters, bad JSON and path traversal |
| `tools/webui_e2e/device_test.mjs` | The web console in Chromium: admin password, profiles, backup/restore (no secrets, no tails, logos section) |
| `tools/device_check.py` | Repeated resets: WiFi, NTP, watchdog resets, panics |
| `tools/device_screens.py` | Screenshots of every page, tab and theme (screenshot build); the corner rule (docs/UI_GUIDE.md) |
| `tools/serial_soak.py`, `/api/status` `mem` | Long runs, heap and LVGL pool, crash backtraces |

## Bug classes from the history and their cover

| Class (commits mentioning it) | Covered by | Gap |
|---|---|---|
| Out of memory, heap (21) | `/api/status` `mem`, check_size, device runs | No automated heap regression test; measured by hand per change (docs/MEMORY.md) |
| DNS / thread safety (7), network wedge | `connect_host()` by design; device runs | The wedge (no DNS while "connected", 2026-11-04) has no detector yet (docs/TASKS.md) |
| Quiet window / NetLock (10) | Code review; device runs | No host test (FreeRTOS-bound; the counter is a few lines) |
| Watchdog (11) | `device_check.py` | Manual |
| Config validation (22) | `test_config`, `test_source_tables.py` | - |
| Parsers (10) | `test_parsers` | - |
| Site and installer (39) | install/site tests, Pages build in CI | ESP Web Tools itself (third party) |
| Web security (many) | `web_security_check.sh` | Manual (needs a device) |
| LVGL / themes (17 / 9) | LVGL guard, theme table test, screenshots | Visual checks stay manual |
| LED (43) | `test_led` | - |
