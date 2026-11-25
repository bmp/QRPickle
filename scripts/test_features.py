"""Tests for scripts/gen_features.py (the feature list from docs/features.toml)."""
import os
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_features as g  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")


class Features(unittest.TestCase):
    def test_readme_is_current(self):
        self.assertEqual(g.run(ROOT, check=True), [])

    def test_home_lists_every_feature_with_a_home_line(self):
        d = g.load(ROOT)
        out = g.home_html(d)
        n = sum("home" in i for i in d["screen"] + [i for gr in d["group"] for i in gr["item"]])
        self.assertEqual(out.count("<li>"), n)
        self.assertIn("<strong>DX Cluster:</strong>", out)
        self.assertNotIn("{{", out)

    def test_markdown_to_html(self):
        self.assertEqual(g.md_inline("a [b](https://x.y/z) **c** `d` <e> & f"),
                         'a <a href="https://x.y/z">b</a> <strong>c</strong> <code>d</code> &lt;e&gt; &amp; f')

    def test_edit_in_source_without_regenerating_fails(self):
        with tempfile.TemporaryDirectory() as d:
            os.makedirs(os.path.join(d, "docs"))
            shutil.copy(os.path.join(ROOT, "README.md"), d)
            with open(os.path.join(ROOT, g.SOURCE), encoding="utf-8") as f:
                src = f.read()
            with open(os.path.join(d, g.SOURCE), "w", encoding="utf-8") as f:
                f.write(src.replace("Live DX spots", "Live DX spots and more", 1))
            self.assertTrue(any("out of date" in e for e in g.run(d, check=True)))

    def test_home_without_home_name_is_an_error(self):
        d = {"intro": "", "screen": [{"name": "A", "shows": "s", "data": "d", "home": "x"}], "group": []}
        self.assertEqual(len(g.validate(d)), 1)


if __name__ == "__main__":
    unittest.main()
