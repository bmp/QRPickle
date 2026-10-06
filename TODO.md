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

## Security items to investigate (not yet verified)
- [ ] Check whether `GET /api/config` returns secrets (WiFi password, OpenWeather key, APRS passcode, HamAlert password) in plain text.
- [ ] Check whether the web console and endpoints like `/api/system/update`, `/api/cloud_ota/flash` and `/api/system/reboot` require any authentication on the LAN.
- [ ] POTA/SOTA clients use `WiFiClientSecure::setInsecure()` (no certificate validation). Assess whether Cloud OTA does the same; an unvalidated firmware download is the higher risk.
