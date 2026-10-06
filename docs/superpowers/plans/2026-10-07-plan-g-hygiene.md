# Plan G: Refactors and Hygiene

- [x] 5.7: LED task breadcrumbs; alive at 14.9s (2/2), so not reproduced and no change.
- [x] 1.13: chunked request bodies (device: 3KB → 200, 9KB → 413; security suite 15/15).
- [x] 5.1: `sensor_is_online()`; `--` in the UI, `null` in `/api/status`.
- [x] 1.9/1.10: NVS field table + `static Config` (device: 31/31 keys identical after load and after save + reboot).
- [x] 1.11: changelog tags stripped (64); 1.12 units documented (verified against the code).
- [x] 5.2 LED doc, 5.5 dead code + fault LED, 5.6 README DST note.
- [x] Final device: 5 boots 0 watchdog resets; security 15/15; config identical; sensor online.
- Deferred with reasons: 4.3, 4.4, 5.3, 5.4; partial: 1.9 (JSON side), 3.12 (DX).
