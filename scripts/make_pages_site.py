#!/usr/bin/env python3
"""Build the GitHub Pages site published by CI on each release (review 2.9, ESP Web Tools).

  <out>/ota/ota.json + firmware.bin              -> Cloud OTA (devices read this over WiFi)
  <out>/install/index.html + versions.json       -> browser installer (ESP Web Tools) with a version list
  <out>/install/<version>/manifest.json + *.bin  -> full-flash images at their flash addresses
  <out>/install/manifest.json + *.bin            -> the latest version (stable links)
  <out>/index.html, manual/, space/              -> the microsite: home with a screen tour, the manual
                                                     as one page, the satellite feed

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
import subprocess
import sys
import tempfile

PARTITIONS_CSV = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "partitions.csv")


def partition_offsets(path=PARTITIONS_CSV):
    """{name: offset} from partitions.csv (otadata, app0, spiffs, ...)."""
    out = {}
    for line in open(path, encoding="utf-8"):
        cols = [c.strip() for c in line.split("#")[0].split(",")]
        if len(cols) >= 4 and cols[3]:
            out[cols[0]] = int(cols[3], 0)
    return out


_P = partition_offsets()
# The five images and where they go. The bootloader and the partition table sit at the ESP32's fixed
# addresses; the rest come from partitions.csv (the same for every version since v0.2.0).
PARTS = [("bootloader.bin", 0x1000), ("partitions.bin", 0x8000), ("boot_app0.bin", _P["otadata"]),
         ("firmware.bin", _P["app0"]), ("littlefs.bin", _P["spiffs"])]
# What each image is, for the installer's custom mode.
PART_INFO = {"bootloader.bin": "ESP32 bootloader", "partitions.bin": "Partition table",
             "boot_app0.bin": "otadata: start from app0", "firmware.bin": "Firmware (app0)",
             "littlefs.bin": "Web console files (LittleFS); erases saved profiles"}
# The first release with the web login and this installer; older ones are never offered.
MIN_VERSION = (0, 2, 0)
VERSION_RE = re.compile(r"^v(\d+)\.(\d+)\.(\d+)$")

def parse_version(v):
    m = VERSION_RE.match(v)
    return tuple(int(x) for x in m.groups()) if m else None

def full_notes(path, version):
    """The release notes as written (line breaks kept) for the installer page, without the
    "vX.Y.Z: " prefix that the device notes (ota.json) carry."""
    if not path or not os.path.isfile(path):
        return ""
    text = open(path, encoding="utf-8", errors="replace").read().strip()
    return text[len(version) + 2:] if text.startswith(version + ": ") else text


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
    # Sizes let the installer's custom mode check that images don't overlap or leave the flash.
    sizes = {name: os.path.getsize(os.path.join(vdir, name)) for name, _ in PARTS}
    return {"version": version, "manifest": f"{version}/manifest.json", "notes": notes, "sizes": sizes}

def find_boot_app0(explicit):
    if explicit:
        return explicit
    core = os.environ.get("PLATFORMIO_CORE_DIR", os.path.expanduser("~/.platformio"))
    hits = glob.glob(os.path.join(core, "packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"))
    if not hits:
        raise SystemExit("boot_app0.bin not found; pass --boot-app0")
    return hits[0]

SITE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "web-installer")
REPO_ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
# The microsite's pages: (fragment in web-installer/pages, output folder, menu label).
PAGES = [("home.html", "", "Home"), ("install.html", "install", "Install"), ("manual.html", "manual", "Manual"),
         ("space.html", "space", "Space feed")]
# mailto with the address as HTML entities, as on bharathpalavalli.com (fewer harvesters).
CONTACT = "".join(f"&#{ord(c)};" for c in "mailto:vu3glj@disroot.org")
EXTERNAL_NAV = [("Releases", "https://github.com/bmp/QRPickle/releases"),
                ("Source", "https://github.com/bmp/QRPickle"),
                ("Getting On The Air", "https://ham.bharathpalavalli.com/"),
                ("Contact", CONTACT)]


def absolute_repo_links(html, ref):
    """Relative links and images (README.md, docs/...) point at the repository as of `ref`."""
    raw = f"https://raw.githubusercontent.com/bmp/QRPickle/{ref}/"
    blob = f"https://github.com/bmp/QRPickle/blob/{ref}/"

    def absolute(m):
        attr, url = m.group(1), m.group(2)
        if re.match(r"^(https?:|mailto:|#|/|&#)", url):
            return m.group(0)
        return f'{attr}="{(raw if attr == "src" else blob) + url}"'
    return re.sub(r'\b(src|href)="([^"]*)"', absolute, html)


def manual_html(ref):
    """The manual (scripts/build_manual.py) as one HTML page."""
    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, "manual.html")
        subprocess.run([sys.executable, os.path.join(REPO_ROOT, "scripts", "build_manual.py"),
                        "--version", ref, "--html-out", out], check=True)
        return absolute_repo_links(open(out, encoding="utf-8").read(), ref)



def render_pages(out, ref):
    """Every page = web-installer/layout.html around a fragment from web-installer/pages."""
    layout = open(os.path.join(SITE, "layout.html"), encoding="utf-8").read()
    # The subtitle is the README's first "## " heading, as on the manual's cover.
    readme = open(os.path.join(REPO_ROOT, "README.md"), encoding="utf-8").read()
    layout = layout.replace("{{SUBTITLE}}", re.search(r"(?m)^## (.+)$", readme).group(1).strip())
    for frag, folder, _ in PAGES:
        text = open(os.path.join(SITE, "pages", frag), encoding="utf-8").read()
        title = re.search(r"<!-- title: (.*?) -->", text).group(1)
        text = re.sub(r"<!-- title: .*? -->\n", "", text)
        head = ""
        m = re.search(r"<!-- head -->\n(.*?)<!-- /head -->\n", text, re.S)
        if m:
            head, text = m.group(1).rstrip(), text.replace(m.group(0), "")
        body, _, scripts = text.partition("<!-- scripts -->\n")
        if "{{MANUAL}}" in body:
            body = body.replace("{{MANUAL}}", manual_html(ref))
        root = "../" if folder else ""
        active = ' class="active"'
        nav = [f'      <a href="{root}{f + "/" if f else ""}"{active if f == folder else ""}>{label}</a>'
               for _, f, label in PAGES]
        nav += [f'      <a href="{url}">{label}</a>' for label, url in EXTERNAL_NAV]
        page = (layout.replace("{{TITLE}}", title).replace("{{ROOT}}", root).replace("{{HEAD}}", head)
                .replace("{{NAV}}", "\n".join(nav)).replace("{{BODY}}", body.rstrip())
                .replace("{{SCRIPTS}}", scripts.rstrip()))
        dest = os.path.join(out, folder) if folder else out
        os.makedirs(dest, exist_ok=True)
        with open(os.path.join(dest, "index.html"), "w", encoding="utf-8") as f:
            f.write(page)
    for name in ("site.css", "site.js"):
        shutil.copy(os.path.join(SITE, name), os.path.join(out, name))
    shutil.copy(os.path.join(REPO_ROOT, "docs", "pics", "qrpickle-tour.gif"), os.path.join(out, "tour.gif"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--version", required=True)
    ap.add_argument("--notes-file", required=True)
    ap.add_argument("--boot-app0")
    ap.add_argument("--previous-dir")
    ap.add_argument("--ref", help="git ref for the manual's images and links (default: --version)")
    ap.add_argument("--keep", type=int, default=5, help="versions offered by the installer, incl. this one")
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
    entries = [write_version(inst, a.version, current, full_notes(a.notes_file, a.version))]

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
            previous.append((ver, name, files, full_notes(os.path.join(folder, "notes.txt"), name)))
    for _, name, files, pnotes in sorted(previous, reverse=True)[:max(a.keep - 1, 0)]:
        entries.append(write_version(inst, name, files, pnotes))
    entries[1:] = sorted(entries[1:], key=lambda e: parse_version(e["version"]), reverse=True)

    with open(os.path.join(inst, "versions.json"), "w") as f:
        json.dump({"latest": a.version, "versions": entries,
                   "parts": [{"path": n, "offset": f"0x{o:X}", "what": PART_INFO[n]} for n, o in PARTS]},
                  f, indent=2)
    # Stable links to the latest version (install/manifest.json and its files).
    for name, _ in PARTS:
        shutil.copy(os.path.join(inst, a.version, name), os.path.join(inst, name))
    shutil.copy(os.path.join(inst, a.version, "manifest.json"), os.path.join(inst, "manifest.json"))
    # The microsite pages (home, install, manual, space); images come from this ref.
    render_pages(a.out, a.ref or a.version)
    open(os.path.join(a.out, ".nojekyll"), "w").close()
    print(f"site: {a.out}  version {a.version}  firmware sha256 {sha}")
    print("installer versions: " + ", ".join(e["version"] for e in entries))

if __name__ == "__main__":
    main()
