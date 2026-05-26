# QRPickle RGB LED Telemetry Status Guide

The Cheap Yellow Display (CYD) features a rear-mounted SMD RGB LED. The QRPickle firmware utilizes this hardware to provide immediate, unobtrusive background telemetry without requiring the operator to wake the LCD screen during field operations.

## 1. Boot Sequence (Initialization Phase)
During a cold boot or hardware reset, the LED acts as a progressive loading indicator.

* **Solid Amber:** Hardware Initialization. (Mounting NVS, checking I2C sensors, allocating display canvas).
* **Solid Blue:** Network Search. (Scanning for Wi-Fi configurations or broadcasting the setup Hotspot).
* **Breathing Cyan:** Services Sync. (Connecting to NTP servers, fetching GitHub OTA release manifests).
* **Dim Green (1 Second):** System Ready. (All boot checks passed, handing execution to the dashboard).
* **Off:** Standby. (Normal operation, conserving power).

## 2. Network Status
* **Breathing Magenta:** Link Lost. The Wi-Fi connection was dropped post-boot, and the OS is attempting to auto-reconnect.

## 3. Live Data Traffic
* **Crisp Dim Cyan Pulse (30ms):** Data Ingress. A standard telemetry packet (APRS coordinate, POTA log, solar conditions) was successfully parsed. Faint to prevent blinding the operator in tactical/low-light environments.
* **White & Magenta Strobe (3 Seconds):** High-Priority Alert. A HamAlert filter was triggered, or a direct peer-to-peer APRS message was received.

## 4. Hardware Faults
* **Triple Red Flash:** Critical System Fault. Repeated 3-second cycle indicating a severe blockage (e.g., flash memory corruption or continuous socket failures).

---
**Hardware Note:** The CYD RGB LED is wired via a **Common Anode** configuration. This requires inverted logic in the software (`0` is full brightness, `255` is off). To prevent PWM "ghosting" or "leakage shimmer" when a channel is idle, the firmware explicitly re-routes the pin to a standard digital output and drives it `HIGH` to physically lock the voltage rail.
