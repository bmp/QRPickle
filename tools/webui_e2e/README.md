# Web console end-to-end test (on a device)

`device_test.mjs` drives the real web console of a connected CYD in a headless Chromium page and cross-checks every step through the REST API:
- **Admin password:** mismatched or short inputs are rejected; a valid change applies, the old password is refused, and it's changed back through the UI.
- **Profiles:** save a new profile; edit it in the settings form (banner, "Save to Profile", admin fields hidden); the device settings stay unchanged; apply it, and WiFi stays up.
- **Backup and restore:** the backup contains no secrets; delete, then restore.

At the end it restores the device's settings and admin password exactly, and deletes its test profile (`E2E_Test`). It changes the theme on the device for a few seconds while it runs.

Needs Node 22+ and a Chromium-based browser started with remote debugging. From the repo root:

```bash
chromium --headless=new --remote-debugging-port=9223 --user-data-dir="$(mktemp -d)" about:blank &
QRP_ADMIN_PW=<admin password> node tools/webui_e2e/device_test.mjs <device-ip> 9223
kill %1
```

(Flatpak Ungoogled Chromium: see `tools/webui_screenshots/README.md`.) The password comes only from the environment. Expect `RESULT pass=31 fail=0`.
