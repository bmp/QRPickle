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
| LVGL pool (`LV_MEM_SIZE`) | 64 KB | heap, once at `lv_init()` | peak use 52 KB (DX cluster page); 28.5 KB after boot |
| LVGL image cache (`LV_CACHE_DEF_SIZE`) | up to 16 KB | inside the pool | decoded icons from LittleFS; 32 KB ran the pool out (crash) |
| LVGL layer buffer (`LV_DRAW_LAYER_SIMPLE_BUF_SIZE`) | 8 KB chunks | inside the pool | only for objects drawn through a layer (object opacity, transforms) |
| Display draw buffer | 12.8 KB | static | `320 x 10` RGB565 x 2 |
| TLS session (mbedTLS) | ~40 KB in total | heap | two ~16.7 KB buffers plus contexts; certificate checks add more |
| Task stacks | 2-12 KB each | heap | APRS 10 KB, Cloud OTA 12 KB, solar/POTA fetch 8 KB, HamAlert/DX/SOTA 4 KB, LED 2 KB |
| WiFi, lwIP, web server | varies | heap | buffers come and go with traffic |

Measured after 2 minutes of normal running: about 110 KB free in total, 68 KB of it 8-bit, the
big region in 125 blocks with the largest free one 18.4 KB. The lowest free heap at boot was about
4 KB. TLS succeeds or fails depending on whether two 16.7 KB blocks are free at that moment.

## Rules

1. **No large static buffers.** Static DRAM is nearly full and takes from the heap region beside
   it. Allocate at init instead (the LVGL pool does).
2. **Allocate long-lived memory early, once.** Every allocation made at run time, and every task
   that is stopped and restarted, can land somewhere new and split the big region. Prefer one
   buffer kept for the device's lifetime over repeated allocate/free.
3. **One network session at a time:** `services::NetLock` (`net_lock.h`) around every connect or
   HTTP(S) request. Two TLS sessions never fit together.
4. **Background TLS takes the quiet window:** `services::quiet` (`quiet_window.h`) pauses HamAlert
   and APRS while held. It is counted: the last holder restarts what was running. Background
   tasks use `quiet::Hold`; the UI uses `acquire()`/`release()` (xOTA).
5. **Check the 8-bit heap**, not `ESP.getMaxAllocHeap()` (above). Log the largest 8-bit block when a
   TLS request fails.
6. **Don't start heavy work in the first seconds after boot.** WiFi, NTP, the solar fetch and the
   telnet connects already crowd them. The automatic Cloud OTA check starts 2 minutes after boot
   and retries every 10 minutes until it succeeds.
7. **LVGL:** fade with `bg_opa`/`text_opa`, not object opacity on objects with children (that
   renders through a layer buffer). Show only the open tab's objects (`lv_obj_clean` on tab
   change). Keep the number of distinct images on a page small (the image cache is 16 KB).
8. **Measure before and after** a change that adds memory (below), and keep the LVGL peak below
   ~56 KB (8 KB margin in the pool).

## Measuring

- `/api/status` → `mem`: `heap_min_free`, `largest_block` (8-bit, now and lowest since boot),
  `lvgl_used` and `lvgl_max_used`. Sampled every 5 s on the main loop (`src/core/mem_stats.cpp`).
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
standardised in mbedTLS's headers; document the common ones in `docs/DEBUGGING.md`), compressed
(RLE) splash logos (~5-8 KB net), and link-time optimisation with a newer toolchain (Arduino core 3 /
ESP-IDF 5, `docs/TASKS.md`).

## Next

A memory and network coordinator is planned for v0.2.5 (`docs/TASKS.md`): fixed stacks for the
long-running tasks (no restart churn), one gate for TLS that checks the 8-bit heap before
connecting, and a boot schedule that starts services one after another.
