# Changelog

User-facing changes per release, newest first. Each section is also the release commit message (see `docs/RELEASING.md`).

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
