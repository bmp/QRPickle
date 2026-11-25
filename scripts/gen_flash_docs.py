#!/usr/bin/env python3
"""Flash layout in the docs, generated from partitions.csv (the single source).

Writes the sections between the markers in README.md (partition table, web-flasher offsets) and
docs/RELEASING.md (offsets table). `--check` (also used by scripts/check_flash_docs.py before every
build and in CI) fails when a generated section is out of date, or when a flash command in the
docs (`0x... file.bin`) uses an address that differs from the layout.

Run: python3 -I scripts/gen_flash_docs.py [--check]
"""
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
CSV = "partitions.csv"
BEGIN = "<!-- BEGIN generated: {} (scripts/gen_flash_docs.py from partitions.csv) -->"
END = "<!-- END generated: {} -->"

# What each region holds and which install file writes it (the addresses come from the CSV).
INFO = {
    "bootloader": ("Bootloader", "ESP32 second-stage bootloader", "`bootloader.bin`"),
    "table": ("Partition table", "The table below", "`partitions.bin`"),
    "nvs": ("**`nvs`**", "Settings: WiFi, callsign, keys, admin password",
            "Kept (cleared only by \"Erase device\")"),
    "otadata": ("**`otadata`**", "Which app slot boots (`app0` or `app1`)", "`boot_app0.bin` (boot `app0`)"),
    "app0": ("**`app0`**", "Firmware slot 1", "`firmware.bin`"),
    "app1": ("**`app1`**", "Firmware slot 2: updates (web upload or Cloud OTA) are written to whichever "
             "slot isn't running, so a failed update can roll back", "Not written"),
    "spiffs": ("**`spiffs`**", "LittleFS: web console (`www/`), images, about text, saved profiles",
               "`littlefs.bin`"),
}
FIXED = [("bootloader", 0x1000, None), ("table", 0x8000, None)]  # set by the ESP32, not in the CSV
FILE_OF = {"bootloader": "bootloader.bin", "table": "partitions.bin", "otadata": "boot_app0.bin",
           "app0": "firmware.bin", "spiffs": "littlefs.bin"}
DOCS = ["README.md", "docs/RELEASING.md", "docs/SETUP.md", "docs/DEVELOPMENT.md", "docs/DEBUGGING.md"]


def layout(root=ROOT):
    """[(name, offset, size or None)] in flash order."""
    rows = list(FIXED)
    with open(os.path.join(root, CSV), encoding="utf-8") as f:
        for line in f:
            line = line.split("#", 1)[0].strip()
            if line:
                c = [x.strip() for x in line.split(",")]
                rows.append((c[0], int(c[3], 0), int(c[4], 0)))
    return rows


def partition_table(rows):
    out = ["| Address | Region | Size | Contents | Written at install |",
           "| :--- | :--- | :--- | :--- | :--- |"]
    for name, off, size in rows:
        region, what, file = INFO[name]
        size_txt = "" if size is None else f"`0x{size:06X}` (" + (
            f"{size / (1 << 20):g} MB)" if size >= 1 << 20 else f"{size // 1024} KB)")
        out.append(f"| `0x{off:06X}` | {region} | {size_txt} | {what} | {file} |")
    return "\n".join(out)


def install_files(rows):
    """(file, offset) for every file a full install writes, in flash order."""
    return [(FILE_OF[n], off) for n, off, _ in rows if n in FILE_OF]


def offsets_list(rows):
    return "\n".join(f"   * **`0x{off:X}`** ➔ `{f}`" for f, off in install_files(rows))


def offsets_table(rows):
    return "\n".join(["| File | Offset |", "|---|---|"]
                     + [f"| `{f}` | `0x{off:X}` |" for f, off in install_files(rows)])


SECTIONS = {"README.md": {"partitions": partition_table, "flash offsets": offsets_list},
            "docs/RELEASING.md": {"flash offsets": offsets_table}}


def render(text, key, body):
    b, e = BEGIN.format(key), END.format(key)
    if b not in text or e not in text:
        raise KeyError(key)
    head, rest = text.split(b, 1)
    indent = head[head.rfind("\n") + 1:]  # markers inside a list keep the list's indent
    return head + b + "\n" + body + "\n" + indent + e + rest.split(e, 1)[1]


def check_commands(root, rows):
    """Errors for `0x<addr> <file>.bin` pairs in the docs that differ from the layout."""
    want = dict(install_files(rows))
    errors = []
    for doc in DOCS:
        path = os.path.join(root, doc)
        if not os.path.exists(path):
            continue
        with open(path, encoding="utf-8") as f:
            for n, line in enumerate(f, 1):
                for addr, name in re.findall(r"(0x[0-9a-fA-F]+)\W{0,8}\s+`?([a-z_0-9]+\.bin)", line):
                    if name in want and int(addr, 16) != want[name]:
                        errors.append(f"{doc}:{n}: {name} at {addr}, partitions.csv says 0x{want[name]:X}")
    return errors


def run(root=ROOT, check=False):
    rows, errors = layout(root), []
    for doc, sections in SECTIONS.items():
        path = os.path.join(root, doc)
        with open(path, encoding="utf-8") as f:
            text = f.read()
        new = text
        for key, fn in sections.items():
            try:
                new = render(new, key, fn(rows))
            except KeyError:
                errors.append(f"{doc}: markers for '{key}' missing")
        if new != text:
            if check:
                errors.append(f"{doc}: flash layout out of date (run python3 -I scripts/gen_flash_docs.py)")
            else:
                with open(path, "w", encoding="utf-8") as f:
                    f.write(new)
    return errors + check_commands(root, rows)


if __name__ == "__main__":
    errs = run(check="--check" in sys.argv)
    for e in errs:
        print(f"[flash_docs] {e}", file=sys.stderr)
    sys.exit(1 if errs else 0)
