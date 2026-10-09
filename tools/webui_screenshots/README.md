# Web console screenshots

Regenerates `docs/screenshots/*.png` from the current `data/www/` files, with demo data from a mock API. No device is needed and no real settings or secrets appear in the images.

Needs Node 22+ and a Chromium-based browser. From the repo root:

```bash
python3 -I tools/webui_screenshots/mock_server.py 8765 &
chromium --headless=new --remote-debugging-port=9223 --user-data-dir="$(mktemp -d)" about:blank &
node tools/webui_screenshots/capture.mjs 9223 http://127.0.0.1:8765/
kill %1 %2
```

With the Flatpak Ungoogled Chromium, start the browser with
`flatpak run --filesystem="$DIR" io.github.ungoogled_software.ungoogled_chromium --headless=new --remote-debugging-port=9223 --user-data-dir="$DIR" about:blank`, where `DIR` is a temporary directory under your home.

- The demo data is in `mock_server.py`; the version comes from `src/core/metadata.h`, so bump `FW_VERSION` before capturing for a release.
- The list of screenshots, and the clicks before each one, is `SHOTS` in `capture.mjs`. Images are full-page captures, 1128 CSS px wide at 2x.
