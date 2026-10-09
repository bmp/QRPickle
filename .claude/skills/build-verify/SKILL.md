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
   - Compare against the last known value (83.1% on 2026-10-09, v0.2.0). Flag any jump of more than 2 points and name the likely cause.

3b. **Lint:** `ruff check` (Python) and `scripts/check_format.sh` (C/C++ lines changed vs `origin/main`), with the pinned tools (`clang-format==19.1.7`, `ruff==0.16.10`; set `CLANG_FORMAT`). Format only changed lines (`git clang-format`); never reformat whole files.

4. **Filesystem** (only if `data/` changed):
   `pio run -t buildfs`. LittleFS is 448KB.

5. **Native tests:** `pio test -e native`
   - Read the summary line. **`0 test cases` means nothing was tested. Report "no tests exist", not "tests pass."**
   - If tests were collected, every one must succeed.

6. **Secret scan:** `scripts/check_secrets.py` runs automatically before every build and fails it on credential literals. Confirm the build log shows `[check_secrets] OK`.

7. **Build artifacts:** `release/` is gitignored, so builds no longer dirty the tree. Never commit binaries; CI publishes them.

## What the hardware is still needed for

A successful compile does not prove runtime behaviour on the CYD. State explicitly what still needs testing on the device:
- heap and out-of-memory behaviour during HTTPS/TLS fetches and OTA,
- touch input and display output,
- LED states.

Never flash the board yourself (`pio run -t upload` asks for permission); ask the user to do it.

## Report format

```
Build:   SUCCESS  (Flash 83.3% | static DRAM headroom 64.6 KB)
Static:  no defects (cppcheck)
Lint:    ruff OK, format OK (changed lines)
FS:      skipped (data/ unchanged)
Tests:   29/29 passed
Secrets: clean
Needs on-device check: <list or "none">
```

## Common mistakes

| Mistake | Reality |
|---|---|
| "Tests pass" when `0 test cases` were collected | Nothing ran. Say so. |
| Committing binaries | Never; `release/` is ignored and CI publishes release binaries. |
| Running `buildfs` only, after editing C++ | The `data/` and firmware builds are independent; run whichever ones the change touches. |
| Claiming an OTA or memory fix works because it compiles | Compiling proves nothing at runtime. List it as needing an on-device check. |
