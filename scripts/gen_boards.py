#!/usr/bin/env python3
"""Writes the generated board files from boards/boards.toml (the one board list):

  src/hw/boards_gen.h   the firmware's board table and pin-conflict table
  docs/BOARDS.md        the readable list (model, how to recognise it, status, pins, sources)
  data/www/boards.json  the web console's and installer's board picker

scripts/check_boards.py fails the build when these don't match the TOML or an entry breaks a rule.
Run after editing boards/boards.toml:  python3 -I scripts/gen_boards.py
"""
import json
import os
import sys
import tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOML = os.path.join(ROOT, "boards", "boards.toml")
HEADER = os.path.join(ROOT, "src", "hw", "boards_gen.h")
DOC = os.path.join(ROOT, "docs", "BOARDS.md")
SITE_JSON = os.path.join(ROOT, "data", "www", "boards.json")
HARDWARE_MD = os.path.join(ROOT, "docs", "HARDWARE.md")
README = os.path.join(ROOT, "README.md")
BEGIN = "<!-- BEGIN generated: boards (scripts/gen_boards.py from boards/boards.toml) -->"
END = "<!-- END generated: boards -->"

BOARD_MAX = 16
UI_W, UI_H = 320, 240
INITS = {"ili9341_2": "ILI9341_2", "ili9341": "ILI9341", "st7789": "ST7789", "st7796": "ST7796"}
TOUCH = {"none": "None", "xpt2046_own": "XPT2046Own", "xpt2046_shared": "XPT2046Shared", "gt911": "GT911"}
STATUS = {"tested": "Tested", "reported": "Reported", "mapped": "Mapped"}
NO_ID = 0xFFFFFFFF
REQUIRED_SCREEN = ("init", "width", "height", "swap_xy", "mirror_x", "mirror_y", "bgr", "invert",
                   "sclk", "mosi", "miso", "cs", "dc")


def load(path=TOML):
    with open(path, "rb") as f:
        return tomllib.load(f)


def name(b):
    """'ESP32-2432S028R (2.8", USB-C + micro-USB)'"""
    extra = f", {b['feature']}" if b.get("feature") else ""
    return f"{b['model']} ({b['size']}{extra})"


def complete(b):
    s = b.get("screen", {})
    return (all(k in s for k in REQUIRED_SCREEN) and "pin" in b.get("backlight", {})
            and all(k in b.get("led", {}) for k in ("r", "g", "b")))


def outputs(b):
    """GPIOs this board's settings drive as push-pull outputs (I2C is open drain: not counted)."""
    s, t = b.get("screen", {}), b.get("touch", {})
    pins = [s.get(k) for k in ("sclk", "mosi", "cs", "dc")]
    pins.append(b.get("backlight", {}).get("pin"))
    pins += [b.get("led", {}).get(k) for k in ("r", "g", "b")]
    kind = t.get("type", "none")
    if kind == "xpt2046_own":
        pins += [t.get(k) for k in ("sclk", "mosi", "cs")]
    elif kind == "xpt2046_shared":
        pins.append(t.get("cs"))
    elif kind == "gt911":
        pins += [t.get("rst"), t.get("irq")]  # INT is driven while selecting the I2C address
    return {p for p in pins if isinstance(p, int) and p < 34}  # 34-39 are input-only on the ESP32


def conflicts(boards):
    """conflict[fitted][chosen] = bitmask of GPIOs the chosen board drives against a chip output of
    the fitted board."""
    out = []
    for fitted in boards:
        chips = set(fitted.get("chip_outputs", []))
        out.append([sum(1 << p for p in outputs(chosen) & chips) for chosen in boards])
    return out


def _c_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def _pin(d, k):
    v = d.get(k)
    return str(v) if isinstance(v, int) else "NO_PIN"


def _bool(v):
    return "true" if v else "false"


def _row(b):
    s, bl, led, t = b["screen"], b["backlight"], b["led"], b.get("touch", {})
    i2c, ldr, sd = b.get("i2c", {}), b.get("ldr", {}), b.get("sd", {})
    ui_x = s.get("ui_x", (s["width"] - UI_W) // 2)
    ui_y = s.get("ui_y", (s["height"] - UI_H) // 2)
    cal = t.get("cal", [0, 0, 0, 0])
    chips = sum(1 << p for p in b.get("chip_outputs", []))
    fields = [
        _c_str(b["id"]), _c_str(b["model"]), _c_str(name(b)), f"BoardStatus::{STATUS[b['status']]}",
        _bool(complete(b)), _bool(b.get("addons_v04", False)),
        f"PanelInit::{INITS[s['init']]}", f"0x{s.get('rddid', NO_ID):X}u", f"0x{s.get('rdid4', NO_ID):X}u",
        str(s["width"]), str(s["height"]),
        *(_bool(s[k]) for k in ("swap_xy", "mirror_x", "mirror_y", "bgr", "invert")),
        str(ui_x), str(ui_y), *(str(s[k]) for k in ("sclk", "mosi", "miso", "cs", "dc")),
        str(bl["pin"]), _bool(bl.get("active_high", True)),
        *(str(led[k]) for k in ("r", "g", "b")), _bool(led.get("active_low", True)),
        f"TouchType::{TOUCH[t.get('type', 'none')]}",
        *(_pin(t, k) for k in ("sclk", "mosi", "miso", "cs", "irq", "sda", "scl", "rst")),
        f"0x{t.get('addr', 0):02X}", "{" + ", ".join(str(c) for c in cal) + "}", _c_str(t.get("note", "")),
        _pin(i2c, "sda"), _pin(i2c, "scl"), _pin(ldr, "gpio"), _pin(sd, "cs"), f"0x{chips:X}ull",
    ]
    return "    {" + ", ".join(fields) + "},"


def header(data):
    boards = data["board"]
    ids = [b["id"] for b in boards]
    fallback = ids.index(data["fallback"])
    conf = conflicts(boards)
    n = len(boards)
    rows = "\n".join(_row(b) for b in boards)
    conf_rows = "\n".join("    {" + ", ".join(f"0x{conf[i][j]:X}ull" if i < n and j < n else "0"
                                              for j in range(BOARD_MAX)) + "},"
                          for i in range(BOARD_MAX))
    return f"""// GENERATED by scripts/gen_boards.py from boards/boards.toml. Do not edit: edit the TOML and
// run  python3 -I scripts/gen_boards.py  (scripts/check_boards.py fails the build on drift).
#pragma once
#include "board_def.h"

namespace hw {{

    constexpr size_t BOARD_MAX = {BOARD_MAX};
    constexpr size_t BOARD_COUNT = {n};
    constexpr int BOARD_FALLBACK = {fallback};  // {data['fallback']}: drives the screen as v0.3.1 did

    // clang-format off
    // inline: one copy for the whole firmware (a plain constexpr array in a header is copied into
    // every source file that uses it, so addresses differ between files).
    inline constexpr BoardDef BOARDS[BOARD_COUNT] = {{
{rows}
    }};

    // BOARD_CONFLICT[fitted][chosen]: GPIOs (bit n = GPIO n) the chosen board would drive as outputs
    // while a chip on the fitted board drives them. They stay off whenever the fitted board is in doubt.
    inline constexpr uint64_t BOARD_CONFLICT[BOARD_MAX][BOARD_MAX] = {{
{conf_rows}
    }};
    // clang-format on

}}  // namespace hw
"""


def _status_text(b):
    note = f" ({b['status_note']})" if b.get("status_note") else ""
    return b["status"] + note


def doc(data):
    boards = data["board"]
    lines = [
        "# Supported boards",
        "",
        "<!-- GENERATED by scripts/gen_boards.py from boards/boards.toml: edit the TOML, not this file. -->",
        "",
        "QRPickle detects the board at start-up, or you choose it (web console, System Info; or the "
        "website's Set up over USB page). Check the number printed on the back of the board first.",
        "",
        "Status: **tested** = run by the developer for a release; **reported** = a user confirmed it works; "
        "**mapped** = wired up from the maker's data, nobody has run it yet. Running a mapped board? Please "
        "report how it works (web console, System Info, Report this board).",
        "",
        "| Model (back of the board) | Size | How to recognise it | Status | Screen | Touch |",
        "|---|---|---|---|---|---|",
    ]
    for b in boards:
        t = b.get("touch", {})
        kind = {"xpt2046_own": "resistive (XPT2046)", "xpt2046_shared": "resistive (XPT2046)",
                "gt911": "capacitive (GT911)", "none": "none"}[t.get("type", "none")]
        touch = t["note"] if t.get("type", "none") == "none" and t.get("note") else \
            kind + (f"; {t['note']}" if t.get("note") else "")
        lines.append(f"| {name(b)} | {b['size']} | {b['recognise']} | {b['status']} | "
                     f"{b['screen']['chip']}, {b['screen']['width']} x {b['screen']['height']} | {touch} |")
    lines += ["", "Not listed? It may still work: install, then report it.", ""]
    TOUCH_TEXT = {"xpt2046_own": "XPT2046 (resistive, own SPI bus)",
                  "xpt2046_shared": "XPT2046 (resistive, shares the screen's SPI bus)",
                  "gt911": "GT911 (capacitive, I2C)", "none": "none"}
    for b in boards:
        s, t = b["screen"], b.get("touch", {})
        ids = ", ".join(f"{k.upper()} 0x{s[k]:06X}" for k in ("rddid", "rdid4") if k in s) or "none"
        lines += [
            f"## {name(b)}",
            "",
            f"- **Id:** `{b['id']}` · **Status:** {_status_text(b)}",
            f"- **Screen:** {s['chip']}, start-up `{s['init']}`, {s['width']} x {s['height']}; "
            f"SPI SCLK {s['sclk']}, MOSI {s['mosi']}, MISO {s['miso']}, CS {s['cs']}, DC {s['dc']}; "
            f"screen ID {ids} ({s.get('id_source', 'unknown')})",
            f"- **Backlight:** GPIO{b['backlight']['pin']}",
            f"- **Touch:** {TOUCH_TEXT.get(t.get('type', 'none'), t.get('type'))}"
            + (f" ({t['note']})" if t.get("note") else "")
            + "".join(f", {k.upper()} {t[k]}" for k in ("sclk", "mosi", "miso", "cs", "irq", "sda", "scl", "rst")
                      if k in t),
            "- **Module connector (BME280):** " + (f"{b['i2c']['connector']} SDA GPIO{b['i2c']['sda']}, "
                                                    f"SCL GPIO{b['i2c']['scl']}" if "i2c" in b else "not mapped yet"),
            f"- **RGB LED:** GPIO{b['led']['r']}/{b['led']['g']}/{b['led']['b']} · **Light sensor:** "
            + (f"GPIO{b['ldr']['gpio']}" if "ldr" in b else "none")
            + (f" · **SD card CS:** GPIO{b['sd']['cs']}" if "sd" in b else ""),
            f"- **v0.4 add-ons:** {'tested' if b.get('addons_v04') else 'not tested on this board'}",
            "- **Sources:** " + "; ".join(b.get("sources", [])),
        ]
        if b.get("reports"):
            lines.append("- **Reports:** " + ", ".join(f"#{n}" for n in b["reports"]))
        lines.append("")
    return "\n".join(lines)


def summary_table(data, link):
    """The short board table for README.md and docs/HARDWARE.md (link: path to BOARDS.md from there)."""
    status = {"tested": "**tested**", "reported": "reported working", "mapped": "untested"}
    lines = ["| Board (number on the back) | Screen | Touch | Status |", "|---|---|---|---|"]
    for b in data["board"]:
        t = b.get("touch", {})
        touch = {"xpt2046_own": "resistive", "xpt2046_shared": "resistive", "gt911": "capacitive",
                 "none": "not supported yet" if t.get("chip") else "none"}[t.get("type", "none")]
        s = b["screen"]
        lines.append(f"| {name(b)} | {s['width']} x {s['height']} | {touch} | {status[b['status']]} |")
    lines.append("")
    lines.append(f"Pins, connectors and sources for each board: [board list]({link}).")
    return "\n".join(lines) + "\n"


def with_section(path, table):
    """The file's text with the generated board table between BEGIN and END (unchanged if no markers)."""
    with open(path, encoding="utf-8") as f:
        text = f.read()
    if BEGIN not in text or END not in text:
        return text
    head, rest = text.split(BEGIN, 1)
    tail = rest.split(END, 1)[1]
    return f"{head}{BEGIN}\n{table}{END}{tail}"


def site_json(data):
    out = []
    for b in data["board"]:
        t = b.get("touch", {})
        out.append({"id": b["id"], "model": b["model"], "name": name(b), "size": b["size"],
                    "recognise": b["recognise"], "status": b["status"], "chip": b["screen"]["chip"],
                    "touch_note": t.get("note", ""), "photo": os.path.basename(b["photo"]) if b.get("photo") else "",
                    "selectable": complete(b)})
    return json.dumps({"fallback": data["fallback"], "boards": out}, indent=1, ensure_ascii=False) + "\n"


def outputs_for(data):
    """{path: content} of every generated file."""
    return {HEADER: header(data), DOC: doc(data), SITE_JSON: site_json(data),
            HARDWARE_MD: with_section(HARDWARE_MD, summary_table(data, "BOARDS.md")),
            README: with_section(README, summary_table(data, "docs/BOARDS.md"))}


def main():
    data = load()
    for path, text in outputs_for(data).items():
        with open(path, "w", encoding="utf-8") as f:
            f.write(text)
        print(f"[gen_boards] wrote {os.path.relpath(path, ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
