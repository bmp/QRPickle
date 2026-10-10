# Open Tasks

The single list of open work: checks that need a person (screen, touch, browser), release steps, and follow-ups. Remove an item once it's done; git history keeps the record.

## Hands-on checks (owner, after v0.2.2)
The web console checks are automated (`tools/webui_e2e/`, 31/31 on 2026-10-09). These need a person:
- [ ] **Theme colours (v0.2.2):** review `release/shots/after/index.html` (7 changed screens in the 6 non-Classic themes; Classic is unchanged by construction). Noted: in E-Ink Light the grey "P" (poor) band badge has white text, which is low contrast.
- [ ] **On the device:** Settings → tap the profile button → Save applies the whole profile (e.g. the APRS macros change too).
- [ ] **Network screen** shows the admin password after changing it in the web console.
- [ ] **Setup hotspot:** when the device can't reach WiFi, `QRPickle-Setup` asks for the admin password, and the LED stays amber.
- [ ] **Sensor offline (optional):** unplug the BME280 and boot. The dashboard shows `-- C | --%` and `-- hPa`, the weather screen `--`.

## Watching
- [ ] **DX screen freeze (review 4.6): probably fixed, keep an eye on it.** Likely cause found 2026-10-10: unsafe DNS in the HTTPS fetches corrupted lwIP (crash in dns_tmr, or a dead network while "connected"); fixed with SafeClient/SafeTlsClient. Verified: xOTA opened 10x and a 5-min DX-screen test with an HTTPS fetch every 15 s (20/20, no crash, no reboot). If a freeze is ever seen again, capture with `tools/serial_soak.py` and decode the backtrace.

## Planned for v0.2.3
- [x] **Favicon for the web console:** the International amateur radio symbol (public domain), `data/www/favicon.svg`, 853 B gzipped, served behind login; recorded in `assets/img/SOURCES.md`.
- [x] **Library and platform updates:** LVGL 9.6.0, ESPAsyncWebServer 3.12.1, AsyncTCP 3.5.0; TFT_eSPI 2.5.43, ArduinoJson 7.4.3, Adafruit BME280 2.3.0 and XPT2046 (f956c5d) are current. espressif32 7.1.3 evaluated and not adopted: for Arduino it ships the same core 2.0.17 (`framework-arduinoespressif32` 4.20017, same source hash dcc1105b), GCC 8.4 and esptool 4.11 as 6.13.0; its ESP-IDF 6 / GCC 15 changes apply only to the pure ESP-IDF framework. It builds and passes every guard (image 384 B smaller, same DRAM). A real step up (Arduino 3 / IDF 5) would mean the pioarduino platform and a port: a separate project.

## Done in v0.2.4: propagation, privacy, screenshots, size
Findings: SFI/K/A are only parsed from APRS-IS lines, which never carry solar data, so the dashboard and band ratings always show the start-up defaults (SFI 100, K 2, A 10); the band-conditions screen shows three hardcoded placeholder lines ("IONOSPHERIC REACTION ACTIVE", "CORONAL PLASMA FLUX C1.0", "AURORAL SCATTER PATH CLOSED"); the rating rules are over-optimistic at night (15/10 m "GOOD" after dark) and ignore the A-index; the parser accepts a stray "K="/"A=" anywhere in a line. The README claims real solar data.
Order: data source -> rating model -> bands -> band-conditions screen -> privacy masks -> screenshots (last, so they show the final screens).
- [x] **Solar data source:** hamqsl.com `solarxml` (N0NBH) by default: SFI, sunspot number, A, K, X-ray, solar wind, Bz, aurora, MUF. Admin panel: optional own source URL (same XML format; validated, admin-only; never logged). One HTTPS fetch at a time via `SafeTlsClient` + NetLock, stream-parsed; credit N0NBH in About and `THIRD_PARTY_NOTICES.md`; follow its usage terms; data age on screen; "--" until the first fetch.
- [x] **Fetch schedule (agreed):** the indices are global and change slowly (SFI and A daily, K every 3 h), while day/night/greyline is local and computed on the device every minute without any fetch. So: fetch at boot, then shortly after each 3-hour K-index update (00/03/.../21 UTC + 15 min) = 8 a day; hourly while K >= 5 or an M/X flare is reported (storm mode); manual refresh on the screen (rate-limited). Never more often than hourly (be kind to the free service).
- [x] **Station location (agreed):** lat/lon from Settings when the user has set them (more precise), else the centre of the grid square; the web console warns when lat/lon lie outside the grid square. Adds a "lat/lon set" marker (today they default to 12.97/77.59, indistinguishable from a real entry).
- [x] **Bands (agreed grouping by propagation behaviour):** Low 160/80/60 (night, D-layer absorption by day); 40/30 (day and night workhorses); 20/17 (main DX, day + greyline); 15/12/10 (need high SFI, daytime); 6 m (Es/F2 only at solar maximum, shown as "Es possible"). Band-conditions screen: every band individually; dashboard tile: the groups. Web console: the user picks which groups the tile shows.
- [x] **Band-conditions screen:** remove the placeholders; real values (SFI, SSN, A/K, X-ray class, solar wind, Bz, aurora, MUF), data age, per-band day/night table; owner reviews the layout via the screenshot tool.
- [x] **Privacy masks on the device:** the Network screen shows the admin password, WiFi name, IP and MAC; mask secrets as `******` with tap to reveal (auto-hide after a few seconds); review every screen for other secrets.
- [x] **Screenshots replace the photos (once; afterwards only new or changed screens are captured):** extend `tools/device_screens.py` to every screen (tabs, menu, splash, power-save, sub-pages, and any new screen in this release), captured with live data loaded; capture builds mask WiFi name, IP, MAC and password, and the APRS list shows no position-revealing distances. Replace `docs/pics/*.jpeg` and the README table with the PNGs. Keep `docs/pics/vu3glj-logo.png` (manual cover) and `docs/pics/third-party/` (CYD pinout).
- [x] **README:** correct the solar-data description; new screenshot table; credit N0NBH (hamqsl.com) in About and `THIRD_PARTY_NOTICES.md`.
- [x] **UI guide** (`docs/UI_GUIDE.md`, manual appendix "Screens, Widgets and Themes"): geometry, shell, pages, tiles, themes, privacy, screenshots, checklists.
- Note: the `cyd-screens` build has less free heap; on it, HTTPS with certificate checks (Cloud OTA) and the first solar fetch can fail. Release builds are fine (checked 2026-10-10).


## Planned for v0.2.5
- [ ] **Rating model, documented and tested:** per band from SFI/SSN (estimated MUF), K/A and local day/night/greyline (sunrise/sunset from the station location); unit tests done in v0.2.4 (`band_rating`, `sun_calc`); still to write: the manual appendix (formulas, thresholds, sources, limits).
- [ ] **Satellites, SSTV and astronaut contacts (scope agreed 2026-10-10; decisions 2026-10-10):** passes for popular amateur satellites (FM voice, linear, SSTV, voice beacons, optional APRS), ISS crew voice frequencies, ARISS school contacts ("audible here" when the ISS is above the horizon) and SSTV events (start/end, frequency, mode).
  - **Feed:** a GitHub Actions job every 6 h builds one small JSON from SatNOGS (transmitters, filtered by category), CelesTrak (TLEs) and the ARISS pages (parsers tested against saved copies) and commits it to a separate `space-feed` branch; the CYD fetches it every 6 h (no location is sent) and computes passes itself with SGP4 (MIT/Apache library, licence check). The Pages site gets a sub-section explaining the feed (sources, schedule, format, privacy).
  - **Device:** Satellites screen (next passes: time, max elevation, direction, SSTV mode/frequency, voice frequencies) and an events list; LED signal before an SSTV-event or school-contact pass, **30 minutes** ahead by default, configurable (and switchable off) in the web console.
  - **Web console:** categories/satellites, minimum elevation, LED lead time, manual events.
  - Flash budget: 81.9 % of the slot after v0.2.4.
- [ ] **Theme review (owner):** E-Ink Light: the home button icon is faint on white; the band badges now pick a readable text colour automatically.
- [ ] **Web console: auto-brightness** tick box (stored and in profiles, but only settable on the device today).
- [ ] **`web_enabled` setting:** stored in NVS but never read or set; remove it or make it switch the web console.
- [ ] **APRS MY BEACON tab:** the status line ("STATUS: ACTIVE (APRS-IS Secure Link)") overlaps "TX COUNT" (seen in the v0.2.4 screenshots).
- [ ] **Screenshot build heap:** `cyd-screens` falls to ~100-400 B free at boot (the release build ~4 KB); data screens (OpenWeather, DX) need several minutes before they fill. Part of the memory coordinator work.
- [ ] **xOTA: show download failures.** A failed POTA fetch (seen with low heap) leaves an empty list with no message; show "POTA download failed, retrying" and the retry time in the comment bar.
- [ ] **TLS errors as numeric codes** (~15 KB flash): log mbedTLS's standard negative hex codes instead of `mbedtls_strerror`'s text table, keep text for the few common ones (`-0x7F00` out of memory, `-0x2700` certificate verification failed, timeouts, connection closed) and document them in `docs/DEBUGGING.md`.
- [ ] **Compressed splash logos** (LVGL RLE): measure the net saving (decoder cost included); the logos stay in the firmware.
- [ ] **Memory and network coordinator** (findings in `docs/MEMORY.md`, 2026-10-10): the heap's big region fragments within minutes (125 blocks, largest free 18 KB), mostly from stopping and restarting the telnet tasks in quiet windows, so TLS fails intermittently. (1) Fixed stacks for HamAlert, APRS and DX (`xTaskCreateStatic`, ~18 KB, no restart churn); (2) one gate for TLS: NetLock + quiet window + a check of the largest 8-bit block, retrying later instead of failing in mbedTLS; (3) a boot schedule: WiFi, NTP, solar, telnet services, update check, each after the previous has settled. Verify with `/api/status` `mem` over a long soak.

## Later
- [ ] **Arduino core 3 / ESP-IDF 5 (pioarduino platform) evaluation:** newer GCC (13+), mbedTLS 3, possibly lower memory use; with it, re-evaluate link-time optimisation (not supported by the current IDF 4.4 build) and the flash size. A port, not an upgrade: own release, long device tests.
