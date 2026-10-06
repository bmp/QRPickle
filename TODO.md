# TODO

Gaps found during the initial Claude Code review (2026-10-06). Revisit once the Claude Code setup is complete.

## Build scripts (wired into `platformio.ini` but empty)
- [ ] `scripts/check_secrets.py` is 0 bytes. Implement a pre-build check that fails if credential-like strings appear in tracked source.
- [ ] `scripts/gzip_data.py` is 0 bytes. Either implement gzip of `data/www/*` (and serve `.gz` from `web_server.cpp`) or remove it from `extra_scripts`.
- [ ] `scripts/check_size.py` is 0 bytes. Implement a post-build size check against the 1.75MB app slot (`0x1C0000`) and the 448KB LittleFS partition, or remove it.

## Tests
- [ ] `test/test_parsers`, `test/test_scheduler` and `test/test_config` are empty, so `pio test -e native` runs nothing. Add native Unity tests, starting with the parsers (POTA/SOTA/DX/HamAlert) and `config_validation`.
- [ ] `test/test_hw_led.cpp` is an on-device sketch sitting in `test/`. Move it to a `test_*` folder as a proper embedded test, or move it out of `test/`.

## Build / release hygiene
- [ ] Every `pio run` overwrites the tracked `release/*.bin` via `scripts/release_copy.py`, so local builds produce noisy diffs. Decide whether `release/` should stay in git; CI already publishes binaries to GitHub Releases.
- [ ] `release.yml` releases on every push to `main` and force-overwrites the tag if `FW_VERSION` wasn't bumped. Consider moving to a feature-branch workflow and/or triggering releases only on a version bump or a manual dispatch.

## Small inconsistencies
- [ ] `platformio.ini`: the comment says `CORE_DEBUG_LEVEL=1`, but the flag is set to `4` (verbose).
- [ ] Filename typo: `src/ui/screens/clout_ota.cpp` (the header is `cloud_ota.h`).

## Security fixes (from the code review; details in `docs/reviews/2026-10-code-review.md`)
Ordered by priority.
- [ ] **1.1 High**: escape APRS messages and profile names in `app.js`, using `textContent` instead of `innerHTML`. Add a CSP header.
- [ ] **1.2 Critical**: add an admin password (ESPAsyncWebServer Digest auth) on `/api/*` and `/save-basic`. Give the setup AP a WPA2 password (`wifi_manager.cpp:50`).
- [ ] **1.3 High**: stop returning secrets from `/api/config` and `/api/profiles/get`; return `*_set: true/false` instead. On save, an empty value means "keep the existing value".
- [ ] **1.4 Medium**: replace the `strncpy(dst, doc["x"])` pattern with a null-safe `copy_json_str()` helper (web_server.cpp, profile_manager.cpp).
- [ ] **1.5 Medium**: route all config writes (web, profile, UI) through one validate-and-apply function.
- [ ] **1.6 Medium**: apply web config changes on the main loop rather than the AsyncTCP task; make the `flag_trigger_*` flags atomic.
- [ ] **1.7 Medium**: whitelist profile names (`[A-Za-z0-9_-]{1,24}`).
- [ ] **1.8 Low**: keep secrets out of profile files; document that NVS is unencrypted.

## Refactors (from the code review)
- [ ] **1.9**: replace the ~9 hand-maintained copies of the config field list with a single field table. This also covers 1.3–1.5.
- [ ] **1.10–1.13**: drop the `#define cfg` macro; rewrite the `FIXED:`/`NEW:` comments; document units (`tz_offset_hh` is half-hours, the `forecast_slots` bitmask); reassemble chunked request bodies.

## Still to investigate
- [ ] POTA/SOTA use `WiFiClientSecure::setInsecure()`. Check whether Cloud OTA does too (area 2 of the review).
