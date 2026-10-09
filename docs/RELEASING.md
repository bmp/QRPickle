# Releasing a New Firmware Version

Releases are created by GitHub Actions (`.github/workflows/release.yml`) **only when you push a version tag**. Pushing ordinary commits to `main` never publishes anything.

Devices on the field pick up new releases through **Cloud OTA** (sidebar menu → **Cloud OTA** → ↻ to check → **INITIATE FIRMWARE FLASH**), so every tag you push ships firmware to users.

## One-time GitHub setup (before the first Pages release)

Releases also publish to **GitHub Pages**: Cloud OTA files under `/ota/` and the browser installer under `/install/` (https://bmp.github.io/QRPickle/install/).
1. Repository **Settings → Pages → Build and deployment → Source: GitHub Actions**.
2. **Settings → Environments → github-pages → Deployment branches and tags → Add rule → Tag, pattern `v*`.** By default only the default branch may deploy, and releases run from tags.

## Version numbers

- The version lives in one place: `FW_VERSION` in `src/core/metadata.h`, e.g. `"v0.1.12"`.
- The format is `vMAJOR.MINOR.PATCH`. Devices compare these numerically and only offer **newer** versions, so `v0.1.10` counts as newer than `v0.1.9`.
- The git tag must be **exactly** the same string as `FW_VERSION`. CI refuses to release if they differ.

| Change | Bump | Example |
|---|---|---|
| Bug fixes only | PATCH | `v0.1.11` → `v0.1.12` |
| New features, settings stay compatible | MINOR | `v0.1.12` → `v0.2.0` |
| Breaking change (partition table, NVS keys, flashing procedure) | MAJOR | `v0.2.0` → `v1.0.0` |

**Never change `partitions.csv` in a Cloud OTA release.** OTA does not rewrite the partition table, so deployed devices would break. Partition changes need a USB flash and a MAJOR bump, with clear release notes.

## Step by step

### 1. Verify on hardware first
On your release branch:
```bash
pio test -e native                 # unit tests
pio check -e cyd --severity=high --severity=medium   # expect "No defects found"
pio run && pio run -t buildfs      # check_size / check_secrets / check_licenses guards run here
pio run -t upload && pio run -t uploadfs
~/.platformio/penv/bin/python -I tools/device_check.py --cycles 10   # expect PASS, wdt=0
```
If the web console changed, also run `QRP_ADMIN_PW=<pw> tools/web_security_check.sh <device-ip>` (expect 17/17).

If the web console's look changed, regenerate the screenshots in `docs/screenshots/` (see `tools/webui_screenshots/README.md`) after the version bump, so they show the new version.

The release build must **not** use the `cyd-debug` environment or `-DQRP_TEST_CRASH_AT_BOOT`.

### 2. Bump the version
Edit `src/core/metadata.h`:
```cpp
constexpr const char* FW_VERSION = "v0.1.12";
```
Commit it. **The commit message becomes the release notes**, so write it for users:
```bash
git commit -am "Release v0.1.12: SOTA spots from the SOTA cluster, faster boot

- SOTA spots work again (the old SOTA API was retired)
- Fixes occasional freeze ~12 s after power-on
- Web console now asks for a login (password on the Network screen)"
```

### 3. Merge to `main`, then tag and push
```bash
git checkout main
git merge --ff-only <your-branch>
git tag v0.1.12
git push origin main v0.1.12
```
Only the **tag push** starts the release.

### 4. Watch CI
On GitHub → **Actions** → "Auto-Build and Release":
1. Checks that the tag matches `FW_VERSION`.
2. Runs the unit tests.
3. Builds the firmware and filesystem.
4. Builds the PDF manual.
5. Publishes the release with these files:
   - `firmware.bin`
   - `firmware.bin.sha256`
   - `littlefs.bin`
   - a ZIP containing everything, including `bootloader.bin` and `partitions.bin`
6. Publishes **GitHub Pages**: `ota/ota.json` + `ota/firmware.bin` (what devices update from) and the `install/` page (built by `scripts/make_pages_site.py`).

### 5. Verify the release on a device
On a CYD running the previous version, open the sidebar menu → **Cloud OTA**, tap ↻, then **INITIATE FIRMWARE FLASH**. On the serial log, expect:
```
[OTA Worker] SHA-256 verified.
[OTA Guard] Armed: 3 trial boots, fallback slot app0
[OTA Guard] Trial boot of new image, 2 attempt(s) left after this one
[OTA Guard] New image healthy (60 s with WiFi); rollback disarmed.
```
The **"New image healthy"** line has to appear. If the new firmware can't run for 60 seconds with WiFi within 3 boots, the device **rolls back to the previous version automatically**.

## What devices do with a release

| Step | Behaviour |
|---|---|
| Check | Reads `https://bmp.github.io/QRPickle/ota/ota.json` (with HamAlert/APRS paused for memory); offers it only if newer and it has a valid SHA-256 |
| Download | Streams `firmware.bin` into the inactive app slot |
| Integrity | Compares against the SHA-256 in `ota.json`; refuses on mismatch, and never flashes a release without one |
| Safety net | Trial-boot guard: no healthy boot within 3 tries → previous version restored |

Cloud OTA updates **firmware only**. If `data/` changed (web console files), users must also upload `littlefs.bin` (web console → **System Info** → Wireless Maintenance → filesystem target), or flash it by USB. Writing the filesystem **erases saved profiles**, so tell users to download a backup first (Profiles → Download Backup). Say both in the release notes.

## When something goes wrong

**CI says "Tag vX != FW_VERSION vY":** you tagged before bumping, or the strings differ. Delete the tag, fix it, re-tag:
```bash
git tag -d v0.1.12
git push origin :refs/tags/v0.1.12
# fix metadata.h, commit, then tag and push again
```

**A bad release is already out:**
- Devices that couldn't boot it have already rolled back by themselves.
- To stop new installs, publish a fixed **higher** version (e.g. `v0.1.13`). Devices never install an older version, so don't re-release the old number.
- Don't overwrite or force-push tags. The workflow intentionally can't.

**A test build reached a device:** reflash over USB (`pio run -t upload`), or upload a good `firmware.bin` through the web console.

## Users without Cloud OTA

**Easiest:** open https://bmp.github.io/QRPickle/install/ in Chrome or Edge, connect the CYD by USB and click **Install** (ESP Web Tools). This is a full install, so settings are erased.

Pre-built files are also attached to every GitHub release. The USB and web-flasher instructions in the README ("Easy Web Installation") use these offsets:

| File | Offset |
|---|---|
| `bootloader.bin` | `0x1000` |
| `partitions.bin` | `0x8000` |
| `firmware.bin` | `0x10000` |
| `littlefs.bin` | `0x390000` |
