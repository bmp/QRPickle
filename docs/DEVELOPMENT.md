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
scripts/dev.sh manual     # PDF manual -> release/QRPickle_Documentation_<version>-container.pdf
scripts/dev.sh pages      # GitHub Pages site (Cloud OTA + installer) -> release/site
scripts/dev.sh shell      # a shell inside the container
scripts/dev.sh --ubuntu 26.04 all   # the same on another Ubuntu (default 24.04, like CI)
```

What's inside (pinned in `Containerfile`, the same as in `.github/workflows/`): Ubuntu 24.04,
PlatformIO 6.2.0, clang-format 19.1.7, ruff 0.16.10, Typst 0.15.1, and Ubuntu's pandoc and
ImageMagick (the versions the CI runner installs).

Notes:
- Build output and PlatformIO's packages live in podman/docker volumes
  (`qrpickle-workspace-<ubuntu>`, `qrpickle-core-<ubuntu>`), not in the source tree, so the
  container never mixes with a local `pio` build. Remove them to start clean:
  `podman volume rm qrpickle-workspace-24.04 qrpickle-core-24.04`.
- The source is mounted with `:z` (SELinux shared label). With `:Z` every run gets a private
  label and files copied into the cache volume become unreadable to the next run.
- Flashing and device tests run on the host (USB): `pio run -t upload`, `tools/device_check.py`,
  `tools/web_security_check.sh`, `tools/webui_e2e/`.
- Verified on 2026-10-10: `all` passes on Ubuntu 24.04 and 26.04 and both produce the same
  firmware SHA-256 as each other.
