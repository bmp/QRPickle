# Debugging QRPickle

## Log levels

Two kinds of serial output exist:

- **App logs**, such as `[Wi-Fi]`, `[SOTA]`, `[OTA Guard]`, `[CRASHLOG]` and `[Config]`. These are plain `Serial.print` calls and are **always on**, in every build.
- **Framework logs** (`[  1234][D][HTTPClient.cpp:303] ...`) come from Arduino-ESP32 and are controlled by `CORE_DEBUG_LEVEL`:

| Level | Name | Shows |
|---|---|---|
| 0 | None | nothing |
| **1** | **Error** | errors only. **Release builds (`cyd`) use this.** |
| 2 | Warn | + warnings |
| 3 | Info | + info |
| **4** | **Debug** | + HTTP requests, WiFi events, TLS details. **`cyd-debug` uses this.** |
| 5 | Verbose | everything (very noisy; can slow the device) |

### Which build to use

| Environment | Command | Level | Use |
|---|---|---|---|
| `cyd` (default) | `pio run -t upload` | 1 | Normal use and **all releases** |
| `cyd-debug` | `pio run -e cyd-debug -t upload` | 4 | Chasing network, WiFi or HTTP problems |

`cyd-debug` is about 14KB larger (84.0% vs 83.3% of the OTA slot). The size guard still applies.

> **Do not share level-4 logs publicly.** At level 4, HTTPClient prints full request URLs, which included the **OpenWeather API key** (review 3.5b). Mask the key before pasting a log anywhere, or reproduce the problem with the `cyd` build. Never release a `cyd-debug` build.

### Changing the level

The level is set **once per environment** in `platformio.ini`; the shared flags live in `[common]`:
```ini
[env:cyd]
build_flags = ${common.build_flags}
    -DCORE_DEBUG_LEVEL=1
```
Don't try to override it with `-UCORE_DEBUG_LEVEL -DCORE_DEBUG_LEVEL=4` in an `extends`-ed environment. PlatformIO keeps the **first** definition of a duplicated `-D` macro, so the override is silently ignored. That happened on 2026-10-07 and was caught with a compile-time probe.

For another level (e.g. 3 = Info), temporarily change the number in `[env:cyd-debug]`. A `PLATFORMIO_BUILD_FLAGS=-DCORE_DEBUG_LEVEL=…` override does **not** work, for the same first-definition reason.

## Reading the serial log

```bash
pio device monitor -b 115200 -f esp32_exception_decoder
```
The exception decoder turns crash backtraces into file:line. Exit with Ctrl+C. Close the monitor before flashing (the port can only be open once).

## After a crash or freeze

Every boot after an **abnormal reset** (watchdog, panic, brownout) prints the last step each task reached:
```
[CRASHLOG] previous reset reason=5, last steps:
[CRASHLOG]   loop     step=7 t=11585 ms core=1
[CRASHLOG]   aprs     step=2 t=6435 ms core=0
...
```
- **Reset reasons** (`esp_reset_reason_t`): 4 = panic, 5 = interrupt watchdog, 6 = task watchdog, 7 = other watchdog, 9 = brownout.
- **Steps:** the numbers are `crashlog::mark(slot, step)` calls in the code; search for the slot name (e.g. `SLOT_APRS`) to see what each step means. Steps 101–105 come from `connect_host()` (101 DNS start, 102 DNS failed, 103 connecting, 104 connected, 105 connect failed).
- **Adding a breadcrumb** is cheap: `crashlog::mark(crashlog::SLOT_LOOP, 42);` (`src/core/crashlog.h`).

This is how the boot freeze (review 3.14) was traced to a non-thread-safe DNS call even though the crash printed nothing.

### Hangs (loop watchdog)

`loop()` runs under the task watchdog with a 30s timeout (`enableLoopWDT()` in `setup()`). A hang reboots the device and prints `Task watchdog got triggered` plus a backtrace. Decode it with:
```bash
~/.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-addr2line -pfiaC -e .pio/build/cyd/firmware.elf 0x400d1234 0x400d5678 ...
```
(or monitor with `-f esp32_exception_decoder`). Keep any single piece of loop work well under 30s.

**Don't** try to read the breadcrumbs from a hung device with `esptool dump_mem`: entering download mode cleared that RTC memory in testing. Let the watchdog reboot the device and read `[CRASHLOG]` instead.

## Automated device checks

| Tool | What it checks | Pass |
|---|---|---|
| `~/.platformio/penv/bin/python -I tools/device_check.py --cycles 10` | Resets the board N times over USB; per boot: WiFi, NTP, watchdog resets, panics, brownouts | `PASS`, `wdt=0` |
| `QRP_ADMIN_PW=<pw> tools/web_security_check.sh <ip>` | Auth on all routes, no secrets in the API, crash payloads, path traversal, CSP | `pass=17 fail=0` |
| `~/.platformio/penv/bin/python -I tools/serial_soak.py --hours 0.5` | Timestamped serial capture for rare freezes; echoes resets, panics, watchdog and `[CRASHLOG]` lines. Opening the port may reset the board once, so start it before opening the screen under test | `0 alert lines` |

The device's IP and web password are printed at boot (`[Wi-Fi] Network Link Stable! ... IP:` and `Web console login: admin / ...`); the IP can change with DHCP.

## Memory

- **Static RAM** (globals) and **heap** are separate. PlatformIO's "RAM %" does not show the static-RAM limit. The build guard (`scripts/check_size.py`) prints both and fails under 4KB of static headroom.
- For the biggest static users:
  ```bash
  PLATFORMIO_BUILD_FLAGS='-Wl,-Map,${BUILD_DIR}/firmware.map' pio run
  python3 -I tools/dram_report.py .pio/build/cyd/firmware.map
  ```
- After editing `include/lv_conf.h`, run `pio run -t clean` first, because LVGL isn't rebuilt otherwise.

## Static checks before committing

```bash
pio test -e native                                     # unit tests (host)
pio check -e cyd --severity=high --severity=medium     # cppcheck, expect "No defects found"
```
The `build-verify` skill (`.claude/skills/build-verify/`) runs the full checklist.

## Test-only builds

`-DQRP_TEST_CRASH_AT_BOOT` makes the firmware abort 3s after boot. It exists only to prove the OTA rollback guard (see `docs/RELEASING.md`). **Never release it.**
