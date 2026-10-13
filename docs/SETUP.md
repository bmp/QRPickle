# First-time Setup

From a new Cheap Yellow Display to a working QRPickle in about 15 minutes. You need nothing but the
board, a USB data cable, a computer with Chrome or Edge, and your WiFi details. The optional services
need free accounts, listed first so you can get them ready.

## 1. Before you start

| Need | For | Where to get it |
| :--- | :--- | :--- |
| ESP32-2432S028R "Cheap Yellow Display" (2.8", resistive touch) | everything | see [Hardware and Wiring](HARDWARE.md) |
| A USB **data** cable and Chrome or Edge on a computer | installing | some cables only charge |
| A 2.4 GHz WiFi network (name and password) | everything online | the CYD can't use 5 GHz networks |
| Your callsign and Maidenhead grid square (e.g. `MK82tw`) | spots, band conditions, satellites | |
| *Optional:* an OpenWeather API key | weather and forecast | free account at [openweathermap.org](https://openweathermap.org); a new key can take a while (up to a couple of hours) to start working |
| *Optional:* a HamAlert account and telnet password | your own spot alerts | [hamalert.org](https://hamalert.org): set a telnet password in your account, and give your triggers the *Telnet* action |
| *Optional:* your APRS-IS passcode | APRS (beacon, messages) | a 5-digit number computed from your callsign; APRS apps and passcode generators give it |

DX cluster, POTA, SOTA, band conditions and satellites need no account.

## 2. Install QRPickle

Open the [web installer](https://quixotic.bharathpalavalli.com/QRPickle/install/), connect the CYD and choose
**Full install** (for a new board, tick **Erase device** when asked). It takes about two minutes.

- "Failed to initialize": hold the **BOOT** button while clicking Install, and release it once installing
  starts. On Linux see [Web installer on Linux](../README.md#downloading-pre-compiled-releases).
- No browser that works? The same section shows how to flash from a terminal with `esptool`.

## 3. First start: the setup hotspot

1. The CYD restarts and shows the splash screen. With no WiFi saved, it gives up after about 20 seconds
   and opens its own WiFi network, **`QRPickle-Setup`**. The status LED stays **amber** while it does
   ([LED colours](LEDColours.md)).
2. On the CYD, open the menu (top-left icon) → **Network**. It shows the **admin password**, made up on
   the first start. That password is both the hotspot's WiFi password and the web console login.
3. On your phone or computer, join `QRPickle-Setup` with that password.
4. Open **http://192.168.4.1** and log in as user **`admin`** with the same password.

## 4. Basic Settings

In the web console's **Basic Settings** tab:

| Setting | What to enter |
| :--- | :--- |
| Operator Callsign | your callsign (replaces `N0CALL`) |
| Grid Square | your 4- or 6-character locator |
| Station Latitude / Longitude | *optional*, more precise than the grid square for sunrise, band conditions and satellite passes; tick "Use latitude/longitude for band conditions" |
| UTC Offset (Hours) | your time zone, e.g. `5.5` for India (half-hour steps; no automatic daylight saving: change it twice a year where clocks change) |
| Local Network SSID / WPA Passphrase | your 2.4 GHz WiFi |

Click **Save Configuration**. The CYD joins your WiFi and closes the hotspot. Its new address is on the
**Network** screen; open it in your browser (from your normal network) and log in again as `admin`.

**Change the admin password** now (Basic Settings → Admin Password, 8-16 characters, no spaces). The
Network screen always shows the current one, so you can't lock yourself out.

## 5. Services (all optional)

| Service | In the web console | Notes |
| :--- | :--- | :--- |
| Weather | Advanced → OpenWeatherMap API Token; pick the forecast slots | the dashboard and Weather screen fill in within a few minutes |
| HamAlert | Advanced → HamAlert Telnet Password | logs in with your callsign; only triggers with the *Telnet* action arrive |
| APRS | Advanced → APRS Network Connection on, passcode, SSID suffix, symbol and comment; quick macros for messages | on the device, APRS can also be switched on and off in Settings |
| DX cluster | Advanced → DX Cluster Endpoints | works with the defaults (`dxspider.co.uk`, fallback `dxc.w6bgr.com`); the list fills as spots arrive |
| Band conditions | Advanced → Solar Data; Dashboard Band Tile Shows | solar data from HAMQSL.com by default; pick the band groups for the dashboard tile |
| Satellites | Advanced → Satellites | which kinds of satellites, the minimum elevation, the LED alert before SSTV and school-contact passes |

Secrets you save (WiFi password, keys, passcodes) are never shown again in the console; leave a field
empty to keep the saved value.

## 6. On the device

- **Menu** (top-left): every screen. The **home** button (bottom-right) returns to the dashboard.
- **Settings** (in the menu): callsign, grid, WiFi, brightness (or automatic), theme, screen sleep,
  and APRS, screen cycling and satellite pass alerts on or off.
- **Screen cycling** (web console, Basic Settings): chosen screens in turn; a touch pauses it.
- The clock shows `--:--` until the CYD has set its time from the internet; UTC is always right.

## 7. Save your setup

- **Profiles** tab → **Save Current Form as New Profile** (e.g. "Home", "Field"); switch between them
  later on the device or in the console.
- **Profiles → Backup and Restore → Download Backup** keeps all settings and profiles in a file, without passwords and keys. Do this
  before writing a new filesystem image or a full install, which erase profiles.

## 8. Updates

- **Cloud OTA** (device: menu → Cloud OTA, or the web console tab) checks for new releases and installs
  them over WiFi; your settings stay.
- The release notes say when a release also needs the web console files (`littlefs.bin`, System Info tab).

## Troubleshooting

| Problem | Try |
| :--- | :--- |
| No `QRPickle-Setup` network | wait 30 seconds after a restart; the LED should be amber. It only appears when the saved WiFi can't be reached |
| Hotspot rejects the password | use the one on the device's Network screen (it changes only when you change it) |
| Web console asks for login again and again | user `admin`, the password from the Network screen; try another browser if a password manager fills in an old one |
| Clock shows `--:--` | the CYD has no internet yet: check WiFi; it sets the time as soon as it is online |
| No weather | check the key; new OpenWeather keys can take a couple of hours to work |
| HamAlert shows nothing | check the telnet password and that your triggers have the *Telnet* action |
| Lost everything after a full install | restore your backup (Profiles), then enter the passwords and keys again (Profiles → Restore from Backup) |

More: [LED colours](LEDColours.md), [Hardware and Wiring](HARDWARE.md), and the full
[manual](https://quixotic.bharathpalavalli.com/QRPickle/manual/).
