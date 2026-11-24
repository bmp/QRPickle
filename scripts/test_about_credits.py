"""Tests for scripts/about_credits.py.  Run: python3 -m unittest scripts/test_about_credits.py"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import about_credits  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


class AboutCredits(unittest.TestCase):
    def test_libraries_from_the_real_notices(self):
        with open(os.path.join(ROOT, "THIRD_PARTY_NOTICES.md"), encoding="utf-8") as f:
            text = about_credits.credits_section(f.read())
        for name in ("LVGL 9.6", "qrcodegen", "ArduinoJson", "ESPAsyncWebServer", "XPT2046_Touchscreen", "Sgp4",
                     "ESP-IDF 5.5", "TFT_eSPI"):
            self.assertIn(name, text)
        self.assertNotIn("Atkinson", text)  # fonts and images are credited by about.txt itself
        self.assertNotIn("](", text)        # markdown links flattened
        self.assertNotIn("`", text)

    def test_markdown_flattened(self):
        notices = ("## In the firmware\n\n| Component | Licence | Copyright / source | Licence text |\n"
                   "|---|---|---|---|\n"
                   "| Foo `lib` 1.0 | MIT | © Bar, [bar/foo](https://x) | in the library |\n"
                   "| A font | OFL | © X | t |\n\n## In the documentation only\n| Doc thing | X | Y |\n")
        text = about_credits.credits_section(notices)
        self.assertIn("- Foo lib 1.0: MIT, © Bar, bar/foo", text)
        self.assertNotIn("A font", text)
        self.assertNotIn("Doc thing", text)


if __name__ == "__main__":
    unittest.main()
