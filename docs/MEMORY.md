# Memory: Budgets and Rules

The CYD's ESP32 has about 300 KB of internal RAM and **no PSRAM**. Most crashes and failed HTTPS
fetches in QRPickle's history were memory problems, so every feature has to fit the budget below
and follow the rules at the end. Measured on a CYD with v0.2.4 on 2026-10-10.

## Where the memory is

```
 static DRAM (dram0_0_seg, 124,580 B)        heap (what's left, in regions)
 ┌──────────────────────────┬─────────────┐  ┌──────────────────────────────┐
 │ .data/.bss: ~60 KB       │ heap region │  │ 0x3FFE4350: ~111 KB, empty   │
 │ (globals, static buffers,│ ~80 KB:     │  │ at boot; WiFi, lwIP, tasks,  │
 │  display draw buffer)    │ LVGL's 64KB │  │ TLS, web server live here    │
 │                          │ pool + 13KB │  ├──────────────────────────────┤
 └──────────────────────────┴─────────────┘  │ smaller regions (5-25 KB)    │
                                             └──────────────────────────────┘
```

- **Static DRAM** holds globals. `scripts/check_size.py` fails the build if less than 4 KB stays
  free. Anything added here is taken from the heap region next to it, where LVGL's pool lives.
- **The heap** is split into regions; a block can't span two. The big one (~111 KB) starts empty
  and is shared by everything allocated at run time.
- **8-bit vs 32-bit heap.** Part of the internal RAM (IRAM) is heap that only allows 32-bit access.
  TLS buffers, strings and most data need byte access (`MALLOC_CAP_8BIT`). `ESP.getFreeHeap()` and
  `ESP.getMaxAllocHeap()` include the IRAM part and overstate what is usable: logs in v0.2.3 showed
  a "largest block 40948 B" that was an IRAM region, while the usable largest block was ~18 KB.
  Use `heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)`.

## Budget

| User | Size | Where | Notes |
|------|------|-------|-------|
| LVGL pool (`LV_MEM_SIZE`) | 48 KB (64 until v0.2.7) | heap, once at `lv_init()` | page peaks (v0.2.8): Weather 39.3, Settings 36.8, Band 36.1, APRS 33.9, xOTA 32.9 KB; the splash preview adds ~0.3 KB (it opens the dashboard under it on a heavy page) |
| LVGL image cache (`LV_CACHE_DEF_SIZE`) | up to 16 KB | inside the pool | decoded icons from LittleFS; 32 KB ran the pool out (crash) |
| LVGL layer buffer (`LV_DRAW_LAYER_SIMPLE_BUF_SIZE`) | 8 KB chunks | inside the pool | only for objects drawn through a layer (object opacity, transforms) |
| Display draw buffer | 12.8 KB | static | `320 x 10` RGB565 x 2 |
| TLS session (mbedTLS) | ~40 KB in total | heap | two ~16.7 KB buffers plus contexts; certificate checks add more |
| Task stacks | 2-12 KB each | heap | sized from measurements (below): APRS 4 KB, HamAlert 4 KB, AsyncTCP 8 KB, Cloud OTA check 12 KB, solar/POTA fetch 8 KB, DX dial/SOTA 4 KB, LED 2 KB, loopTask 8 KB |
| WiFi, lwIP, web server | varies | heap | buffers come and go with traffic |
| Satellites (`space_manager`) | ~3.3 KB static; ~15 KB while passes are computed | static, heap | the feed (~13 KB) lives in LittleFS (`/space.json`), read one object at a time |

Measured after 2 minutes of normal running in v0.2.4: about 98 KB free, the largest 8-bit block
swinging between 19 and 35 KB, the lowest free heap since boot 0.1-1.8 KB, and the Cloud OTA check
failing for memory. In v0.2.5 (tasks parked, stacks sized from measurements): about 114 KB free,
the largest block a steady 45 KB, the lowest since boot 4.4 KB, and the check passing 6 of 6
(2026-10-11).

Stack use, from `/api/status` → `mem.stack_free` (bytes never used, lowest seen):

| Task | Stack | Never used | Note |
|---|---|---|---|
| aprs_task | 4 KB (was 10) | ~1.8 KB | |
| hamalert_task | 4 KB | ~2.0 KB | 3 KB was too tight (review 3.9) |
| async_tcp (web server) | 8 KB (was 16) | ~4.8 KB | `CONFIG_ASYNC_TCP_STACK_SIZE` in platformio.ini |
| gh_ota_check | 12 KB | ~6.8 KB | with certificate verification |
| solar_fetch | 8 KB | ~4.2 KB | |
| loopTask | 8 KB | ~2.1 KB | LVGL and the UI |

## Rules

1. **No large static buffers.** Static DRAM is nearly full and takes from the heap region beside
   it. Allocate at init instead (the LVGL pool does).
2. **Long-lived tasks are created once and parked, never deleted** (HamAlert, APRS: on `stop()`
   they close the socket and wait; `start()` wakes them). Deleting and re-creating them moved their
   stacks around the heap on every stop. **Size every stack from `mem.stack_free`** with
   ~1.5-2 KB to spare, and add new tasks to `mem_stats` (`watch_stack` or `note_stack`).
3. **Allocate long-lived memory early, once.** Every allocation made at run time, and every task
   that is stopped and restarted, can land somewhere new and split the big region. Prefer one
   buffer kept for the device's lifetime over repeated allocate/free. Exception (v0.3.0): the POTA,
   SOTA and DX lists (3.4-3.8 KB each) are freed when no screen shows them and cycling doesn't come
   back to them (`services/list_release.h`): they held ~11 KB for good once opened. The POTA and
   SOTA lists are allocated only while no download runs (`net_lock_busy()`), so they don't land in
   the middle of a TLS session's space; the DX list is allocated when its page opens.
4. **The main loop never waits for NetLock** (`NetLock lock(0)`, retry later): a task download can
   hold it for 20 s or more, and the loop watchdog fires at 30 s (weather reset the board this way,
   2026-11-06).
4a. **One network session at a time:** `services::NetLock` (`net_lock.h`) around every connect or
   HTTP(S) request. Two TLS sessions never fit together.
5. **HamAlert and APRS stay connected during downloads** (since v0.2.8). Stopping them for each
   TLS session and reconnecting afterwards put their new socket buffers (1,436 B each plus lwIP
   records) in the middle of the space TLS had just freed, which split the largest block every
   time. The quiet window that did this (`services::quiet`) was removed in v0.3.0; xOTA had kept
   waiting for it and never loaded POTA when opened by hand (v0.2.8 bug).
6. **Check the 8-bit heap**, not `ESP.getMaxAllocHeap()` (above). Log the largest 8-bit block when a
   TLS request fails.
7. **Don't start heavy work in the first seconds after boot.** WiFi, NTP, the solar fetch and the
   telnet connects already crowd them. The automatic Cloud OTA check starts 2 minutes after boot
   and retries every 10 minutes until it succeeds.
8. **LVGL:** fade with `bg_opa`/`text_opa`, not object opacity on objects with children (that
   renders through a layer buffer). Show only the open tab's objects (`lv_obj_clean` on tab
   change). Keep the number of distinct images on a page small (the image cache is 16 KB).
9. **Measure before and after** a change that adds memory (below). The LVGL pool is sized from
   the highest page peak + 25 % + allocator overhead; a new or heavier page needs a new
   measurement (`mem.lvgl_page_peak`), and if it passes ~44 KB the pool must grow.
10. **Bulky data goes to LittleFS, not RAM.** Holding the satellite feed in RAM (10 KB) was enough
    to make the Cloud OTA certificate check fail on every boot (2026-10-12, A/B tested). Stream
    downloads to a file and keep only what screens show; read the rest on demand.
11. **Verify TLS certificates outside the handshake** when the chain needs RSA-4096 checks (Let's
   Encrypt's roots): inside it, the two 16 KB TLS buffers, the parsed CA bundle and the signature
   checks don't fit together and mbedTLS reports a failed signature, not "out of memory"
   (`services::TwoStepTlsClient`, Cloud OTA, 2026-11-04).
12. **The web server refuses rather than crashes when memory is low:** a middleware answers nothing
   (connection closed) when the 8-bit heap has less than 12 KB free or no 6 KB block
   (`mem::web_can_serve`, counted in `mem.web_refused`). An allocation failure inside the web
   library's Digest login aborted the device (xOTA, 2026-11-06).
13. **Measure downloads under cycling.** An HTTPS download takes ~52-57 KB while it runs (TLS,
   parsing, its task's stack). Cycling holds ~14 KB more than the dashboard alone: the POTA, SOTA
   and DX spot lists (3.4-3.8 KB each, kept once allocated) and the DX connection. With the LVGL
   pool at 56 KB, POTA and feed downloads failed for memory while cycling (0/5, 0/4, also on
   v0.2.7); at 48 KB, with POTA's stack 8 -> 6 KB, all succeeded in 2 x 30 min (2026-11-06).
   Judge memory changes by download success in that workload (`tools/cycle_run.sh`), on a normal
   build, from a fresh boot, over at least two runs; the largest block varies 33-55 KB by uptime.
   Make sure the screens really change (the display asleep pauses cycling).
14. **Permanent task stacks cost every TLS session.** A network worker (one 12 KB, then 8 KB task for
   all downloads, instead of short-lived tasks) was built and measured in v0.2.8 and dropped: later
   downloads started with 3-9 KB less free heap, weather failed twice and the lowest free heap fell
   to 72 B (7.8 KB before). The code is on the `wip/net-worker` branch for v0.3.

## Core 3 settings (v0.3.0)

Measured on the CYD with `release/logs/p3_measure.sh` (steady free 8-bit heap, one boot each).
Kept (in `platformio.ini` `custom_sdkconfig`): Bluetooth off; WiFi RX buffers 6 static / 16
dynamic; TLS out buffer 4 KB; lwIP task stack 3 KB and timer task 2.5 KB (core 3 defaults are
4 KB each; measured use ~1.2 / ~0.8 KB, `mem.stack_free` `tcpip`/`timer`; +2.2 KB); unused bundled
components removed and the error-name table off (flash). Not pursued: fewer sockets and a smaller
AsyncTCP queue (a few hundred bytes), 5 static WiFi RX buffers (1.6 KB, slower downloads incl.
Cloud OTA), a smaller TLS receive buffer (servers send 16 KB records), dynamic TLS buffers
(fragmented the heap in the platform spike).

## Measuring

- `/api/status` → `mem`: `heap_min_free`, `largest_block` (8-bit, now and lowest since boot),
  `frag_pct` (how much of the free heap is outside the largest block), `lvgl_used`,
  `lvgl_max_used`, `lvgl_page_peak` (per page), `web_refused`, `stack_free`, and `jobs`: the last
  downloads with their duration and free/largest before, lowest during (sampled every 50 ms) and
  after. Serial prints the same as `[Mem] job ...` lines, and `[HEAP]` lines trace the boot.
  `reset_reason` (top level) tells a crash (4) from a restart (3) or power-on (1).
- **Heap map** (what sits between the free areas): a temporary probe allocates the largest free
  block repeatedly, records each area's address and size, frees them, and prints the heap headers
  (`0xabba1234` + size, with heap poisoning) in the gap with their first bytes. That identified the
  HamAlert/APRS receive buffers (`# logresp`, `# aprsc`) splitting the heap. `heap_caps_dump()`
  crashed the device (it prints while holding the heap lock); don't use it. The probe itself briefly
  takes all free memory, so judge stability on a build without it.
- LVGL peak over all pages: flash the `cyd-screens` build and open each page with
  `POST /api/debug/screen?page=N` (or `tools/device_screens.py`), reading `/api/status` after each.
- Heap regions: a temporary `heap_caps_print_heap_info(MALLOC_CAP_8BIT)` prints every region's
  free space, largest block and block count (remove it before committing).
- Static DRAM: `check_size.py` on every build; `tools/dram_report.py <map>` lists the largest users.
- The `cyd-screens` build has less free heap than a release build; judge HTTPS behaviour on `cyd`.

## Flash (firmware size)

The app slot is 1,835,008 B; `check_size.py` warns above 85 % and fails above 95 %. The partition
layout can't grow without breaking OTA for deployed devices, so size is managed in the code. v0.2.4
went from 85.5 % to 81.9 % (66 KB):

- **No `sscanf`** (~10 KB of scanf code): parse with `strtol`/`strtof` (`version.h`, `solar_parse`,
  `hamalert_parse`). The libc time-zone code keeps the integer-only scanf engine anyway.
- **TFT_eSPI fonts 2-8 and smooth fonts off** (`src/hw/User_Setup.h`, ~19 KB): LVGL draws all text.
- **Unused LVGL widgets off** (`include/lv_conf.h`, ~17 KB): enable a widget there before using it.
- **Icons-only fallback font** instead of Montserrat 10/14 (~20 KB, see `docs/UI_GUIDE.md`).

In reserve: TLS errors as numeric codes instead of mbedTLS's 15 KB text table (codes are
standardised in mbedTLS's headers; document the common ones in `docs/DEBUGGING.md`), and link-time
optimisation with a newer toolchain (Arduino core 3 / ESP-IDF 5, `docs/TASKS.md`). Compressing the
splash logos was measured and dropped (2026-11-04): RLE makes them larger, LZ4 saves 1.7 KB net.

**Budget review (2026-11-04, v0.2.6 work, 1,580,704 B = 86.1 %):** flash by library, from a linker map:

| KB | What | Can it shrink? |
|---:|---|---|
| ~275 | WiFi stack (net80211, pp, wpa_supplicant, phy) | no (prebuilt) |
| 164 | LVGL | widgets already trimmed; fonts are ours (below) |
| 149 | mbedTLS (crypto + TLS) | prebuilt; the server handshake (7 KB) is linked although QRPickle is only a client: needs a custom sdkconfig (port) |
| 105 | lwIP | no |
| 97 | libc | printf twice (`_vfprintf_r` 12 KB, `_svfprintf_r` 12 KB) and the scanf engine (8 KB, kept by the framework/time-zone code, not by QRPickle) |
| 43 | SGP4 (satellites) | `sgp4init` 17 KB, `sgp4` 8 KB: needed |
| 43 | ESPAsyncWebServer | no |
| 30 | `web_server.cpp` | the inline fallback page is 1.7 KB; the rest is routes |
| 29 | LittleFS | no |
| 15 | `cloud_ota.cpp` | includes the ISRG root certificates (needed) |
| 13 | splash logos (C arrays) | measured: compression doesn't pay |

Headroom to the 95 % limit: 162 KB. Nothing large is removable on this platform without a port; the
candidates for the platform spike are the TLS server code, the duplicate printf and the error texts
(together ~30-40 KB).

## Next

A memory and network coordinator is planned for v0.2.5 (`docs/TASKS.md`): fixed stacks for the
long-running tasks (no restart churn), one gate for TLS that checks the 8-bit heap before
connecting, and a boot schedule that starts services one after another.
