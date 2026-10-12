#!/usr/bin/env python3
"""Pre-build guard: QRPickle's code may only use the LVGL parts that are switched on.

Fails the build (or exits 1 when run directly) when
  - src/ calls a widget or feature switched off in include/lv_conf.h (the compiler would also
    fail, but with a cryptic "not declared" message), or
  - an LV_SYMBOL_* used by src/ or by an enabled LVGL widget is not in the icons-only fallback
    font (scripts/build_fonts.sh SYMBOLS): such a symbol renders as nothing, silently.

The list of what is switched off, and how to switch something on: docs/UI_GUIDE.md.
"""
import glob
import os
import re
import sys

# lv_conf.h switch -> the API prefixes it removes (default: lv_<name>_).
PREFIXES = {
    "IMAGEBUTTON": ["lv_imagebutton_"],
    "ANIMIMG": ["lv_animimg_"],
    "ARCLABEL": ["lv_arclabel_"],
    "OBSERVER": ["lv_subject_", "lv_observer_", "lv_obj_bind_"],
    "GRID": ["lv_obj_set_grid", "lv_obj_set_style_grid", "LV_LAYOUT_GRID", "lv_grid_"],
}
# Widgets whose source may use LV_SYMBOLs (checked when switched on).
WIDGET_DIRS = ["keyboard", "dropdown", "msgbox", "checkbox", "textarea", "tabview", "slider", "label",
               "button", "buttonmatrix", "image", "bar", "list", "menu", "win", "spinbox", "calendar"]
EXTRA_GLYPHS = {0xB0, 0x2022}  # degree sign and bullet, also in the icons font


def code_files(root):
    for pattern in ("src/**/*.cpp", "src/**/*.c", "src/**/*.h"):
        for path in glob.glob(os.path.join(root, pattern), recursive=True):
            if "/fonts/" not in path:  # generated font data
                yield path


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    return re.sub(r"//[^\n]*", " ", text)


def disabled_switches(root):
    conf = open(os.path.join(root, "include", "lv_conf.h"), encoding="utf-8").read()
    return sorted(set(re.findall(r"#define\s+LV_USE_([A-Z0-9_]+)\s+0\b", conf)))


def lvgl_dir(root):
    for path in sorted(glob.glob(os.path.join(root, ".pio", "libdeps", "*", "lvgl"))):
        return path
    return None


def symbol_codepoints(lvgl):
    """LV_SYMBOL_NAME -> code point, from LVGL's symbol header."""
    hdr = None
    for cand in ("include/lvgl/font/lv_symbol_def.h", "src/font/lv_symbol_def.h"):
        p = os.path.join(lvgl, cand)
        if os.path.exists(p) and os.path.getsize(p) > 1000:
            hdr = open(p, encoding="utf-8").read()
            break
    if hdr is None:
        return {}
    out = {}
    for name, val in re.findall(r'#define\s+LV_SYMBOL_(\w+)\s+"((?:\\x[0-9A-Fa-f]{2})+)"', hdr):
        b = bytes(int(x, 16) for x in re.findall(r"\\x([0-9A-Fa-f]{2})", val))
        out[name] = ord(b.decode("utf-8"))
    return out


def font_codepoints(root):
    script = open(os.path.join(root, "scripts", "build_fonts.sh"), encoding="utf-8").read()
    m = re.search(r'SYMBOLS="([^"]+)"', script)
    return {int(x, 16) for x in m.group(1).split(",")} | EXTRA_GLYPHS if m else set()


def check(root):
    errors = []
    switched_off = disabled_switches(root)
    sources = {p: strip_comments(open(p, encoding="utf-8", errors="replace").read()) for p in code_files(root)}

    for sw in switched_off:
        prefixes = PREFIXES.get(sw, [f"lv_{sw.lower()}_"])
        for path, text in sources.items():
            for pre in prefixes:
                m = re.search(r"\b" + re.escape(pre), text)
                if m:
                    line = text[:m.start()].count("\n") + 1
                    rel = os.path.relpath(path, root)
                    errors.append(f"{rel}:{line} uses {pre}... but LV_USE_{sw} is 0 in include/lv_conf.h "
                                  "(switch it on there, then `pio run -t clean`; docs/UI_GUIDE.md)")
                    break

    # The generated icon fonts must hold the code points listed in the script (regenerated).
    script = open(os.path.join(root, "scripts", "build_fonts.sh"), encoding="utf-8").read()
    m = re.search(r'SYMBOLS="([^"]+)"', script)
    for path in sorted(glob.glob(os.path.join(root, "src", "ui", "fonts", "font_symbols_*_raw.c"))):
        opts = re.search(r"Opts:.*?-r (\S+)", open(path, encoding="utf-8").read())
        if m and (not opts or opts.group(1) != m.group(1)):
            errors.append(f"{os.path.relpath(path, root)} does not match SYMBOLS in scripts/build_fonts.sh: "
                          "regenerate the icon fonts (scripts/build_fonts.sh)")

    lvgl = lvgl_dir(root)
    if not lvgl:
        print("[check_lvgl_usage] note: LVGL not installed yet; symbol check skipped")
        return errors
    cps = symbol_codepoints(lvgl)
    font = font_codepoints(root)
    used = {}
    for path, text in sources.items():
        for name in re.findall(r"\bLV_SYMBOL_(\w+)", text):
            used.setdefault(name, os.path.relpath(path, root))
    for w in WIDGET_DIRS:
        if w.upper() in switched_off:
            continue
        for path in glob.glob(os.path.join(lvgl, "src", "widgets", w, "*.c")):
            for name in re.findall(r"\bLV_SYMBOL_(\w+)", strip_comments(open(path, encoding="utf-8").read())):
                used.setdefault(name, f"LVGL widget {w}")
    for name, where in sorted(used.items()):
        if name not in cps:
            continue  # aliases such as LV_SYMBOL_DUMMY
        if cps[name] not in font:
            errors.append(f"LV_SYMBOL_{name} (U+{cps[name]:04X}, used by {where}) is not in the icons font: "
                          "add its code point to SYMBOLS in scripts/build_fonts.sh and regenerate")
    return errors


def report(errors):
    if errors:
        print("[check_lvgl_usage] FAILED:")
        for e in errors:
            print("  - " + e)
    else:
        print("[check_lvgl_usage] OK")


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
