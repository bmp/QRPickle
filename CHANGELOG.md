# Changelog

User-facing changes per release, newest first. Each section is also the release commit message (see `docs/RELEASING.md`).

## v0.2.4 (2026-10-10)

Real band conditions from HAMQSL.com, privacy masks, screenshots; memory and crash fixes

Updating from v0.2.3: Cloud OTA (device or web console) or upload firmware.bin;
settings are kept. The new web console settings (own solar data source, band
groups on the dashboard, "use latitude/longitude") and the About credits also
need littlefs.bin (web console: System Info, "LittleFS Storage Image").
Writing the filesystem erases profiles, so back them up first (Profiles,
Backup and Restore). Without it the device works; those settings are missing.

Fixed
- The device could restart after opening Weather and then DX Cluster (the
  display library ran out of memory).
- The automatic update check often failed at boot and was not tried again
  until the next restart; it now starts 2 minutes after boot and retries
  every 10 minutes.
- A failed OpenWeather update was retried after 2 hours; now after 5 minutes.
- Forecast temperatures were nearly invisible.
- Downloads could fail when HamAlert and APRS were restarted in the middle of
  another download.

New
- Real solar data (SFI, K, A, X-ray, solar wind, Bz, noise) from HAMQSL.com,
  courtesy of N0NBH, every 3 hours (hourly in storms), or from your own source.
  Before, the screens showed fixed start-up values.
- Band conditions for 11 bands (160 m to 6 m), day and night at your location,
  with greyline and the next sunrise or sunset. The screen has three tabs:
  BANDS, SOLAR (each indicator with a scale and what it means) and GUIDE.
- Dashboard band tile: choose which band groups it shows (web console).
- Your latitude/longitude (or the grid square) set day and night.
- Privacy: the admin password is masked on the Network screen (tap the eye
  to show it for 10 seconds) and in the serial log.

Changed
- The firmware is 66 KB smaller (81.9 % of the update slot).
- The web console shows latitude and longitude side by side.

For developers
- docs/UI_GUIDE.md (also a manual appendix) and docs/MEMORY.md.
- tools/device_screens.py captures every screen, tabs and menu included;
  screenshot builds hide network names, addresses and positions.
- scripts/dev.sh format; /api/status reports memory figures.

## v0.2.3 (2026-10-10)

Updated LVGL and web server libraries; web console icon

Updating from v0.2.2: Cloud OTA (device or web console) or upload firmware.bin.
Settings and profiles are kept. The new browser icon also needs littlefs.bin
(web console: System Info, "LittleFS Storage Image"). Writing the filesystem
erases profiles, so back them up first (Profiles, Backup and Restore).
Without it everything works; the tab just has no icon.

New
- The web console shows the International amateur radio symbol as its
  browser tab icon.

Changed
- Display library LVGL 9.6 (from 9.5) and web server libraries
  ESPAsyncWebServer 3.12.1 / AsyncTCP 3.5.0 (from 3.11.0 / 3.4.10). Screens
  look the same; the WiFi network list is drawn without LVGL's list widget.

For developers
- CI and scripts/dev.sh run on Ubuntu 26.04.
- espressif32 7.1.3 was evaluated and not adopted (same Arduino core 2.0.17
  as 6.13.0; see docs/TASKS.md).

## v0.2.2 (2026-10-10)

Verified Cloud OTA downloads, theme fixes, correct APRS positions; container-based development

Updating from v0.2.1: Cloud OTA (device or web console) or upload firmware.bin.
No filesystem update is needed; settings and profiles are kept.

Fixed
- Cloud OTA could restart the device mid-download when the download took more
  than 30 seconds (the main-loop watchdog wasn't fed while waiting).
- Crash or dead network ("connected" but unreachable) when opening xOTA or
  during other HTTPS downloads (unsafe DNS lookups).
- APRS position: near a whole degree the minutes became "60.00" (an invalid
  position); now rounded correctly.
- "E-Ink Monochrome Dark" can be chosen in the web console and profiles
  (it was saved as E-Ink Light).
- DX cluster calls of 12 characters kept their last character; long APRS
  payloads and comments are no longer cut short on screen.
- Colours that ignored the selected theme (status dots, buttons, splash
  screen, list rows) now follow it; the Classic theme looks the same.

New
- Cloud OTA verifies the HTTPS certificate of the update server, in addition
  to the SHA-256 check.

For developers
- scripts/dev.sh runs every check in a container with CI's exact tools
  (Ubuntu 24.04 and 26.04).
- Compiler warnings are enabled for QRPickle's code, and CI fails on any.
- tools/device_screens.py captures every screen in every theme.

## v0.2.1 (2026-10-10)

Fixes the Cloud OTA update check; manual with images and appendices; restyled installer

Updating from v0.2.0 (once, by hand): v0.2.0's update check can't follow
the redirect to the release site, so its Cloud OTA won't find v0.2.1.
1. Web console → System Info → upload firmware.bin (target: firmware).
   No filesystem update is needed: settings and profiles are kept.
From v0.2.1 on, Cloud OTA finds new releases again.
Updating from v0.1.x: as for v0.2.0 (firmware.bin, then littlefs.bin),
or use the one-click installer.

Fixed
- Cloud OTA update check follows redirects (v0.2.0 reported "check failed").
- PDF manual: all screenshots and photos are included, sized to the page.
- Release ZIP holds only the release files.

New
- Manual: cover page, appendices (LED colours, flash memory map, building
  from source, libraries, third-party notices), header and footer, and an
  accessible tagged PDF.
- Installer page styled like ham.bharathpalavalli.com, with a link to the
  QRPickle article.
- Status LED: writes only when its colour changes.

## v0.2.0 (2026-10-09)

Web login, full profiles with backup, Cloud OTA via GitHub Pages

Updating from v0.1.x (once, by hand; Cloud OTA works from v0.2.0 on):
1. Note down any saved profiles: the new filesystem image erases them.
2. Web console → System Info → upload firmware.bin (target: firmware).
   The device restarts into v0.2.0.
3. Log in again: user "admin", password on the device's Network screen.
4. System Info → upload littlefs.bin (target: filesystem).
Or use the one-click installer: https://bmp.github.io/QRPickle/install/
(pick a version; "Erase device" clears all settings).

New
- Web console and setup hotspot are password-protected; change the
  password under Basic Settings.
- Profiles hold every setting; edit them in the settings form, delete
  them, and download/restore a backup.
- Cloud OTA downloads from GitHub Pages and verifies SHA-256; a release
  that doesn't boot healthily is rolled back automatically.
- One-click browser installer with a version picker.
- Status LED gives a faint cyan blink when new data arrives (APRS, DX
  and SOTA spots, POTA, propagation, weather).
- Hardware and wiring guide with the optional BME280 sensor.

Fixed
- APRS symbol chosen on the web page is now saved.
- Boot-time freezes (DNS), DX cluster connecting off the main loop,
  DX filters, SOTA spots from the SOTA cluster, and many smaller fixes.
- About tab credits the icons, logos and fonts QRPickle actually uses;
  licences are listed in THIRD_PARTY_NOTICES.md.
