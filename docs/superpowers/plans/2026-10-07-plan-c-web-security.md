# Plan C: Web Console Security (review 1.1–1.8)

- [x] `config::sanitize()` + `is_valid_profile_name()`; native tests (`test/test_config`, 4 cases).
- [x] Admin password generated on first boot (NVS `admin_pw`); shown on the Network screen and boot log; used as the setup-AP WPA2 key.
- [x] Digest auth on all 18 routes (body/upload callbacks check too).
- [x] No secrets in the API (`*_set` flags); an empty secret on save keeps the stored one; profiles take live secrets when blank.
- [x] Null-safe JSON copies (`json_copy.h`) in `web_server` and `profile_manager`.
- [x] Config and profile changes applied on the main loop; atomic flags.
- [x] `app.js`: `esc()` for `innerHTML`; "saved" placeholders. CSP header.
- [x] Device: `tools/web_security_check.sh` 15/15 PASS; 5 boots, 0 watchdog resets.
- Visual checks listed in `docs/NEEDS_REVIEW.md`.
