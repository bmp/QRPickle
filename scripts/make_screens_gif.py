#!/usr/bin/env python3
"""Build the microsite's screen tour (docs/pics/qrpickle-tour.gif) from docs/pics/screens/*.png.

Run after the README screenshots change (needs ImageMagick 7 `magick`); the GIF is committed like
the other generated assets. Usage: python3 scripts/make_screens_gif.py
"""
import os
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCREENS = os.path.join(ROOT, "docs", "pics", "screens")
OUT = os.path.join(ROOT, "docs", "pics", "qrpickle-tour.gif")
# The tour, in order (file names without .png); each shows for 2 seconds.
ORDER = ["dashboard", "menu", "band-conditions", "band-solar", "satellites", "satellites-pass",
         "dx-cluster", "pota", "sota", "aprs-radar", "aprs-traffic", "hamalert", "weather-local",
         "openweather", "forecast", "cloud-ota", "settings"]


def main():
    frames = [os.path.join(SCREENS, n + ".png") for n in ORDER]
    missing = [f for f in frames if not os.path.isfile(f)]
    if missing:
        raise SystemExit(f"missing screenshots: {missing}")
    subprocess.run(["magick", "-delay", "200", "-loop", "0", *frames,
                    "-layers", "Optimize", OUT], check=True)
    print(f"{OUT}: {os.path.getsize(OUT) // 1024} KB, {len(frames)} frames")


if __name__ == "__main__":
    main()
