# Changelog

User-facing changes per release, newest first. Each section is also the release commit message (see `docs/RELEASING.md`).

## v0.3.0 (2026-11-08)

New platform with security updates, POTA spots load again when you open xOTA, faster WiFi recovery, and data that says how old it is

Update with Cloud OTA (from v0.2.8) or the web console; your settings,
profiles and logos stay. This release moves to a newer platform (Arduino
core 3 / ESP-IDF 5): going back to v0.2.x needs a web console upload of
that version.

New
- Weather (OpenWeather tab) and xOTA (POTA tab) say how old their data is
  ("updated 5 min ago"), in the warning colour when it is stale.
- The firmware runs on Arduino core 3.3.12 / ESP-IDF 5.5, which still gets
  security fixes (TLS included); v0.2.x used a platform that no longer
  does.

Fixes
- Opening xOTA by hand never loaded POTA spots while HamAlert or APRS-IS
  was on (since v0.2.8; screen cycling still worked).
- Leaving xOTA no longer reconnects the DX cluster in the background
  (it held about 5 KB of memory for nothing).
- After a missed WiFi connect at start-up the device tries the saved
  network every 15 seconds for 5 minutes (was every minute), and the log
  says why the connection failed.
- HamAlert, APRS-IS and the DX and SOTA clusters notice a dead connection
  (after a router restart, for example) and reconnect, instead of looking
  connected with no data. The Spots page reconnects DX while it is open.
- More free memory for downloads: unused spot lists are freed, task stacks
  are sized from measurements, and downloads no longer start while
  another one is running.
- The satellite feed download can no longer hang (it waited for the
  server to close the connection).

For developers
- /api/status: `fresh` (age, failures and state per data source); free
  stack of the network and timer tasks.
- docs/DATA_FLOW.md lists every download and background job; its table is
  generated from src/services/sources.h and checked by the build.
- Building needs Python 3.14 or older and PlatformIO 6.2.0, also inside
  pioarduino's own environment (docs/DEVELOPMENT.md).

## v0.2.8 (2026-11-07)

Downloads work while screens cycle, no crash on the xOTA screen, and a splash logo preview

Update with Cloud OTA (from v0.2.7) or the web console. The web console
files changed (the preview button): also upload littlefs.bin in the web
console, which keeps your profiles and logos.

New
- Web console, Splash Logos: "Preview on the device" shows the start screen
  with your logos for 10 seconds (or until you tap it), without a restart.
- GitHub: security policy (report vulnerabilities privately), contributing
  guide, code of conduct, accessibility notes, and issue and pull request
  templates.

Fixes
- The device could restart on the xOTA screen while the web console was
  open: the web server ran out of memory during the login. It now turns a
  request away when memory is low instead.
- While screens cycle, POTA spots, the satellite feed and the update check
  download again (they all failed for memory, also in v0.2.7). Measured on
  the device over 2 x 30 minutes of cycling: every download OK (was none).
- Turning cycling off while the DX cluster was connected in the background
  left it running unread, and its spots filled the memory (20 KB within
  minutes). It now disconnects.
- HamAlert and APRS-IS stay connected during downloads instead of
  reconnecting after each one (the reconnects split the free memory).
- xOTA and APRS lists use less screen memory (xOTA 49.5 -> 33.5 KB,
  APRS 44.1 -> 33.9 KB), so the screen memory pool is 16 KB smaller and that
  memory goes to downloads.
- The device could restart (watchdog) when a weather update was due while
  another download was running: the screen waited for the network for up to
  40 seconds. Weather now tries again 30 seconds later instead.
- A firmware or filesystem upload in the web console failed ("Could Not
  Activate The Firmware" or "SESSION_ABORTED") when the upload before it had
  been cut off. Each upload now starts clean.
- Switching APRS-IS on or off in the web console (or with a profile) takes
  effect at once; it used to wait for a restart.
- A download that never finishes can no longer keep the device off the
  network: after 3 minutes the device restarts and /api/status names the
  task that was stuck.
- The log no longer reports a skipped weather update as "OK".

For developers
- /api/status: memory per download job, fragmentation, screen memory per
  page, refused web requests, the reset reason, the background connections
  (links), and the task behind a network-lock restart; [HEAP] lines trace
  the boot.

## v0.2.7 (2026-11-04)

Cloud OTA can install updates again

Updating from v0.2.6 or earlier: upload firmware.bin in the web console
(System Info, firmware); settings are kept. Cloud OTA on those versions finds
the update but can't install it (this release fixes that). From v0.2.7 on,
Cloud OTA installs updates by itself. The web console files are unchanged
(no littlefs.bin needed).

Fixes
- Cloud OTA: installing an update failed right after the download started
  ("No Error"): with the HTTPS connection open, the device had no 4 KB left
  for the updater. The updater now reserves its memory first. Checked on the
  device: download, SHA-256 check, install and restart into the new version.
- No error lines in the log when no custom splash logo is set.

## v0.2.6 (2026-11-04)

Your own splash logos, cycling order, saved-secret hints, a lighter DX page; theme and LED fixes

Updating from v0.2.5: Cloud OTA (device or web console) or upload firmware.bin;
settings are kept. Then upload littlefs.bin (web console: System Info, "LittleFS
Storage Image") for the new web console. Download a backup first (Profiles,
Backup and Restore): this first update still runs from the old console, which
erases profiles. From v0.2.6 on, the console keeps them. On v0.2.5 the update
check often fails (fixed in v0.2.6) but retries every 10 minutes until it gets
through; uploading firmware.bin in the web console works at once.

New
- Splash logos: your own logo left and/or right of the title on the start
  screen (a club logo, a callsign badge). Upload any image in the web console
  (System Info, Splash Logos); it is scaled into 60 x 60 in the browser, with a
  preview. Kept across updates and included in backups. A PNG with a
  transparent background looks best.
- Screen cycling in your order: the web console lists the screens with up and
  down arrows.
- Saved passwords and keys show their last 2 characters in the web console
  ("saved, ends in ••4f"; only for 6 or more characters), and each has a
  Clear button. Backups still contain no secrets.

Changes
- A filesystem update in the web console keeps your profiles and logos: it
  saves them first and writes them back after the restart (profiles without
  their own passwords and keys, as with a backup).
- Backups include your splash logos.
- The weather icons are 36 x 36 (were 40 x 40): each fits one 4 KB block of
  the filesystem, which frees 72 KB.
- The DX Cluster page says what it is doing ("CONNECTED: waiting for
  spots...") instead of showing an empty list, and needs much less memory
  (one line per spot).
- Home and refresh are in the same place on every page (bottom right), and
  nothing else sits under them.
- The satellite feed is retried after 2 and 5 minutes when a download fails
  (it waited 15); its sources are a list (AMSAT orbits when CelesTrak is down).

Fixes
- Cloud OTA can check for updates again: the certificate check ran out of
  memory during the connection and failed almost every time (also on v0.2.5).
  The certificate is now checked between two connections, with the memory
  free; the security is the same.
- Changing the theme now recolours the whole screen at once: the background,
  the home button and the menu kept the old theme until a restart (in E-Ink
  Light the home icon was faint and the Network page unreadable).
- The status LED stays amber while the setup hotspot is up, as documented; it
  used to go dark at the end of the start-up.
- Opening a page while Settings was open left Settings on top.

## v0.2.5 (2026-10-12)

Satellites and SSTV, screen cycling, more settings on the device; more free memory

Updating from v0.2.4: Cloud OTA (device or web console) or upload firmware.bin;
settings are kept. The new web console settings (screen cycling, satellites)
also need littlefs.bin (web console: System Info, "LittleFS Storage Image").
Writing the filesystem erases profiles, so back them up first (Profiles,
Backup and Restore).

New
- Satellites screen: the next passes of popular amateur satellites over your
  station (FM, linear, SSTV, voice beacons, optionally APRS), with times,
  directions and frequencies; ISS SSTV events and ARISS school contacts, with
  whether the ISS is above your horizon for a contact. Passes are computed on
  the device from orbits updated every 6 hours; your location is not sent.
- The status LED blinks purple before an ISS SSTV or school-contact pass (30
  minutes ahead by default) and breathes during it. Configurable in the web
  console, switchable on the device.
- Screen cycling: chosen screens in turn (10 seconds each by default). A touch
  pauses it; it resumes after 2 minutes. An icon in the status bar shows it
  and pauses or resumes it.
- Device Settings: APRS, screen cycling and pass alerts on/off.
- Web console: automatic brightness (light sensor) on/off.
- A page about the satellite feed on the project site.

Fixed
- The automatic update check failed for lack of memory; about 16 KB more
  memory is free while running.
- APRS sent a position beacon on every reconnect.
- xOTA showed an empty POTA list without a word when the download failed.
- The APRS beacon tab's status line ran into the beacon count.



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
