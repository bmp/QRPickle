#!/usr/bin/env python3
"""Tests for scripts/check_boards.py and the conflict table in scripts/gen_boards.py, on fixture
boards (not the real list). Run:  python3 -I scripts/test_check_boards.py  (exit 1 on failure; CI)."""
import copy
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_boards  # noqa: E402
import gen_boards  # noqa: E402

BASE = {
    "id": "A", "model": "ESP32-TEST", "size": '2.8"', "recognise": "test", "status": "mapped",
    "sources": ["test"], "chip_outputs": [12],
    "screen": {"chip": "x", "init": "ili9341", "width": 320, "height": 240, "swap_xy": True, "mirror_x": False,
               "mirror_y": False, "bgr": True, "invert": False, "sclk": 14, "mosi": 13, "miso": 12, "cs": 15, "dc": 2},
    "backlight": {"pin": 21, "active_high": True},
    "led": {"r": 4, "g": 16, "b": 17, "active_low": True},
}


def board(**changes):
    b = copy.deepcopy(BASE)
    for k, v in changes.items():
        b[k] = v
    return b


def data(*boards, fallback="A"):
    return {"schema": 1, "fallback": fallback, "board": list(boards)}


def errs(d):
    return check_boards.check(d, root="/nonexistent")


def has(d, text):
    e = errs(d)
    assert any(text in x for x in e), f"expected an error containing {text!r}, got {e}"


FAILED = []


def case(fn):
    try:
        fn()
        print(f"PASS {fn.__name__}")
    except AssertionError as e:
        FAILED.append(fn.__name__)
        print(f"FAIL {fn.__name__}: {e}")
    return fn


@case
def test_valid_fixture_passes():
    assert errs(data(board())) == [], errs(data(board()))


@case
def test_duplicate_id_rejected():
    has(data(board(), board()), "duplicate id 'A'")


@case
def test_flash_pin_rejected():
    has(data(board(backlight={"pin": 7, "active_high": True})), "backlight.pin = 7 is not a usable")


@case
def test_pin_above_39_rejected():
    has(data(board(led={"r": 40, "g": 16, "b": 17})), "led.r = 40")


@case
def test_input_only_output_rejected():
    has(data(board(backlight={"pin": 35, "active_high": True})), "input-only")


@case
def test_pin_used_twice_rejected():
    has(data(board(backlight={"pin": 15, "active_high": True})), "GPIO15 used twice")


@case
def test_shared_touch_miso_allowed():
    b = board(touch={"type": "xpt2046_shared", "cs": 33, "irq": 36, "miso": 12})
    assert errs(data(b)) == [], errs(data(b))


@case
def test_tested_needs_note():
    has(data(board(status="tested")), "needs a status_note")


@case
def test_mapped_needs_sources():
    has(data(board(sources=[])), "'sources' missing")


@case
def test_incomplete_board_flagged():
    b = board()
    del b["backlight"]
    has(data(b), "could never be selected")


@case
def test_unknown_fallback_rejected():
    has(data(board(), fallback="Z"), "fallback 'Z'")


@case
def test_led_pins_must_match():
    has(data(board(), board(id="B", led={"r": 4, "g": 16, "b": 22})), "LED pins differ")


@case
def test_screen_pins_must_match():
    other = board(id="B")
    other["screen"] = dict(other["screen"], dc=4)
    has(data(board(), other), "screen SPI pins differ")


@case
def test_conflict_chip_output_vs_backlight():
    # A: a touch chip drives GPIO21 (interrupt output), backlight on 27.
    a = board(id="A", backlight={"pin": 27, "active_high": True}, chip_outputs=[12, 21])
    # B: backlight on GPIO21, no chip drives anything but the screen's data out.
    b = board(id="B")
    conf = gen_boards.conflicts([a, b])
    assert conf[0][1] & (1 << 21), "B chosen on A must flag GPIO21"
    assert not conf[1][0] & (1 << 21), "A chosen on B: GPIO21 is only a transistor input on B"
    assert not conf[0][0] & (1 << 21), "a board never conflicts with itself on GPIO21"


@case
def test_conflict_ignores_i2c_and_input_only():
    a = board(id="A", chip_outputs=[27, 36])
    b = board(id="B", i2c={"sda": 27, "scl": 22}, touch={"type": "xpt2046_own", "sclk": 25, "mosi": 32,
                                                           "miso": 39, "cs": 33, "irq": 36})
    assert gen_boards.conflicts([a, b])[0][1] == 0


@case
def test_conflict_gt911_reset_and_int_are_outputs():
    a = board(id="A", chip_outputs=[12, 25])
    b = board(id="B", backlight={"pin": 27, "active_high": True},
              touch={"type": "gt911", "sda": 33, "scl": 32, "rst": 25, "irq": 21, "addr": 0x5D})
    assert gen_boards.conflicts([a, b])[0][1] & (1 << 25)


@case
def test_real_list_passes():
    d = gen_boards.load()
    assert check_boards.check(d) == [], check_boards.check(d)


@case
def test_header_has_every_board_and_fallback():
    d = gen_boards.load()
    h = gen_boards.header(d)
    for b in d["board"]:
        assert f'"{b["id"]}"' in h, b["id"]
    assert f"BOARD_FALLBACK = {[b['id'] for b in d['board']].index(d['fallback'])};" in h


if __name__ == "__main__":
    print(f"{'FAILED: ' + ', '.join(FAILED) if FAILED else 'all passed'}")
    sys.exit(1 if FAILED else 0)
