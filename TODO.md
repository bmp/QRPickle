# TODO

Gaps found during the initial Claude Code review (2026-10-06). The code review findings, with severity, location and status, live in `docs/reviews/2026-10-code-review.md`; owner checks and follow-up tasks live in `docs/NEEDS_REVIEW.md`. Don't duplicate them here.

## Done
- [x] Build guards implemented: `check_secrets.py`, `gzip_data.py`, `check_size.py`, plus `check_licenses.py`.
- [x] Native unit tests: `test_parsers` and `test_config` (`pio test -e native`).
- [x] `release/` is untracked; binaries come only from CI.
- [x] Releases run only on a `v*` tag matching `FW_VERSION`.
- [x] `CORE_DEBUG_LEVEL` is 1 in `cyd`; verbose logs use the `cyd-debug` env.
- [x] Filename typo fixed: `src/ui/screens/cloud_ota.cpp` (was `clout_ota.cpp`).

## Open
- [ ] `test/test_scheduler/` is empty and excluded from `test_filter`. Add tests or remove the folder.
- [ ] `test/test_hw_led/` is a standalone on-device LED/TFT sketch, not a Unity test.
- [ ] There is no linter or formatter config.
