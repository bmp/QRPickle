# Needs the owner's review

Items that automation could not verify (screen/touch/browser), plus patches for ask-first files.

## Visual / hands-on checks
- [ ] **Plan C, Network screen:** shows `Web login: admin / <8 chars>`.
- [ ] **Plan C, browser:** opening the console prompts for a login. After logging in, config loads; secret fields are blank with the placeholder "saved (leave blank to keep)"; saving with blanks keeps the stored secrets (WiFi still connects after a reboot).
- [ ] **Plan C, profiles:** saving a new profile (name with letters/digits/`_-` only) and loading it still works; the dropdown lists the profiles.
- [ ] **Plan C, setup AP:** `QRPickle-Setup` now needs the same password (only seen when the device can't reach WiFi).
- [ ] **2.1/2.2:** manual firmware and filesystem upload through the web console.

## Patches to apply (files on the ask-first list)
Apply with `git apply docs/patches/<file>` after review.
- `0001-native-test-filter-add-test_config.patch`: run the config unit tests in plain `pio test -e native`. Until it's applied, use `pio test -e native -f test_parsers -f test_config`.
