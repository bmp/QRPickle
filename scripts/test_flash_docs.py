"""Tests for scripts/gen_flash_docs.py (flash layout docs from partitions.csv)."""
import os
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_flash_docs as g  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")


class FlashDocs(unittest.TestCase):
    def test_install_files_follow_the_csv(self):
        files = dict(g.install_files(g.layout(ROOT)))
        self.assertEqual(files["bootloader.bin"], 0x1000)
        self.assertEqual(files["boot_app0.bin"], 0xE000)
        self.assertEqual(files["firmware.bin"], 0x10000)
        self.assertEqual(files["littlefs.bin"], 0x390000)

    def test_repo_docs_are_current(self):
        self.assertEqual(g.run(ROOT, check=True), [])

    def test_wrong_address_in_a_command_is_reported(self):
        with tempfile.TemporaryDirectory() as d:
            os.makedirs(os.path.join(d, "docs"))
            shutil.copy(os.path.join(ROOT, "partitions.csv"), d)
            for doc in g.SECTIONS:
                shutil.copy(os.path.join(ROOT, doc), os.path.join(d, doc))
            with open(os.path.join(d, "docs", "SETUP.md"), "w") as f:
                f.write("esptool write_flash 0x20000 firmware.bin\n")
            errs = g.run(d, check=True)
            self.assertEqual(len(errs), 1)
            self.assertIn("firmware.bin at 0x20000", errs[0])

    def test_stale_table_is_reported(self):
        with tempfile.TemporaryDirectory() as d:
            os.makedirs(os.path.join(d, "docs"))
            shutil.copy(os.path.join(ROOT, "partitions.csv"), d)
            for doc in g.SECTIONS:
                shutil.copy(os.path.join(ROOT, doc), os.path.join(d, doc))
            with open(os.path.join(d, "partitions.csv"), "a") as f:
                f.write("\nextra, data, fat, 0x400000, 0x1000,\n")
            g.INFO.setdefault("extra", ("x", "x", "x"))
            self.assertTrue(any("out of date" in e for e in g.run(d, check=True)))


if __name__ == "__main__":
    unittest.main()
