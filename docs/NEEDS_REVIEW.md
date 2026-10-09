# Needs the owner's review

Items that automation could not verify (screen/touch/browser), plus patches for ask-first files.

## Visual / hands-on checks
- [x] **Plan C, Network screen:** shows `Web login: admin / <8 chars>`.
- [x] **Plan C, browser:** opening the console prompts for a login. After logging in, config loads; secret fields are blank with the placeholder "saved (leave blank to keep)"; saving with blanks keeps the stored secrets (WiFi still connects after a reboot).
- [x] **Plan C, profiles:** saving a new profile (name with letters/digits/`_-` only) and loading it still works; the dropdown lists the profiles.
- [ ] **Plan C, setup AP:** `QRPickle-Setup` now needs the same password (only seen when the device can't reach WiFi).
- [x] ~~2.1/2.2~~: verified automatically with `curl` (Plan D).
- [ ] **GitHub Pages setup (one-time):** Settings → Pages → Source: GitHub Actions; Settings → Environments → github-pages → allow tag pattern `v*` (docs/RELEASING.md).
- [ ] **First Pages release:** check https://bmp.github.io/QRPickle/install/ loads, and that Cloud OTA on a CYD finds the release (the local end-to-end test already passed: download, SHA-256, flash, healthy).
- [ ] **Plan D, Cloud OTA:** not testable without publishing a release. On the next release (docs/RELEASING.md step 5), use sidebar → Cloud OTA → ↻ → INITIATE FIRMWARE FLASH; the serial log should show `SHA-256 verified.` and then `[OTA Guard] New image healthy`.

- [x] **Plan E, LVGL trim (S2):** all images are RGB565A8 (kept) and no UI code uses whole-object opacity, transforms or blend modes, so the risk is low. Check: the splash logos, the weather-screen condition icon, the dashboard weather widget's two 20×20 icons, and symbol glyphs (⌂ ↻ WiFi). If anything is blank, re-enable `LV_DRAW_SW_SUPPORT_ARGB8888` in `include/lv_conf.h` and run `pio run -t clean`.
- [x] ~~keep `release/*.bin` in git?~~ No: untracked and ignored 2026-10-09; README links to GitHub Releases.
- [x] **gzip_data.py:** implemented 2026-10-09 (56KB → 12.6KB, verified with `Content-Encoding: gzip`).

- [x] **Plan F, xOTA:** POTA now loads in the background. The status dot is orange while fetching, the list fills when done, and the screen stays responsive during the fetch.
- [x] **Plan F, APRS radar:** stations now show their real symbol-based type (an off-by-one used to read E/W); compressed-position stations no longer appear at bogus coordinates.

- [ ] **Plan G, sensor offline:** (optional) unplug the BME280 and boot. The dashboard should show `-- C | --%` and `-- hPa`, and the weather screen `--`.

## Follow-up tasks
- [x] **Change the web/AP password from the web UI:** Basic Settings → "Admin Password" (8-16 printable characters, no spaces, typed twice). Device-checked on 2026-10-09 via the API: invalid values are ignored, a valid one applies at once (old login 401), reverted afterwards. Also fixed: the APRS symbol chosen on the web page was never saved (`aprs_icon` vs `aprs_icn`).
- [ ] **Owner check, admin password in the browser:** change it on Basic Settings, Save, confirm the browser asks for the new login and the Network screen shows it. Change it back if you like.
- [x] **Profiles hold every setting** (except the admin password) through one JSON field table shared with `/api/config` (review 1.9). Editing a profile keeps its own secrets; a profile without secrets keeps the device's when applied; delete and backup/restore added. Device-checked on 2026-10-09 via the API (snapshot, restore-mode, edit, apply with WiFi staying up, delete, path traversal, auth) and `web_security_check.sh` 17/17.
- [ ] **Owner check, profiles in the browser:** Profiles tab → save a profile, "Edit in Settings Form" (banner shows; Save writes the profile), Apply, Delete, Download Backup, then Restore from it. On the device: Settings → tap the profile button → Save applies the whole profile.

## Patches
All five patches were reviewed and applied on 2026-10-07 (0001 test filter, 0002 SHA-256, 0003 platformio.ini, 0004 CI, 0005 tag-only releases).

- [x] **First GitHub run of CI:** green on GitHub on 2026-10-09 (run 37940002821, branch `public/claude-setup`).
