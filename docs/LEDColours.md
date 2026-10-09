# QRPickle RGB LED Telemetry Status Guide

The Cheap Yellow Display (CYD) features a rear-mounted SMD RGB LED. The QRPickle firmware utilizes this hardware to provide immediate, unobtrusive background telemetry without requiring the operator to wake the LCD screen during field operations.

## 1. Boot Sequence (Initialization Phase)
During a cold boot or hardware reset, the LED acts as a progressive loading indicator.

* **Solid Amber:** Hardware Initialization. (Mounting NVS, checking I2C sensors, allocating display canvas).
* **Solid Blue:** Network Search. (Scanning for Wi-Fi configurations or broadcasting the setup Hotspot).
* ~~**Breathing Cyan:** Services Sync.~~ *Disabled in firmware: the LED stays off during NTP/GitHub sync.*
* **Dim Green (1 Second):** System Ready. (All boot checks passed, handing execution to the dashboard).
* **Off:** Standby. (Normal operation, conserving power).

## 2. Network Status
* ~~**Breathing Magenta:** Link Lost.~~ *Disabled in firmware: the LED stays off while Wi-Fi reconnects; the status bar shows the link state.*

## 3. Live Data Traffic
* **Crisp Dim Cyan Pulse (30ms):** Data Ingress. A standard telemetry packet (APRS coordinate, POTA log, solar conditions) was successfully parsed. Faint to prevent blinding the operator in tactical/low-light environments.
* **White & Magenta Strobe (3 Seconds):** High-Priority Alert. A HamAlert filter was triggered, or a direct peer-to-peer APRS message was received.

## 4. Hardware Faults
* **Triple Red Flash:** Critical System Fault. Repeated 3-second cycle indicating a severe blockage (e.g., flash memory corruption or continuous socket failures).

---
**Hardware Note:** The CYD RGB LED (R = GPIO4, G = GPIO16, B = GPIO17) is **common anode**, so the PWM duty is inverted: `0` is full brightness and `255` is off. An idle channel is driven at duty 255 (off).

**Tests:** the colours and timings above live in `src/hw/led_pattern.cpp` and are checked by the host unit tests in `test/test_led` (run in CI). `test/test_hw_led/` is an optional on-device sketch that cycles through every state with the same code, for checking by eye.
