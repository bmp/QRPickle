#!/usr/bin/env python3
"""Serves the web console from data/www with a mock /api (demo data, no device, no secrets).

Used by capture.mjs to regenerate docs/screenshots/. Run from the repo root:
    python3 -I tools/webui_screenshots/mock_server.py [port]
"""
import json
import os
import re
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
WWW = os.path.join(ROOT, "data", "www")
with open(os.path.join(ROOT, "src", "core", "metadata.h"), encoding="utf-8") as f:
    FW_VERSION = re.search(r'FW_VERSION\s*=\s*"([^"]+)"', f.read()).group(1)
with open(os.path.join(ROOT, "data", "about.txt"), encoding="utf-8") as f:
    ABOUT = f.read()

META = {"fw_name": "QRPickle", "fw_version": FW_VERSION, "author_call": "VU3GLJ"}
CONFIG = {
    **META,
    "callsign": "VU3GLJ", "grid": "MK82tw", "ssid": "Shack-WiFi",
    "password": "", "password_set": True, "apikey": "", "apikey_set": True,
    "lat": 12.92, "lon": 77.60, "offset": 5.5, "brightness": 180, "auto_bright": False,
    "theme_id": 0, "timeout": 5, "fc_slots": 15,
    "dx_url_p": "dxspider.co.uk", "dx_port_p": 7300, "dx_url_s": "dxc.w6bgr.com", "dx_port_s": 7373,
    "aprs_en": True, "aprs_pass": "", "aprs_pass_set": True, "aprs_ssid": 7,
    "aprs_cmt": "QRPickle on the CYD", "aprs_icn": "/[",
    "aprs_macros": ["QRT. Packing up gear.", "CQ POTA, spotting active now.", "All OK, monitoring frequency.",
                    "Changing bands shortly.", "Testing APRS-IS link."],
    "hamalert_pass": "", "hamalert_pass_set": True,
}
PROFILES = {
    "Home": {},
    "Field_Day": {"ssid": "Phone-Hotspot", "aprs_ssid": 9, "aprs_icn": "\\F", "theme_id": 1, "timeout": 2},
    "SOTA_Portable": {"ssid": "Phone-Hotspot", "aprs_ssid": 7, "aprs_icn": "/;", "theme_id": 5, "brightness": 255},
}
STATUS = {**META, "uptime": 5321, "heap": 95064, "rssi": -55, "ip": "192.168.1.50", "sensor_online": True,
          "temp": 28.5, "humidity": 51.7, "pressure": 913.6,
          "ver_lvgl": "v9.5.0", "ver_json": "v7.4.3", "ver_core": "v2.0.17", "ver_idf": "v4.4.7"}
CLOUD_OTA = {"available": False, "latest_ver": FW_VERSION, "notes": "", "local_ver": FW_VERSION}
APRS_MESSAGES = [{"from": "N0CALL-9", "text": "QSY to 145.500, see you there"},
                 {"from": "N0CALL-7", "text": "Activating VU/KA-001 at 0600z"}]
TYPES = {".html": "text/html", ".js": "application/javascript", ".css": "text/css"}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def send(self, body, ctype="application/json", code=200):
        data = body if isinstance(body, bytes) else (body if isinstance(body, str) else json.dumps(body)).encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        url = urlparse(self.path)
        q = parse_qs(url.query)
        routes = {
            "/api/config": lambda: CONFIG,
            "/api/status": lambda: STATUS,
            "/api/profiles": lambda: list(PROFILES),
            "/api/cloud_ota/check": lambda: CLOUD_OTA,
            "/api/aprs/messages": lambda: APRS_MESSAGES,
        }
        if url.path in routes:
            return self.send(routes[url.path]())
        if url.path == "/api/about":
            return self.send(ABOUT, "text/plain")
        if url.path == "/api/profiles/get":
            name = q.get("name", [""])[0]
            if name in PROFILES:
                return self.send({k: v for k, v in {**CONFIG, **PROFILES[name]}.items() if k not in META})
            return self.send({"status": "not_found"}, code=404)
        path = os.path.normpath(os.path.join(WWW, "index.html" if url.path == "/" else url.path.lstrip("/")))
        if path.startswith(WWW) and os.path.isfile(path):
            with open(path, "rb") as f:
                return self.send(f.read(), TYPES.get(os.path.splitext(path)[1], "application/octet-stream"))
        self.send({"status": "not_found"}, code=404)

    def do_POST(self):
        self.send({"status": "success"})


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
    ThreadingHTTPServer(("127.0.0.1", port), Handler).serve_forever()
