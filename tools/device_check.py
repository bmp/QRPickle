#!/usr/bin/env python3
"""Automated on-device health check for QRPickle (CYD over USB serial).

Resets the board N times (RTS -> EN, DTR high = normal boot), captures the serial log
for each boot, and reports per boot: boot completed, WiFi up, NTP synced, watchdog
resets, panics. Secrets in the log are masked. Survives USB re-enumeration.

Usage: python3 -I tools/device_check.py [--port /dev/ttyUSB0] [--cycles 5] [--secs 40]
                                        [--log out.log] [--max-wdt N]
Exit code 0 = pass (no panics, WDT resets <= --max-wdt, every boot reached WiFi).
Requires pyserial (bundled with PlatformIO: ~/.platformio/penv/bin/python).
"""
import argparse
import re
import sys
import time
import serial

MASK = re.compile(r'(appid=|pass(?:word|code)?\s*[:=]\s*)(?!\*\*\*\*)[^ &\s]+', re.I)
CHECKS = {
    'boot': re.compile(r'All operational tasks successfully scheduled'),
    'wifi': re.compile(r'\[Wi-Fi\] Network Link Stable'),
    'ntp':  re.compile(r'NTP Synchronization Successful'),
    'wdt':  re.compile(r'rst:0x(?:7|8|f) '),          # TG0WDT, TG1WDT, RTCWDT
    'panic': re.compile(r'Guru Meditation|Backtrace:|abort\(\) was called|Stack canary|stack overflow'),
    'brownout': re.compile(r'Brownout detector'),
}

def open_port(port):
    for _ in range(40):
        try:
            return serial.Serial(port, 115200, timeout=0.2)
        except Exception:
            time.sleep(0.5)
    sys.exit(f'port {port} unavailable')

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--port', default='/dev/ttyUSB0')
    ap.add_argument('--cycles', type=int, default=5)
    ap.add_argument('--secs', type=float, default=40)
    ap.add_argument('--log', default='device_check.log')
    ap.add_argument('--max-wdt', type=int, default=None, help='default: cycles // 3')
    a = ap.parse_args()
    max_wdt = a.cycles // 3 if a.max_wdt is None else a.max_wdt

    s = open_port(a.port)
    results, glitches, t0 = [], 0, time.time()
    with open(a.log, 'w', buffering=1) as f:  # line-buffered: readable while running
        for c in range(1, a.cycles + 1):
            s.dtr = False; s.rts = True; time.sleep(0.1); s.rts = False
            f.write(f'===== CYCLE {c} =====\n')
            counts = {k: 0 for k in CHECKS}
            end, buf = time.time() + a.secs, b''
            while time.time() < end:
                try:
                    buf += s.read(4096)
                except Exception:
                    glitches += 1
                    f.write('===== USB GLITCH, reopening =====\n')
                    try: s.close()
                    except Exception: pass
                    time.sleep(1); s = open_port(a.port); continue
                *lines, buf = buf.split(b'\n')
                for ln in lines:
                    txt = MASK.sub(r'\1<MASKED>', ln.decode('utf-8', 'replace').rstrip('\r'))
                    f.write(f'{time.time()-t0:7.1f} {txt}\n')
                    for k, rx in CHECKS.items():
                        if rx.search(txt): counts[k] += 1
            f.flush()
            results.append(counts)
    s.close()

    print(f'{"boot":>4} {"ok":>3} {"wifi":>4} {"ntp":>3} {"wdt":>3} {"panic":>5}')
    for i, r in enumerate(results, 1):
        print(f'{i:>4} {r["boot"]:>3} {r["wifi"]:>4} {r["ntp"]:>3} {r["wdt"]:>3} {r["panic"]:>5}')
    wdt = sum(r['wdt'] for r in results); panics = sum(r['panic'] for r in results)
    no_wifi = sum(1 for r in results if r['wifi'] == 0)
    brown = sum(r['brownout'] for r in results)
    ok = panics == 0 and wdt <= max_wdt and no_wifi == 0
    print(f'TOTAL wdt={wdt} (max {max_wdt}) panics={panics} boots_without_wifi={no_wifi} '
          f'brownouts={brown} usb_glitches={glitches} -> {"PASS" if ok else "FAIL"}')
    sys.exit(0 if ok else 1)

if __name__ == '__main__':
    main()
