#!/usr/bin/env python3
"""Licence guard: fails when third-party material isn't recorded.

Runs as a PlatformIO pre-build script and standalone (CI): python3 scripts/check_licenses.py
Checks that THIRD_PARTY_NOTICES.md covers:
  - every lib_deps entry of [env:cyd] and every library PlatformIO actually fetched
    (.pio/libdeps/cyd, incl. transitive ones), with the licence the library declares;
  - the pinned platform (espressif32 version);
  - every font in assets/fonts/ (with its OFL text) and the LVGL built-in fonts enabled in
    include/lv_conf.h;
  - every source/data file carrying a copyright or SPDX line (its path or glob must be listed);
and that every image in assets/img/ is listed in assets/img/SOURCES.md and every file in
docs/pics/third-party/ in that folder's README.md.
"""
import configparser
import fnmatch
import json
import os
import re
import subprocess
import sys

NOTICES = "THIRD_PARTY_NOTICES.md"
# What ships in the firmware, the LittleFS image or the Pages site (git-tracked files only).
SCAN_DIRS = ("src", "include", "data", "web-installer")
SCAN_EXTS = (".c", ".cpp", ".h", ".hpp", ".py", ".js", ".html", ".css", ".sh")
# Libraries used only by the native unit tests, never linked into the firmware.
TEST_ONLY = {"unity"}
LVGL_FONTS = {"MONTSERRAT": "Montserrat", "DEJAVU": "DejaVu", "SIMSUN": "SimSun",
              "UNSCII": "UNSCII", "SOURCE_HAN": "Source Han"}


def norm(s):
    return re.sub(r"[^a-z0-9]", "", s.lower())


def lib_name(dep):
    dep = dep.split("@")[0].strip()
    if dep.startswith(("http://", "https://", "git")):
        return re.sub(r"\.git$", "", dep.split("#")[0].rstrip("/").split("/")[-1])
    return dep.split("/")[-1]


def declared_licence(lib_dir):
    path = os.path.join(lib_dir, "library.json")
    if os.path.exists(path):
        try:
            with open(path, encoding="utf-8") as f:
                lic = json.load(f).get("license")
            if isinstance(lic, str):
                return lic
        except ValueError:
            pass
    path = os.path.join(lib_dir, "library.properties")
    if os.path.exists(path):
        with open(path, encoding="utf-8", errors="ignore") as f:
            for line in f:
                if line.lower().startswith("license="):
                    return line.split("=", 1)[1].strip()
    return None


def tracked_files(root, dirs):
    try:
        out = subprocess.run(["git", "ls-files", "--", *dirs], cwd=root, capture_output=True,
                             text=True, check=True).stdout
        return [p for p in out.splitlines() if p]
    except (OSError, subprocess.CalledProcessError):  # no git: walk, skipping build/venv dirs
        found = []
        for d in dirs:
            for dirpath, dirnames, files in os.walk(os.path.join(root, d)):
                dirnames[:] = [x for x in dirnames if x not in (".pio", "venv", "node_modules")]
                found += [os.path.relpath(os.path.join(dirpath, f), root).replace(os.sep, "/") for f in files]
        return found


def notice_rows(text):
    return [line for line in text.splitlines() if line.startswith("|")]


def check(root):
    errors = []
    try:
        with open(os.path.join(root, NOTICES), encoding="utf-8") as f:
            notices = f.read()
    except OSError:
        return [f"{NOTICES} is missing"]
    rows = notice_rows(notices)
    n_notices = norm(notices)

    def row_for(name):
        # Match the first column (the component name); prefer a row that starts with the name.
        key = norm(name)
        firsts = [(r, norm(r.split("|")[1])) for r in rows if r.count("|") > 2]
        return next((r for r, c in firsts if c.startswith(key)), None) or \
            next((r for r, c in firsts if key in c), None)

    ini = configparser.ConfigParser(interpolation=None, inline_comment_prefixes=(";",))
    ini.read(os.path.join(root, "platformio.ini"))
    env = ini["env:cyd"]

    # 1. Declared libraries.
    for dep in filter(None, (d.strip() for d in env.get("lib_deps", "").splitlines())):
        if not row_for(lib_name(dep)):
            errors.append(f"lib_deps '{lib_name(dep)}' is not in {NOTICES}")

    # 2. Fetched libraries (only after PlatformIO installed them) and their declared licence.
    libdeps = os.path.join(root, ".pio", "libdeps", "cyd")
    if os.path.isdir(libdeps):
        for name in sorted(os.listdir(libdeps)):
            lib_dir = os.path.join(libdeps, name)
            if not os.path.isdir(lib_dir) or norm(name) in TEST_ONLY:
                continue
            row = row_for(name.replace(" Library", ""))
            if not row:
                errors.append(f"fetched library '{name}' is not in {NOTICES}")
                continue
            lic = declared_licence(lib_dir)
            if lic and norm(lic) not in norm(row):
                errors.append(f"library '{name}' declares licence '{lic}', {NOTICES} says: {row.strip()}")

    # 3. Platform version (the Arduino core and ESP-IDF versions follow from it).
    platform = env.get("platform", "").replace(" ", "")
    if norm(platform) not in n_notices:
        errors.append(f"platform '{env.get('platform')}' is not recorded in {NOTICES} (review the core/IDF rows)")

    # 4. Fonts.
    fonts_dir = os.path.join(root, "assets", "fonts")
    for name in sorted(os.listdir(fonts_dir)) if os.path.isdir(fonts_dir) else []:
        if name.lower().endswith((".ttf", ".otf", ".woff", ".woff2")) and name not in notices:
            errors.append(f"font assets/fonts/{name} is not in {NOTICES}")
    for m in re.finditer(r"assets/fonts/(OFL-[\w.-]+\.txt)", notices):
        if not os.path.exists(os.path.join(fonts_dir, m.group(1))):
            errors.append(f"{NOTICES} refers to missing assets/fonts/{m.group(1)}")
    try:
        with open(os.path.join(root, "include", "lv_conf.h"), encoding="utf-8") as f:
            lv_conf = f.read()
        for macro, family in LVGL_FONTS.items():
            if re.search(rf"#define\s+LV_FONT_{macro}\w*\s+1\b", lv_conf) and norm(family) not in n_notices:
                errors.append(f"lv_conf.h enables LVGL's {family} font, which is not in {NOTICES}")
    except OSError:
        pass

    # 5. Files with a foreign copyright/SPDX line must be listed (path or glob in backticks).
    listed = re.findall(r"`([^`]+)`", notices)
    for path in tracked_files(root, SCAN_DIRS):
        if not path.endswith(SCAN_EXTS):
            continue
        with open(os.path.join(root, path), encoding="utf-8", errors="ignore") as f:
            head = f.read(4096)
        if re.search(r"Copyright|SPDX-License-Identifier", head) and \
                not any(fnmatch.fnmatch(path, p) or path.startswith(p.rstrip("/") + "/") for p in listed):
            errors.append(f"{path} carries a copyright/licence line but is not listed in {NOTICES}")

    # 6. Images: every asset needs a recorded source.
    for folder, manifest in (("assets/img", "SOURCES.md"), ("docs/pics/third-party", "README.md")):
        full = os.path.join(root, folder)
        if not os.path.isdir(full):
            continue
        try:
            with open(os.path.join(full, manifest), encoding="utf-8") as f:
                text = f.read()
        except OSError:
            errors.append(f"{folder}/{manifest} is missing")
            continue
        for name in sorted(os.listdir(full)):
            if name != manifest and name not in text:
                errors.append(f"{folder}/{name} is not listed in {folder}/{manifest}")
        pending = sum(1 for line in notice_rows(text) if "UNCONFIRMED" in line)
        if pending:
            print(f"[check_licenses] WARNING: {pending} entr{'y' if pending == 1 else 'ies'} in {folder}/{manifest} "
                  "still UNCONFIRMED (source/licence unknown)")
    return errors


def report(errors):
    if errors:
        print("[check_licenses] FAILED: third-party material without a recorded licence:")
        for e in errors:
            print("  - " + e)
        print(f"  Add it to {NOTICES} (or the folder's manifest); see CLAUDE.md.")
    else:
        print("[check_licenses] OK")


try:
    Import("env")  # noqa: F821  (PlatformIO pre-build)
    _errors = check(env.subst("$PROJECT_DIR"))  # noqa: F821
    report(_errors)
    if _errors:
        env.Exit(1)  # noqa: F821
except NameError:
    if __name__ == "__main__":
        _errors = check(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        report(_errors)
        sys.exit(1 if _errors else 0)
