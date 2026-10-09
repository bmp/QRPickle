#!/usr/bin/env python3
"""Build the GitHub Pages site published by CI on each release (review 2.9, ESP Web Tools).

  <out>/ota/ota.json + firmware.bin              -> Cloud OTA (devices read this over WiFi)
  <out>/install/index.html + versions.json       -> browser installer (ESP Web Tools) with a version list
  <out>/install/<version>/manifest.json + *.bin  -> full-flash images at their flash addresses
  <out>/install/manifest.json + *.bin            -> the latest version (stable links)

Usage: python3 scripts/make_pages_site.py --build-dir .pio/build/cyd --out site \
           --version v0.2.1 --notes-file notes.txt [--boot-app0 PATH] [--previous-dir DIR] [--keep 5]
--previous-dir holds one folder per earlier release (v0.2.0/, ...) with the five .bin files and an
optional notes.txt; CI downloads them from GitHub Releases. Versions before MIN_VERSION, incomplete
folders and names that aren't vMAJOR.MINOR.PATCH are skipped.
The notes are read from files (never shell arguments) so commit messages can't inject commands.
"""
import argparse
import glob
import hashlib
import json
import os
import re
import shutil

# Flash addresses: bootloader and partition table at their fixed places, then the start of the
# otadata, app0 and spiffs partitions from partitions.csv (README "Partition System").
PARTS = [("bootloader.bin", 0x1000), ("partitions.bin", 0x8000), ("boot_app0.bin", 0xE000),
         ("firmware.bin", 0x10000), ("littlefs.bin", 0x390000)]
# The first release with the web login and this installer; older ones are never offered.
MIN_VERSION = (0, 2, 0)
VERSION_RE = re.compile(r"^v(\d+)\.(\d+)\.(\d+)$")

def parse_version(v):
    m = VERSION_RE.match(v)
    return tuple(int(x) for x in m.groups()) if m else None

def short_notes(path):
    if not path or not os.path.isfile(path):
        return ""
    return " ".join(open(path, encoding="utf-8", errors="replace").read().split())[:127]

def write_version(inst, version, files, notes):
    """install/<version>/: the five images plus an ESP Web Tools manifest."""
    vdir = os.path.join(inst, version)
    os.makedirs(vdir)
    parts = []
    for name, offset in PARTS:
        shutil.copy(files[name], os.path.join(vdir, name))
        parts.append({"path": name, "offset": offset})
    with open(os.path.join(vdir, "manifest.json"), "w") as f:
        json.dump({"name": "QRPickle", "version": version, "new_install_prompt_erase": True,
                   "builds": [{"chipFamily": "ESP32", "parts": parts}]}, f, indent=2)
    return {"version": version, "manifest": f"{version}/manifest.json", "notes": notes}

def find_boot_app0(explicit):
    if explicit:
        return explicit
    hits = glob.glob(os.path.expanduser(
        "~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"))
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
    ap.add_argument("--previous-dir")
    ap.add_argument("--keep", type=int, default=5, help="versions offered by the installer, incl. this one")
    ap.add_argument("--installer-page",
                    default=os.path.join(os.path.dirname(__file__), "..", "web-installer", "index.html"))
    a = ap.parse_args()
    if parse_version(a.version) is None:
        raise SystemExit(f"--version must look like v1.2.3, got {a.version!r}")

    shutil.rmtree(a.out, ignore_errors=True)
    ota, inst = os.path.join(a.out, "ota"), os.path.join(a.out, "install")
    os.makedirs(ota); os.makedirs(inst)

    fw = os.path.join(a.build_dir, "firmware.bin")
    sha = hashlib.sha256(open(fw, "rb").read()).hexdigest()
    notes = " ".join(open(a.notes_file, encoding="utf-8", errors="replace").read().split())[:127]

    shutil.copy(fw, os.path.join(ota, "firmware.bin"))
    with open(os.path.join(ota, "ota.json"), "w") as f:
        json.dump({"version": a.version, "notes": notes, "firmware": "firmware.bin", "sha256": sha}, f)

    current = {name: os.path.join(a.build_dir, name) for name, _ in PARTS}
    current["boot_app0.bin"] = find_boot_app0(a.boot_app0)
    for path in current.values():
        if not os.path.isfile(path):
            raise SystemExit(f"missing {path}")
    entries = [write_version(inst, a.version, current, notes)]

    previous = []
    if a.previous_dir and os.path.isdir(a.previous_dir):
        for name in os.listdir(a.previous_dir):
            ver = parse_version(name)
            folder = os.path.join(a.previous_dir, name)
            files = {p: os.path.join(folder, p) for p, _ in PARTS}
            if ver is None or ver < MIN_VERSION or name == a.version:
                continue
            if not all(os.path.isfile(f) for f in files.values()):
                print(f"skipping {name}: incomplete")
                continue
            previous.append((ver, name, files, short_notes(os.path.join(folder, "notes.txt"))))
    for _, name, files, pnotes in sorted(previous, reverse=True)[:max(a.keep - 1, 0)]:
        entries.append(write_version(inst, name, files, pnotes))
    entries[1:] = sorted(entries[1:], key=lambda e: parse_version(e["version"]), reverse=True)

    with open(os.path.join(inst, "versions.json"), "w") as f:
        json.dump({"latest": a.version, "versions": entries}, f, indent=2)
    # Stable links to the latest version (install/manifest.json and its files).
    for name, _ in PARTS:
        shutil.copy(os.path.join(inst, a.version, name), os.path.join(inst, name))
    shutil.copy(os.path.join(inst, a.version, "manifest.json"), os.path.join(inst, "manifest.json"))
    shutil.copy(a.installer_page, os.path.join(inst, "index.html"))

    with open(os.path.join(a.out, "index.html"), "w") as f:
        f.write('<!doctype html><meta http-equiv="refresh" content="0; url=install/">'
                '<a href="install/">Install QRPickle</a>\n')
    open(os.path.join(a.out, ".nojekyll"), "w").close()
    print(f"site: {a.out}  version {a.version}  firmware sha256 {sha}")
    print("installer versions: " + ", ".join(e["version"] for e in entries))

if __name__ == "__main__":
    main()
