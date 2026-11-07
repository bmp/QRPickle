# Development Container

`scripts/dev.sh` runs every check in a container with the same tools and versions as CI, so
"works on my machine" and "works in CI" are the same thing. It needs podman or docker; the image is
built from `Containerfile` on first use (a few minutes) and rebuilt when `Containerfile` changes.

```bash
scripts/dev.sh all        # everything below; ends with "dev.sh: all checks passed"
scripts/dev.sh test       # native unit tests
scripts/dev.sh check      # cppcheck (no defects allowed)
scripts/dev.sh build      # firmware + LittleFS image (size, secret and licence guards run here)
scripts/dev.sh lint       # ruff, clang-format on changed lines, CHANGELOG section for FW_VERSION
scripts/dev.sh format     # apply clang-format (CI's version) to the lines you changed
scripts/dev.sh manual     # PDF manual -> release/QRPickle_Documentation_<version>-container.pdf
scripts/dev.sh pages      # GitHub Pages site (Cloud OTA + installer) -> release/site
scripts/dev.sh shell      # a shell inside the container
scripts/dev.sh --ubuntu 24.04 all   # the same on another Ubuntu (default 26.04, like CI)
```

What's inside (pinned in `Containerfile`, the same as in `.github/workflows/`): Ubuntu 26.04,
PlatformIO 6.2.0, clang-format 19.1.7, ruff 0.16.10, Typst 0.15.1, and Ubuntu's pandoc and
ImageMagick (the versions the CI runner installs).

Notes:
- Build output and PlatformIO's packages live in podman/docker volumes
  (`qrpickle-workspace3-<ubuntu>`, `qrpickle-core3-<ubuntu>`; the volumes without the 3 hold core 2 builds of v0.2.x), not in the source tree, so the
  container never mixes with a local `pio` build. Remove them to start clean:
  `podman volume rm qrpickle-workspace3-26.04 qrpickle-core3-26.04`.
- The source is mounted with `:z` (SELinux shared label). With `:Z` every run gets a private
  label and files copied into the cache volume become unreadable to the next run.
- Flashing and device tests run on the host (USB): `pio run -t upload`, `tools/device_check.py`,
  `tools/web_security_check.sh`, `tools/webui_e2e/`.
- Verified on 2026-10-10: `all` passes on Ubuntu 24.04 and 26.04 and both produce the same
  firmware SHA-256 as each other.
- CI moved to the `ubuntu-26.04` runner in v0.2.3 (GitHub's `ubuntu-latest` switches from 19 October 2026).

## Building without the container (Arduino core 3)

Since v0.3.0 the firmware builds on Arduino core 3.3.12 / ESP-IDF 5.5 through the pioarduino
platform (pinned in `platformio.ini`). Three things differ from the old espressif32 platform:

- **Python 3.14 or older.** pioarduino doesn't run on 3.15 yet. A venv with
  `pip install "platformio==6.2.0"` is enough.
- **PlatformIO 6.2.0 in pioarduino's own environment.** pioarduino creates `~/.platformio/penv`
  with a PlatformIO version it then rejects (`IncompatiblePlatform ... 6.2.0-post.1`). Once, after
  `pio pkg install -e cyd`: `~/.platformio/penv/bin/uv pip install -p ~/.platformio/penv/bin/python "platformio==6.2.0"`.
  CI and `scripts/dev.sh` do this for you.
- **The first build rebuilds the framework** (~7 minutes) because `custom_sdkconfig` changes it,
  and writes `.dummy/`, `managed_components/`, `sdkconfig.*`, `CMakeLists.txt` and
  `dependencies.lock` into the project root (gitignored). A build after a settings change may
  stop once with `FileNotFoundError: .pio/build/cyd`; run it again.

- **`setTimeout()` on a network client takes milliseconds on core 3** (it falls through to
  `Stream::setTimeout()`); core 2's `WiFiClient::setTimeout()` took seconds. A leftover
  `setTimeout(10)` made the SOTA login wait 10 ms (found by the v0.3.0 stress run).

A PlatformIO folder shared with older projects on core 2 can break their toolchain; use a separate
one if you need both (`PLATFORMIO_CORE_DIR=/path/to/other pio run`).
