# Security Policy

QRPickle runs on a device on your home network and stores your WiFi password, API keys and
APRS/HamAlert credentials. Please report security problems privately, not in a public issue.

## Reporting a vulnerability

Use GitHub's private reporting: **[Report a vulnerability](https://github.com/bmp/QRPickle/security/advisories/new)**
(the repository's **Security** tab → **Report a vulnerability**). Please include:

- the firmware version (the device's Cloud OTA screen, or the footer of the web console),
- what an attacker needs (on the same network? the admin password? physical access?),
- steps to reproduce, and what happens.

You should get an answer within a week. Fixes go out as a new release and through Cloud OTA; the
release notes credit the reporter unless you'd rather not be named.

## Supported versions

Only the latest release gets security fixes. Cloud OTA (on the device or in the web console) updates to it.

## What is in scope

- The web console and its API: every route needs the admin login (Digest), and secrets are
  never sent back.
- The setup access point and the stored settings (NVS) and profiles (LittleFS).
- Cloud OTA: the firmware's SHA-256 and the TLS certificate of the update host are checked.

Out of scope: someone with physical access to the board (USB flashing is open by design), and the
third-party services QRPickle reads from (report those to their owners).
