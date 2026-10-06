# Plan D: OTA Safety (review 2.1–2.8)

- [x] 2.1/2.2: `UPDATE_SIZE_UNKNOWN`. Device test: firmware (1.62MB) and filesystem uploaded with `curl --digest -F` → 200, reboot, healthy.
- [x] 2.6: trial-boot guard in NVS (`qrp_ota`: `prev`, `trials`). Device test: a crash image (`-DQRP_TEST_CRASH_AT_BOOT`) via the web → 2 crash boots → rolled back to the previous slot.
- [x] 2.3: restart on every Cloud OTA failure and when the lockdown can't start; 5-minute backstop.
- [x] 2.4: streaming SHA-256 against the `firmware.bin.sha256` asset; CI patch `docs/patches/0002`. TLS authentication remains open.
- [x] 2.5: async `force_update_check()`.
- [x] 2.7: `compare_versions()` + native test.
- [x] 2.8: single response; "no file" rejected.
- [x] Final: 5 boots, 0 watchdog resets; build, cppcheck, 14 native tests.
- Not testable without a GitHub release: the end-to-end Cloud OTA flash (listed in NEEDS_REVIEW).

**Commands used**
```
curl --digest -u admin:$PW -F firmware=@firmware.bin "http://$IP/api/system/update?target=firmware"
curl --digest -u admin:$PW -F fs=@littlefs.bin       "http://$IP/api/system/update?target=filesystem"
```
