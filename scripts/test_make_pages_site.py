"""Tests for make_pages_site.py: partition offsets, the per-version installer files and sizes.
Run: python3 -m unittest scripts/test_make_pages_site.py"""
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(__file__))
import make_pages_site as site  # noqa: E402

NAMES = ["bootloader.bin", "partitions.bin", "boot_app0.bin", "firmware.bin", "littlefs.bin"]


class Site(unittest.TestCase):
    def test_partition_offsets_match_the_fixed_layout(self):
        p = site.partition_offsets()
        # The README, the installer and deployed devices depend on these (CLAUDE.md, "Flash layout").
        self.assertEqual((p["otadata"], p["app0"], p["app1"], p["spiffs"]), (0xE000, 0x10000, 0x1D0000, 0x390000))
        self.assertEqual([o for _, o in site.PARTS], [0x1000, 0x8000, 0xE000, 0x10000, 0x390000])

    def test_write_version_manifest_and_sizes(self):
        with tempfile.TemporaryDirectory() as d:
            files = {}
            for i, name in enumerate(NAMES):
                files[name] = os.path.join(d, name)
                with open(files[name], "wb") as f:
                    f.write(bytes(1000 + i))
            inst = os.path.join(d, "install")
            entry = site.write_version(inst, "v9.9.9", files, "notes")
            self.assertEqual(entry["sizes"], {n: 1000 + i for i, n in enumerate(NAMES)})
            self.assertEqual(entry["manifest"], "v9.9.9/manifest.json")
            man = json.load(open(os.path.join(inst, "v9.9.9", "manifest.json")))
            self.assertTrue(man["new_install_prompt_erase"])  # never erase without asking
            self.assertEqual([(p["path"], p["offset"]) for p in man["builds"][0]["parts"]], site.PARTS)

    def test_versions_and_notes(self):
        self.assertEqual(site.parse_version("v0.2.6"), (0, 2, 6))
        self.assertIsNone(site.parse_version("0.2.6"))
        with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as f:
            f.write("v0.2.6: line one\nline two\n")
        try:
            self.assertEqual(site.full_notes(f.name, "v0.2.6"), "line one\nline two")
        finally:
            os.unlink(f.name)


if __name__ == "__main__":
    unittest.main()
