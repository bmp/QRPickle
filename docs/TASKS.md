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

## Planned for v0.2.4: propagation (approved 2026-10-10)
Findings: SFI/K/A are only parsed from APRS-IS lines, which never carry solar data, so the dashboard and band ratings always show the start-up defaults (SFI 100, K 2, A 10); the band-conditions screen shows three hardcoded placeholder lines ("IONOSPHERIC REACTION ACTIVE", "CORONAL PLASMA FLUX C1.0", "AURORAL SCATTER PATH CLOSED"); the rating rules are over-optimistic at night (15/10 m "GOOD" after dark) and ignore the A-index; the parser accepts a stray "K="/"A=" anywhere in a line. The README claims real solar data.
- [ ] **Real solar data from hamqsl.com `solarxml` (N0NBH):** SFI, sunspot number, A, K, X-ray, solar wind, Bz, aurora, MUF; one HTTPS fetch every 30-60 min through `SafeTlsClient` + NetLock, stream-parsed; check and follow its usage/attribution terms (credit in About and `THIRD_PARTY_NOTICES.md`); show the data age; "--" until the first fetch.
- [ ] **Rating model, documented and tested:** per band from SFI/SSN (estimated MUF), K/A and local day/night (sunrise/sunset from the station's lat/lon); unit tests; written up as a manual appendix (formulas, thresholds, sources, limits).
- [ ] **More bands:** 160, 80, 60, 40, 30, 20, 17, 15, 12, 10, 6 m, each rated (day/night), instead of four pairs; dashboard widget shows a sensible subset.
- [ ] **Band-conditions screen:** remove the placeholders; real values (SFI, SSN, A/K, X-ray class, solar wind, Bz, aurora, MUF), data age, per-band day/night table; owner reviews the layout via the screenshot tool.
- [ ] **README:** correct the solar-data description.

