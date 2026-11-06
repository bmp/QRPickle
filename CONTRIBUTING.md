# Contributing to QRPickle

Thanks for helping. Bug reports, ideas, documentation fixes and code are all welcome.
Please follow the [Code of Conduct](CODE_OF_CONDUCT.md). Security problems go through
[SECURITY.md](SECURITY.md), not public issues.

## Reporting a bug or asking for a feature

Open an [issue](https://github.com/bmp/QRPickle/issues/new/choose) and pick the template. For a
bug, the firmware version and the `mem` part of `http://<device IP>/api/status` help a lot:
most crashes on this board are about memory.

## Changing the code

1. Fork the repository and branch from `main`.
2. Set up the tools (PlatformIO, and for the checks the pinned clang-format and ruff):
   [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md). `scripts/dev.sh all` runs every CI check in a
   container.
3. Build and test:
   ```bash
   pio run                 # firmware
   pio test -e native      # host unit tests
   scripts/dev.sh all      # everything CI runs
   ```
4. Test on a CYD if you can ([docs/TESTING.md](docs/TESTING.md)), and say what you tested in the
   pull request.
5. Open a pull request against `main`; the template has the checklist.

## Rules that keep the firmware working

The board has no PSRAM and about 70 KB of free heap, so:

- Read [docs/MEMORY.md](docs/MEMORY.md) before adding a network feature. HTTPS needs ~40 KB;
  downloads run one at a time on the network worker and take `NetLock`.
- Never call `lv_*` from a background task; LVGL runs only in the main loop.
- Use `services::connect_host()` for DNS, never `WiFi.hostByName()`.
- No credentials in the source: everything is entered at runtime (a pre-build check enforces it).
- Format only the lines you change (`scripts/dev.sh format`); don't reformat whole files.
- New libraries, fonts or images need an entry in `THIRD_PARTY_NOTICES.md` (a pre-build check
  enforces it).
- New or changed screens: follow [docs/UI_GUIDE.md](docs/UI_GUIDE.md) and add screenshots.
- New features: update the README's feature table, `docs/SETUP.md` and the changelog.

## Licence

By contributing you agree that your work is released under the project's MIT licence
([`License`](License)).
