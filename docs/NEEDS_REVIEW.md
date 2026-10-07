# Needs the owner's review

Items that automation could not verify (screen/touch/browser), plus patches for ask-first files.

## Visual / hands-on checks
- [ ] **Plan C, Network screen:** shows `Web login: admin / <8 chars>`.
- [ ] **Plan C, browser:** opening the console prompts for a login. After logging in, config loads; secret fields are blank with the placeholder "saved (leave blank to keep)"; saving with blanks keeps the stored secrets (WiFi still connects after a reboot).
- [ ] **Plan C, profiles:** saving a new profile (name with letters/digits/`_-` only) and loading it still works; the dropdown lists the profiles.
- [ ] **Plan C, setup AP:** `QRPickle-Setup` now needs the same password (only seen when the device can't reach WiFi).
- [x] ~~2.1/2.2~~: verified automatically with `curl` (Plan D).
- [ ] **Plan D, Cloud OTA:** not testable without publishing a release. After applying patch 0002 and making the next release, use "Check for update" → Flash; the serial log should show `SHA-256 verified.` and then `[OTA Guard] New image healthy`.

- [ ] **Plan E, LVGL trim (S2):** all images are RGB565A8 (kept) and no UI code uses whole-object opacity, transforms or blend modes, so the risk is low. Check: the splash logos, the weather-screen condition icon, the dashboard weather widget's two 20×20 icons, and symbol glyphs (⌂ ↻ WiFi). If anything is blank, re-enable `LV_DRAW_SW_SUPPORT_ARGB8888` in `include/lv_conf.h` and run `pio run -t clean`.
- [ ] **Plan E, decisions:** keep `release/*.bin` in git? Implement `gzip_data.py`, or drop it from `extra_scripts`?

- [ ] **Plan F, xOTA:** POTA now loads in the background. The status dot is orange while fetching, the list fills when done, and the screen stays responsive during the fetch.
- [ ] **Plan F, APRS radar:** stations now show their real symbol-based type (an off-by-one used to read E/W); compressed-position stations no longer appear at bogus coordinates.

- [ ] **Plan G, sensor offline:** (optional) unplug the BME280 and boot. The dashboard should show `-- C | --%` and `-- hPa`, and the weather screen `--`.

## Patches to apply (files on the ask-first list)
Apply with `git apply docs/patches/<file>` after review.
- ~~0001 native test_filter~~: applied 2026-10-07.
- `0002-release-publish-firmware-sha256.patch`: CI publishes `firmware.bin.sha256` with each release, so devices can verify Cloud OTA downloads.
- `0003-platformio-release-logging-pinned-deps.patch`: `CORE_DEBUG_LEVEL=1` (fixes the API key in the log), `cyd-debug` env, pinned libs, platform and XPT2046 commit. Test-built and device-checked in a throwaway worktree.
- `0004-ci-build-test-check.patch`: new `ci.yml` (tests, cppcheck, build on PRs).
- `0005-release-on-tags-only.patch`: releases only from `v*` tags matching `FW_VERSION`; tests first; no force-push. **New release flow:** bump `FW_VERSION`, commit, `git tag vX.Y.Z`, `git push origin vX.Y.Z`.

All patches pass `git apply --check` against the merged branch.
