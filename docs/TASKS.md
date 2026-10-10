# Open Tasks

The single list of open work: checks that need a person (screen, touch, browser), release steps, and follow-ups. Remove an item once it's done; git history keeps the record.

## Hands-on checks (owner, after v0.2.2)
The web console checks are automated (`tools/webui_e2e/`, 31/31 on 2026-10-09). These need a person:
- [ ] **On the device:** Settings → tap the profile button → Save applies the whole profile (e.g. the APRS macros change too).
- [ ] **Network screen** shows the admin password after changing it in the web console.
- [ ] **Setup hotspot:** when the device can't reach WiFi, `QRPickle-Setup` asks for the admin password, and the LED stays amber.
- [ ] **Sensor offline (optional):** unplug the BME280 and boot. The dashboard shows `-- C | --%` and `-- hPa`, the weather screen `--`.

## Planned for v0.2.2
- [ ] **Theme colours (review 4.4):** move the ~27 hardcoded colours into the theme system; check each theme.
- [ ] **Cloud OTA certificate checking (review 2.11):** verify HTTPS certificates (verification was intermittent on this mbedTLS); SHA-256 stays mandatory.
- [ ] **CI warnings and errors:** go through the GitHub Actions logs (CI and release) and fix every warning, e.g. compiler warnings, deprecated actions, notices.
- [ ] **Network stack wedge (found 2026-10-10):** after ~1,200 short authenticated HTTP connections from the screenshot tool (cyd-screens build), the CYD stayed "CONNECTED" with its IP, UI and touch alive, but answered neither ping nor HTTP until reset; the loop watchdog doesn't catch it. Investigate (AsyncTCP/lwIP connection exhaustion?) and add a self-check that reconnects or reboots when the network stack stops responding.
- [ ] **DX screen freeze (review 4.6):** cause not found. 2026-10-09: 30 min on DX Spots with v0.2.0 code, no freeze, reset or heap drift (94.2-95.1 KB). Capture with `tools/serial_soak.py` and decode any watchdog backtrace ([DEBUGGING.md](DEBUGGING.md)).

## Planned for v0.2.3
- [ ] **Favicon for the web console:** a small icon (e.g. from the QRPickle/VU3GLJ artwork) served from LittleFS and linked in `data/www/index.html`; keep it tiny (filesystem budget), record its source in the licence manifests.
- [ ] **Library and platform updates:** check for newer versions of the pinned libraries (LVGL, TFT_eSPI, ArduinoJson, ESPAsyncWebServer/AsyncTCP, Adafruit BME280, XPT2046) and the espressif32 platform; update where safe, with device checks and `THIRD_PARTY_NOTICES.md`.
