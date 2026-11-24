#!/usr/bin/env python3
"""Pre-build guard for the board list (boards/boards.toml).

Fails the build (or exits 1 when run directly) when:
  - an entry breaks a rule (ids, required fields, pins, statuses, sources, photos), or
  - a generated file (src/hw/boards_gen.h, docs/BOARDS.md, data/www/boards.json) no longer matches
    what scripts/gen_boards.py writes from the TOML.
The pin-conflict table is computed here too (gen_boards.conflicts), so it can't be forgotten: no one
checks it by hand.
"""
import os
import sys

try:
    _HERE = os.path.dirname(os.path.abspath(__file__))
except NameError:  # PlatformIO runs extra scripts without __file__
    Import("env")  # noqa: F821
    _HERE = os.path.join(env.subst("$PROJECT_DIR"), "scripts")  # noqa: F821
sys.path.insert(0, _HERE)
import gen_boards  # noqa: E402

FLASH_PINS = set(range(6, 12))  # connected to the module's flash: never usable
INPUT_ONLY = set(range(34, 40))
STATUSES = set(gen_boards.STATUS)


def _pins(b):
    """(role, pin) for every pin the board's settings use."""
    s, t = b.get("screen", {}), b.get("touch", {})
    roles = [(f"screen.{k}", s.get(k)) for k in ("sclk", "mosi", "miso", "cs", "dc")]
    roles.append(("backlight.pin", b.get("backlight", {}).get("pin")))
    roles += [(f"led.{k}", b.get("led", {}).get(k)) for k in ("r", "g", "b")]
    roles += [(f"touch.{k}", t.get(k)) for k in ("sclk", "mosi", "miso", "cs", "irq", "sda", "scl", "rst")]
    roles += [(f"i2c.{k}", b.get("i2c", {}).get(k)) for k in ("sda", "scl")]
    roles.append(("ldr.gpio", b.get("ldr", {}).get("gpio")))
    roles.append(("sd.cs", b.get("sd", {}).get("cs")))
    return [(r, p) for r, p in roles if p is not None]


def check(data, root=gen_boards.ROOT):
    errors = []
    boards = data.get("board", [])
    if not boards:
        return ["boards.toml: no [[board]] entries"]
    if len(boards) > gen_boards.BOARD_MAX:
        errors.append(f"boards.toml: {len(boards)} boards, at most {gen_boards.BOARD_MAX}")
    ids = [b.get("id", "") for b in boards]
    for i in sorted({i for i in ids if ids.count(i) > 1}):
        errors.append(f"duplicate id '{i}'")
    if data.get("fallback") not in ids:
        errors.append(f"fallback '{data.get('fallback')}' is not a board id")

    for b in boards:
        bid = b.get("id", "?")
        for k in ("id", "model", "size", "recognise", "status"):
            if not b.get(k):
                errors.append(f"{bid}: '{k}' missing")
        if b.get("status") not in STATUSES:
            errors.append(f"{bid}: status '{b.get('status')}' is not one of {sorted(STATUSES)}")
        if b.get("status") in ("tested", "reported") and not b.get("status_note"):
            errors.append(f"{bid}: status {b.get('status')} needs a status_note (when, what was confirmed)")
        if not b.get("sources"):
            errors.append(f"{bid}: 'sources' missing (where the pins come from)")
        s = b.get("screen", {})
        if s.get("init") not in gen_boards.INITS:
            errors.append(f"{bid}: screen.init '{s.get('init')}' unknown ({', '.join(gen_boards.INITS)})")
        if b.get("touch", {}).get("type", "none") not in gen_boards.TOUCH:
            errors.append(f"{bid}: touch.type '{b['touch'].get('type')}' unknown")
        for k in ("rddid", "rdid4"):
            if k in s and not 0 <= s[k] < (1 << 24):
                errors.append(f"{bid}: screen.{k} must be 24 bits")
        if not gen_boards.complete(b):
            errors.append(f"{bid}: screen, backlight or LED values missing (the board could never be selected)")
        if b.get("photo") and not os.path.isfile(os.path.join(root, "boards", "img", b["photo"])):
            errors.append(f"{bid}: photo boards/img/{b['photo']} not found")

        seen = {}
        shared = b.get("touch", {}).get("type") == "xpt2046_shared"
        for role, pin in _pins(b):
            if not isinstance(pin, int) or not 0 <= pin <= 39 or pin in FLASH_PINS:
                errors.append(f"{bid}: {role} = {pin} is not a usable ESP32 GPIO")
                continue
            if pin in INPUT_ONLY and role in ("screen.sclk", "screen.mosi", "screen.cs", "screen.dc",
                                              "backlight.pin", "led.r", "led.g", "led.b", "touch.sclk",
                                              "touch.mosi", "touch.cs", "touch.rst", "i2c.sda", "i2c.scl"):
                errors.append(f"{bid}: {role} = GPIO{pin} is input-only")
            if pin in seen and not (shared and {seen[pin], role} <= {"screen.miso", "touch.miso"}):
                errors.append(f"{bid}: GPIO{pin} used twice ({seen[pin]} and {role})")
            seen.setdefault(pin, role)

    # The status LED starts before the board is known (boot faults stay visible): every board must
    # have it on the same pins, or the LED would drive another board's pins.
    leds = {(b.get("led", {}).get("r"), b.get("led", {}).get("g"), b.get("led", {}).get("b"),
             b.get("led", {}).get("active_low", True)) for b in boards}
    if len(leds) > 1:
        errors.append(f"LED pins differ between boards {sorted(leds, key=str)}: the LED can't start before "
                      "detection any more (src/main.cpp)")
    return errors


def drift(data):
    """Generated files that differ from what gen_boards.py writes."""
    out = []
    for path, text in gen_boards.outputs_for(data).items():
        try:
            with open(path, encoding="utf-8") as f:
                same = f.read() == text
        except FileNotFoundError:
            same = False
        if not same:
            out.append(f"{os.path.relpath(path, gen_boards.ROOT)} is out of date: run python3 -I scripts/gen_boards.py")
    return out


def main():
    try:
        data = gen_boards.load()
    except Exception as e:  # noqa: BLE001 - any parse error fails the build with its message
        errors = [f"boards/boards.toml: {e}"]
    else:
        errors = check(data) or drift(data)
    if errors:
        print("[check_boards] FAILED:")
        for e in errors:
            print(f"  - {e}")
        return 1
    print(f"[check_boards] OK ({len(data['board'])} boards)")
    return 0


try:
    Import("env")  # noqa: F821  (PlatformIO pre-build)
    if main():
        env.Exit(1)  # noqa: F821
except NameError:  # run directly, or imported by scripts/test_check_boards.py
    if __name__ == "__main__":
        sys.exit(main())
