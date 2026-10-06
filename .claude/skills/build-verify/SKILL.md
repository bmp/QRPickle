---
name: build-verify
description: Use when about to commit, claim a change works, or hand off work in QRPickle. Also use after editing anything in src/, include/, data/, partitions.csv, platformio.ini or lib_deps, or when asked whether a change breaks the firmware build.
---

# Build Verify (QRPickle)

Prove the firmware still builds and fits before saying it works. The evidence is real command output, not "it should compile."

## Steps

Run every step that applies and report each result.

1. **Firmware build** (any change outside `docs/`):
   `pio run 2>&1 | tail -30`
   It must end in `SUCCESS`. Record the `Flash: ... %` and `RAM: ... %` lines.

2. **Static analysis** (any change in `src/`):
   `pio check -e cyd --severity=high --severity=medium`
   The expected output is `No defects found`. **Any finding in `src/` blocks the commit.** Fix it; don't suppress it without asking.

3. **Size budget.** The app slot is 1,835,008 bytes (`0x1C0000`).
   - Flash ≥ 95%: **warn**. OTA will fail once the image no longer fits the slot.
   - Compare against the last known value (86.9% at v0.1.11). Flag any jump of more than 2 points and name the likely cause.

4. **Filesystem** (only if `data/` changed):
   `pio run -t buildfs`. LittleFS is 448KB.

5. **Native tests:** `pio test -e native -f test_parsers -f test_config` (drop the `-f` flags once docs/patches/0001 is applied)
   - Read the summary line. **`0 test cases` means nothing was tested. Report "no tests exist", not "tests pass."**
   - If tests were collected, every one must succeed.

6. **Secret scan of the diff.** `scripts/check_secrets.py` is empty, so do this by hand:
   `git diff HEAD | grep -nEi '(api[_-]?key|passw\w*|secret\w*|token\w*|passcode)\s*[:=]\s*"[^"]{6,}"'`
   Any hit blocks the commit. Show it with the value masked.

7. **Restore build artifacts.** Builds overwrite the tracked `release/*.bin` files.
   `git checkout -- release/`
   Only skip this when the user is deliberately cutting a release.

## What the hardware is still needed for

A successful compile does not prove runtime behaviour on the CYD. State explicitly what still needs testing on the device:
- heap and out-of-memory behaviour during HTTPS/TLS fetches and OTA,
- touch input and display output,
- LED states.

Never flash the board yourself (`pio run -t upload` asks for permission); ask the user to do it.

## Report format

```
Build:   SUCCESS  (Flash 86.9% | RAM 38.0%)
Static:  no defects (cppcheck)
FS:      skipped (data/ unchanged)
Tests:   none exist (0 test cases)
Secrets: clean
release/: restored
Needs on-device check: <list or "none">
```

## Common mistakes

| Mistake | Reality |
|---|---|
| "Tests pass" when `0 test cases` were collected | Nothing ran. Say so. |
| Committing `release/*.bin` from a dev build | Restore it; CI builds the release binaries. |
| Running `buildfs` only, after editing C++ | The `data/` and firmware builds are independent; run whichever ones the change touches. |
| Claiming an OTA or memory fix works because it compiles | Compiling proves nothing at runtime. List it as needing an on-device check. |
