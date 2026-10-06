# SOTA Spots via the SOTA Cluster Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the deprecated SOTA REST endpoint (review 3.13) with a telnet client for `cluster.sota.org.uk:7300`, keeping the xOTA screen's `SotaManager` interface unchanged.

**Architecture:**
- **Parser:** a dependency-free parser (`sota_cluster_parse.{h,cpp}`) turns one `DX de ...` line into a spot. It is unit-tested on the host (`pio test -e native`).
- **Manager:** `SotaManager` runs a FreeRTOS task that connects, answers `login:` with the configured callsign, and feeds lines to the parser. It keeps the newest 30 spots, one entry per activator, with the newest first. Lifecycle uses the `task_alive`/`is_stopped()` pattern from Plan 1.
- **UI:** the connection runs only while the xOTA SOTA tab is active. The cluster sends a backlog on login, so the list fills immediately. Switching to POTA or leaving xOTA stops the connection, and POTA's TLS fetch waits until it has exited.

**Tech Stack:** ESP32 Arduino `WiFiClient`, FreeRTOS, Unity (native tests), PlatformIO 6.2.

**Cluster facts** (observed 2026-10-06):
- The banner is `login:` with no newline. Answer with the callsign; no password.
- The cluster sends about 30 backlog spots right after login.
- Line format: `DX de K1GC:      14054.0  K1GC         W1/DI-009                      1704Z` (spotter, kHz, activator, summit, HHMMZ). There is no mode field.
- Keepalives must be at least 15 minutes apart. We send none; reconnecting on drop is enough.
- The terms of `api-db2.sota.org.uk` do not obviously cover the cluster. The owner will mention this use to SOTA.

---

### Task 1: Native test setup + failing parser tests

**Files:**
- Modify: `platformio.ini` (`[env:native]`)
- Create: `test/test_parsers/test_sota_cluster.cpp`

- [x] **Step 1:** In `[env:native]`, add:
```ini
test_build_src  = yes
build_src_filter = -<*> +<services/sota_cluster_parse.cpp>
```
- [x] **Step 2:** Create the test file (full content in the commit; the cases are listed here):
  - A standard spot → spotter `K1GC`, activator `K1GC`, summit `W1/DI-009`, time `17:04`, 14.054 MHz, mode `CW`.
  - A VHF spot `146520.0` → 146.52 MHz, mode `FM`.
  - A spotter and frequency run together (`DX de KG7LBY-#:14325.0 ...`) → spotter `KG7LBY-#`, 14.325, mode `SSB`.
  - A comment between the summit and the time → summit, comment and time all parsed.
  - A mode word in the comment (`FT8`) overrides the frequency guess.
  - An `RBNHOLE` spotter on a frequency that looks like SSB → mode `CW`.
  - Rejections: `login: `, the cluster prompt line, an empty string, a bad frequency, a line with no summit reference.
  - `is_summit_ref`: `W0C/SR-046`, `3B8/MU-001` and `G/LD-001` are accepted; `K1GC`, `W1/DI-09`, `W1DI-009` and `/DI-009` are rejected.
- [x] **Step 3:** Run `pio test -e native -f test_parsers`. Expected: **build failure** (the header doesn't exist yet).

### Task 2: Implement the parser

**Files:** Create `src/services/sota_cluster_parse.h` and `src/services/sota_cluster_parse.cpp`

- [x] **Step 1:** Implement:
  - `ParsedSpot {spotter[16], activator[16], summit[16], time[6], comment[48], mode[8], freq_mhz}`.
  - `bool is_summit_ref(const char*)`.
  - `const char* mode_for_freq(float mhz)`:
    - FT8 within ±3kHz of the standard dial frequencies.
    - CW segments: 1.800–1.840, 3.500–3.600, 7.000–7.070, 10.100–10.150, 14.000–14.070, 18.068–18.095, 21.000–21.070, 24.890–24.915, 28.000–28.070, 50.000–50.100, 144.000–144.150.
    - FM: 144.5–148 and ≥ 430.
    - Otherwise SSB.
  - `bool parse_line(const char*, ParsedSpot&)`. Mode precedence: a mode word in the comment, then an `RBNHOLE` spotter (→ CW), then `mode_for_freq`.
- [x] **Step 2:** Run `pio test -e native -f test_parsers`. Expected: all tests **PASS**.
- [x] **Step 3:** Commit: `feat: SOTA cluster line parser with native unit tests`

### Task 3: Replace `SotaManager` with the cluster client

**Files:** Modify `src/services/sota_manager.h`; rewrite `src/services/sota_manager.cpp`

- [x] **Step 1:** Header:
  - Add `start()`, `stop()` and `is_stopped()`, plus the private `running`, `task_loop()` and `store_spot()`.
  - Remove `deduce_mode()`.
  - Keep `fetch_async()` (it now calls `start()`), `get_spots()`, `get_spot_count()`, `is_dirty()`/`clear_dirty()`, `get_last_fetch_time()` and `expire_timer()`.
- [x] **Step 2:** The `.cpp`:
  - Remove all HTTPS/JSON code.
  - The task: connect with a 5s connect timeout and a 10s wait for `login:`, then send the callsign; read lines into the parser and call `store_spot()`. If the cluster is unreachable, retry every 30s. Set `last_fetch_time = millis()` while connected so the UI doesn't treat live data as stale. Clear `task_alive` on exit.
  - `store_spot()`: an upsert by activator under a `portMUX` critical section. Newest goes first; when full, the oldest is dropped.
  - Skip login if the callsign is unset or `N0CALL`.
- [x] **Step 3:** Run `pio run` → `[SUCCESS]`; `pio check` → no defects.

### Task 4: Wire up the xOTA lifecycle

**Files:** Modify `src/ui/screens/xota.cpp`

- [x] **Step 1:** `set_tab()`: when switching to `TAB_POTA`, call `services::SotaManager::stop()`.
- [x] **Step 2:** `execute_delayed_fetch()`:
  - The POTA branch waits for HamAlert, APRS **and SOTA** to be stopped before the TLS fetch.
  - The SOTA branch calls `fetch_async()` without waiting (there's no TLS).
- [x] **Step 3:** The `LV_EVENT_DELETE` handler calls `services::SotaManager::stop()`.
- [x] **Step 4:** Build and check. Then commit: `feat: SOTA spots from SOTA cluster telnet (replaces deprecated API)`

### Task 5: Device test

- [x] On the SOTA tab, the log shows `[SOTA] Logged in to SOTA cluster.` and the list fills with real summits (no "DEPRECATED"). The mode column shows CW/SSB/FM/FT8.
- [x] Switching to POTA shows `[SOTA] Cluster session closed.` *before* `[POTA] Synchronous main-thread fetch started.`
- [x] Leaving xOTA closes the session. Rapid in and out causes no crash.
- [x] Re-run the reboot loop for 10 cycles. The watchdog rate must not get worse (baseline 2–3/10, finding 3.14).

### Task 6: Update the review
- [x] Update 3.13 with the fix commit and the device-test result. Commit.
