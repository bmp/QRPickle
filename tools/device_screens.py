#!/usr/bin/env python3
"""Capture every screen of the CYD in every theme, as PNGs plus an HTML gallery (and a diff report).

Needs the screenshot firmware (never released): `pio run -e cyd-screens -t upload`.

    QRP_ADMIN_PW=... python3 -I tools/device_screens.py --ip 192.168.0.4 \
        --out shots/after [--compare shots/before] [--themes 0,6] [--pages 0,1]

Everything goes over WiFi (no serial: opening the port resets the board, and a weak USB cable can
drop it). The device renders a screenshot in 12 bands of 20 rows (src/core/screen_tools.h); the
tool requests and downloads each band and stitches them together.
"""
import argparse
import html
import os
import struct
import sys
import time
import urllib.error
import urllib.request
import zlib


W, H = 320, 240
PAGES = ["dashboard", "weather", "network", "settings", "spots", "xota", "aprs", "aprs_radar",
         "aprs_msg", "band_cond", "hamalert", "cloud_ota"]
THEMES = ["classic", "field_red", "slate_dark", "light", "terminal_green", "eink_light", "eink_dark"]
BAND_ROWS, BANDS = 20, 12


def opener(ip, password):
    mgr = urllib.request.HTTPPasswordMgrWithDefaultRealm()
    mgr.add_password(None, f"http://{ip}/", "admin", password)
    return urllib.request.build_opener(urllib.request.HTTPDigestAuthHandler(mgr))


def call(op, ip, path, method="GET"):
    req = urllib.request.Request(f"http://{ip}{path}", data=b"" if method == "POST" else None, method=method)
    with op.open(req, timeout=10) as r:
        return r.read()


def capture(op, ip):
    """One screenshot as RGB565 bytes (W*H*2), or None if a band couldn't be fetched."""
    fb = bytearray()
    for n in range(BANDS):
        call(op, ip, f"/api/debug/band?n={n}", "POST")
        for _ in range(50):                      # rendering takes a few ms on the main loop
            try:
                data = call(op, ip, f"/api/debug/band?n={n}")
                break
            except urllib.error.HTTPError as e:
                if e.code != 404:
                    raise
                time.sleep(0.05)
        else:
            return None
        if len(data) != W * BAND_ROWS * 2:
            return None
        fb += data
    return bytes(fb)


def write_png(path, rgb565):
    raw = bytearray()
    for y in range(H):
        raw.append(0)
        for x in range(W):
            (v,) = struct.unpack_from("<H", rgb565, (y * W + x) * 2)
            r, g, b = (v >> 11) & 0x1F, (v >> 5) & 0x3F, v & 0x1F
            raw += bytes(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0)) +
           chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)
    with open(path[:-4] + ".rgb565", "wb") as f:   # raw copy for exact comparisons
        f.write(rgb565)


def compare(a_dir, b_dir, name):
    """Number of differing pixels between two captures (None if one is missing)."""
    try:
        with open(os.path.join(a_dir, name + ".rgb565"), "rb") as f:
            a = f.read()
        with open(os.path.join(b_dir, name + ".rgb565"), "rb") as f:
            b = f.read()
    except OSError:
        return None
    return sum(1 for i in range(0, len(a), 2) if a[i:i + 2] != b[i:i + 2])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--compare", help="directory of an earlier capture to diff against")
    ap.add_argument("--themes", default=",".join(str(i) for i in range(len(THEMES))))
    ap.add_argument("--pages", default=",".join(str(i) for i in range(len(PAGES))))
    ap.add_argument("--settle", type=float, default=3.0, help="seconds to wait after opening a page")
    a = ap.parse_args()
    password = os.environ.get("QRP_ADMIN_PW") or sys.exit("set QRP_ADMIN_PW")
    os.makedirs(a.out, exist_ok=True)

    op = opener(a.ip, password)

    results = []
    for t in (int(x) for x in a.themes.split(",")):
        for p in (int(x) for x in a.pages.split(",")):
            name = f"{t}_{THEMES[t]}__{p:02d}_{PAGES[p]}"
            for attempt in range(1, 4):
                try:
                    call(op, a.ip, f"/api/debug/screen?page={p}&theme={t}", "POST")
                    time.sleep(a.settle)
                    px = capture(op, a.ip)
                except OSError as e:   # WiFi hiccup or device reboot: wait and retry
                    print(f"  {name}: {e}; retrying", flush=True)
                    time.sleep(10)
                    px = None
                if px:
                    write_png(os.path.join(a.out, name + ".png"), px)
                    break
                print(f"  {name}: incomplete capture, retry {attempt}", flush=True)
            else:
                print(f"FAILED {name}", flush=True)
                results.append((name, None, None))
                continue
            diff = compare(a.compare, a.out, name) if a.compare else None
            results.append((name, True, diff))
            print(f"{name}" + (f"  diff={diff} px" if a.compare else ""), flush=True)
    # back to the dashboard in the saved theme
    call(op, a.ip, "/api/debug/screen?page=0", "POST")

    rows = []
    for name, _ok, diff in results:
        cells = f'<td><img src="{html.escape(name)}.png" width="320"></td>'
        if a.compare:
            cells = (f'<td><img src="{html.escape(os.path.relpath(os.path.join(a.compare, name + ".png"), a.out))}" '
                     f'width="320"></td>' + cells)
        rows.append(f"<tr><th>{html.escape(name)}<br>{'' if diff is None else str(diff) + ' px differ'}"
                    f"</th>{cells}</tr>")
    head = "<tr><th>screen</th>" + ("<th>before</th>" if a.compare else "") + "<th>capture</th></tr>"
    with open(os.path.join(a.out, "index.html"), "w") as f:
        f.write("<!doctype html><meta charset=utf-8><title>QRPickle screens</title>"
                "<style>body{font-family:sans-serif}th{text-align:left;vertical-align:top;padding:4px}"
                "img{border:1px solid #888}</style><table>" + head + "".join(rows) + "</table>")
    failed = sum(1 for _, ok, _ in results if not ok)
    changed = sum(1 for _, ok, d in results if ok and d)
    print(f"DONE: {len(results) - failed} captured, {failed} failed"
          + (f", {changed} differ from {a.compare}" if a.compare else "") + f" -> {a.out}/index.html")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
