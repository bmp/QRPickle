# Plan A: Static DRAM Headroom (review 6.5)

**Goal:** free static DRAM (64 B headroom) without reducing total free RAM, and stop regressions.

- [x] Measure the largest users from the linker map (`tools/dram_report.py`). LVGL's builtin pool (`work_mem_int`) used 64KB; `draw_buf` 6.4KB; nothing else over 1.4KB.
- [x] `include/lv_conf.h`: `LV_MEM_POOL_INCLUDE <stdlib.h>` + `LV_MEM_POOL_ALLOC malloc` (the same TLSF pool, heap-allocated at `lv_init()`). Clean rebuild. Headroom: 65,600 B.
- [x] `scripts/check_size.py`: fail under 4KB of DRAM headroom; warn above 85% / fail above 95% of the app slot.
- [x] `tools/device_check.py`: automated N-boot health check over USB.
- [x] Device: 5 boots PASS (0 panics; 1 watchdog reset = the 3.14 baseline).
- `draw_buf` stays static (DMA/flush path; 6.4KB is fine now).
