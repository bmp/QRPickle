#!/usr/bin/env python3
"""Long serial capture for catching rare freezes (review 4.6).

Run with PlatformIO's Python:
    ~/.platformio/penv/bin/python -I tools/serial_soak.py --hours 0.5 --out soak.log

Every line is written (line-buffered) with a timestamp; resets, panics, watchdog and
[CRASHLOG] lines are also echoed to stdout. A summary is printed at the end.
Decode a backtrace with: xtensa-esp32-elf-addr2line -pfiaC -e .pio/build/cyd/firmware.elf <addrs>
"""
import argparse
import re
import time

import serial

ALERT = re.compile(r"rst:|Guru Meditation|panic|abort\(\)|task_wdt|Task watchdog|Backtrace|CRASHLOG|Brownout|assert", re.I)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/ttyUSB0")
    ap.add_argument("--hours", type=float, default=0.5)
    ap.add_argument("--out", default="soak.log")
    args = ap.parse_args()

    s = serial.Serial()
    s.port, s.baudrate, s.timeout = args.port, 115200, 1
    s.dtr = False  # don't hold EN/IO0 while capturing. Opening the port on Linux may still
    s.rts = False  # reset the board once, so start the capture before opening the screen to test
    s.open()

    end = time.time() + args.hours * 3600
    alerts = 0
    lines = 0
    last_line = time.time()
    with open(args.out, "a", buffering=1) as log:
        log.write(f"--- soak start {time.strftime('%F %T')} ({args.hours} h) ---\n")
        while time.time() < end:
            raw = s.readline()
            if not raw:
                if time.time() - last_line > 120:
                    log.write(f"{time.strftime('%T')} [soak] no serial output for 120 s\n")
                    last_line = time.time()
                continue
            last_line = time.time()
            line = raw.decode(errors="replace").rstrip()
            lines += 1
            stamped = f"{time.strftime('%T')} {line}"
            log.write(stamped + "\n")
            if ALERT.search(line):
                alerts += 1
                print(stamped, flush=True)
        log.write(f"--- soak end {time.strftime('%F %T')}: {lines} lines, {alerts} alerts ---\n")
    print(f"SOAK DONE: {lines} lines, {alerts} alert lines -> {args.out}", flush=True)


if __name__ == "__main__":
    main()
