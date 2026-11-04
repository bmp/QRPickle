"""Tests for png_to_bin_lvgl9.py (same cases as test/js/logo_test.mjs).

Run: python3 -m unittest scripts/test_png_to_bin_lvgl9.py
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(__file__))
import png_to_bin_lvgl9 as conv  # noqa: E402


class FitAndEncode(unittest.TestCase):
    def test_fit_box(self):
        self.assertEqual(conv.fit_box(100, 50, 60, 60), (60, 30))
        self.assertEqual(conv.fit_box(10, 10, 60, 60), (60, 60))
        self.assertEqual(conv.fit_box(1000, 1, 60, 60), (60, 1))
        self.assertEqual(conv.fit_box(120, 45, 60, 60), (60, 23))  # 22.5 rounds up, as in JS

    def test_encode(self):
        rgba = bytes([255, 0, 0, 255, 0, 255, 0, 128, 0, 0, 255, 0, 255, 255, 255, 255])
        self.assertEqual(conv.encode_rgb565a8(rgba, 2, 2).hex(),
                         "191400000200020004000000" + "00f8e0071f00ffff" + "ff8000ff")

    def test_block_note(self):
        self.assertIn("1 LittleFS block", conv.block_note(36, 36))
        self.assertIn("WARNING", conv.block_note(40, 40))


if __name__ == "__main__":
    unittest.main()
