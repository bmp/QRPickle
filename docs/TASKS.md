# Open Tasks

The single list of open work: checks that need a person (screen, touch, browser), release steps, and follow-ups. Remove an item once it's done; git history keeps the record.

## Hands-on checks (owner, when convenient)
The web console checks are automated (`tools/webui_e2e/`, 31/31 on 2026-10-09). These need a person:
- [ ] **On the device:** Settings → tap the profile button → Save applies the whole profile (e.g. the APRS macros change too).
- [ ] **Network screen** shows the admin password after changing it in the web console.
- [ ] **Setup hotspot:** when the device can't reach WiFi, `QRPickle-Setup` asks for the admin password, and the LED stays amber.
- [ ] **Sensor offline (optional):** unplug the BME280 and boot. The dashboard shows `-- C | --%` and `-- hPa`, the weather screen `--`.

## Release
- [ ] **GitHub Pages, one-time:** Settings → Pages → Source: GitHub Actions; Settings → Environments → github-pages → allow tag pattern `v*` ([RELEASING.md](RELEASING.md)).
- [ ] **After the first Pages release:** https://bmp.github.io/QRPickle/install/ loads.
- [ ] **Cloud OTA end-to-end:** only testable with the release after v0.2.0. A v0.2.0 device → Cloud OTA → ↻ → flash; the serial log shows `SHA-256 verified.` and then `[OTA Guard] New image healthy` ([RELEASING.md](RELEASING.md) step 5).
- [ ] **Code review doc:** decide whether to publish `docs/reviews/2026-10-code-review.md` (kept local for now).

## Licences
- [ ] **VU2ARC logo (optional):** barc.in publishes no logo terms; it's used with attribution. A short OK from the club would remove any doubt.

## Engineering follow-ups
- [ ] **DX screen freeze (review 4.6):** cause not found. 2026-10-09: 30 min on DX Spots with v0.2.0 code, no freeze, reset or heap drift (94.2-95.1 KB). If it recurs, capture with `tools/serial_soak.py` and decode the watchdog backtrace ([DEBUGGING.md](DEBUGGING.md)).
- [ ] **Cloud OTA certificate checking (review 2.11):** HTTPS is not certificate-checked yet; integrity relies on the mandatory SHA-256.
- [ ] **Cosmetic review items 4.3 / 4.4.**
- [ ] `test/test_scheduler/` is empty and excluded from `test_filter`: add tests or remove it.
- [ ] **After v0.2.0: linter and formatter.** `clang-format` for C++ (config matching the current style, checked in CI; no mass reformat) and `ruff` for the Python scripts.
