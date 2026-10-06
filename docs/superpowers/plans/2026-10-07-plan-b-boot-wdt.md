# Plan B: Boot Watchdog Freeze (review 3.14)

Systematic debugging; each step was measured with `tools/device_check.py`.

- [x] Baseline: 10/49 boots reset with a silent `TG1WDT` at about 11.5s uptime.
- [x] Instrument: `src/core/crashlog` breadcrumbs in RTC no-init memory, printed after abnormal resets.
- [x] Finding: both cores were healthy until about 11.55s, so the hang began then; the LED task was separately stuck from 0.56s (5.7).
- [x] Hypothesis "debug logging": rejected (`CORE_DEBUG_LEVEL=1` still gave 2/12).
- [x] Hypothesis "APRS hostname connect": per-caller breadcrumbs showed APRS's `hostByName` failing at about 6.5s, with the crash 5.0s later in every case.
- [x] Root cause: the Arduino 2.0.17 `hostByName` isn't thread-safe (lwIP call without the core lock, stack callback target).
- [x] Fix: `services::connect_host()` using `getaddrinfo()`, used by APRS, HamAlert, SOTA and DX.
- [x] Verify: 30/30 boots clean.
