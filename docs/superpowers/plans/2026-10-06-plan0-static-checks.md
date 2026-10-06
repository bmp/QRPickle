# Static Checks Baseline (Plan 0) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `pio check` (cppcheck) a clean, trusted gate for `src/`; fix the 3 bugs it found; record a compiler-warning baseline; and add the check to the `build-verify` skill. This must run before Plan 1, so that Plan 1 can be checked for *new* findings.

**Architecture:** Configure cppcheck in `platformio.ini` (skip framework packages and suppress the known ArduinoJson macro false positive), fix the findings in `src/`, and measure `-Wall -Wextra` warnings once via `PLATFORMIO_BUILD_FLAGS`, without changing the build flags.

**Tech Stack:** PlatformIO 6.2 `pio check` (cppcheck), GCC 8.4 xtensa.

---

### Task 1: Configure `pio check`

**Files:** Modify `platformio.ini` (the `[env:cyd]` section).

- [ ] **Step 1:** Add the following after `extra_scripts`:
```ini
; Static analysis: `pio check -e cyd`. Framework packages are skipped; ArduinoJson's
; namespace macros are a known cppcheck false positive (preprocessorErrorDirective).
check_tool = cppcheck
check_skip_packages = yes
check_flags =
    cppcheck: --suppress=preprocessorErrorDirective:*ArduinoJson*
```
- [ ] **Step 2:** Run `pio check -e cyd --severity=high --severity=medium`. Expected: only the 4 `src/` findings remain (`widget_wx_all.cpp:98`, `aprs_hub.cpp:179` ×2, `aprs_hub.cpp:194`).
- [ ] **Step 3:** Commit: `git commit -am "build: configure cppcheck static analysis"`

### Task 2: Fix the stray `%` in the weather widget placeholder

**Files:** Modify `src/ui/widgets/widget_wx_all.cpp:98`

- [ ] **Step 1:** Replace `snprintf(buf, sizeof(buf), "-- °C\n-- %\n-- hPa");` with `snprintf(buf, sizeof(buf), "-- °C\n-- %%\n-- hPa");`
- [ ] **Step 2:** `pio check` shows no finding for this file.

### Task 3: Fix the APRS icon label and the countdown format

**Files:** Modify `src/ui/screens/aprs_hub.cpp:179` and `src/ui/screens/aprs_hub.cpp:194`

- [ ] **Step 1:** Line 179: change `%u mins %u secs` to `%d mins %d secs` (`remaining` is an `int`, already clamped at 0).
- [ ] **Step 2:** Line 194: replace `"MAP ICON: %s (\x25%s)"` with `"MAP ICON: %s (%s)"`. `\x25` is a literal `%`, which turned `%s` into `%%s`, so the label showed a literal "(%s)" instead of the icon code.
- [ ] **Step 3:** Run `pio check -e cyd --severity=high --severity=medium`. Expected: **0** findings. Then run `pio run`. Expected: `[SUCCESS]`.
- [ ] **Step 4:** Commit: `git commit -am "fix: printf format bugs found by cppcheck (wx placeholder, APRS icon label)"`

### Task 4: Record the compiler-warning baseline

- [ ] **Step 1:** `pio run -t clean`, then:
  `PLATFORMIO_BUILD_FLAGS="-Wall -Wextra" pio run 2>&1 | grep -E '^src/.*warning:' | sort -u > /tmp/qrp-warnings.txt; wc -l < /tmp/qrp-warnings.txt`
- [ ] **Step 2:** Group the results by warning flag (`grep -oE '\[-W[^]]+\]' | sort | uniq -c`) and record the counts in the review doc (area 6, "Warning baseline"). Then `git checkout -- release/`.
- [ ] **Step 3:** Commit the review doc update.

### Task 5: Add static analysis to the `build-verify` skill

**Files:** Modify `.claude/skills/build-verify/SKILL.md`

- [ ] **Step 1:** Add a step after the firmware build: `pio check -e cyd --severity=high --severity=medium --skip-packages`. **Any finding in `src/` blocks the commit.** Add a `Static:` line to the report format.
- [ ] **Step 2:** Commit: `git commit -am "chore: build-verify runs cppcheck"`
