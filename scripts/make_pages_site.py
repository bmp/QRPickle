#!/usr/bin/env python3
"""Build the GitHub Pages site published by CI on each release (review 2.9, ESP Web Tools).

  <out>/ota/ota.json + firmware.bin          -> Cloud OTA (devices read this over WiFi)
  <out>/install/index.html + manifest.json   -> one-click browser installer (ESP Web Tools)
  <out>/install/*.bin                         -> full-flash images at their partition offsets

Usage: python3 scripts/make_pages_site.py --build-dir .pio/build/cyd --out site \
           --version v0.1.12 --notes-file notes.txt [--boot-app0 PATH]
The notes are read from a file (never a shell argument) so commit messages can't inject commands.
"""
import argparse, glob, hashlib, json, os, shutil

# Offsets from partitions.csv / the README flashing table.
PARTS = [("bootloader.bin", 0x1000), ("partitions.bin", 0x8000), ("boot_app0.bin", 0xE000),
         ("firmware.bin", 0x10000), ("littlefs.bin", 0x390000)]

def find_boot_app0(explicit):
    if explicit:
        return explicit
    hits = glob.glob(os.path.expanduser("~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"))
    if not hits:
        raise SystemExit("boot_app0.bin not found; pass --boot-app0")
    return hits[0]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--version", required=True)
    ap.add_argument("--notes-file", required=True)
    ap.add_argument("--boot-app0")
    ap.add_argument("--installer-page", default=os.path.join(os.path.dirname(__file__), "..", "web-installer", "index.html"))
    a = ap.parse_args()

    shutil.rmtree(a.out, ignore_errors=True)
    ota, inst = os.path.join(a.out, "ota"), os.path.join(a.out, "install")
    os.makedirs(ota); os.makedirs(inst)

    fw = os.path.join(a.build_dir, "firmware.bin")
    sha = hashlib.sha256(open(fw, "rb").read()).hexdigest()
    notes = " ".join(open(a.notes_file, encoding="utf-8", errors="replace").read().split())[:127]

    shutil.copy(fw, os.path.join(ota, "firmware.bin"))
    with open(os.path.join(ota, "ota.json"), "w") as f:
        json.dump({"version": a.version, "notes": notes, "firmware": "firmware.bin", "sha256": sha}, f)

    sources = {"boot_app0.bin": find_boot_app0(a.boot_app0)}
    parts = []
    for name, offset in PARTS:
        src = sources.get(name, os.path.join(a.build_dir, name))
        if not os.path.isfile(src):
            raise SystemExit(f"missing {src}")
        shutil.copy(src, os.path.join(inst, name))
        parts.append({"path": name, "offset": offset})
    with open(os.path.join(inst, "manifest.json"), "w") as f:
        json.dump({"name": "QRPickle", "version": a.version, "new_install_prompt_erase": True,
                   "builds": [{"chipFamily": "ESP32", "parts": parts}]}, f, indent=2)
    shutil.copy(a.installer_page, os.path.join(inst, "index.html"))

    with open(os.path.join(a.out, "index.html"), "w") as f:
        f.write('<!doctype html><meta http-equiv="refresh" content="0; url=install/">'
                '<a href="install/">Install QRPickle</a>\n')
    open(os.path.join(a.out, ".nojekyll"), "w").close()
    print(f"site: {a.out}  version {a.version}  firmware sha256 {sha}")

if __name__ == "__main__":
    main()
