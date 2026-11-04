"""Checks of tables in the C++ sources that the compiler can't check (docs/TESTING.md):
- every setting in config::Config is in the NVS table (config.cpp) and the JSON table
  (config_json.cpp), NVS keys are unique and at most 15 characters (the NVS limit);
- every theme (theme.cpp) defines all base colour tokens (theme.h).
Run: python3 -m unittest scripts/test_source_tables.py"""
import os
import re
import unittest

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
# Deliberate gaps: the APRS macros have their own NVS keys; the admin password is never in JSON.
NOT_IN_NVS_TABLE = {"aprs_macros"}
NOT_IN_JSON = {"admin_password"}


def read(path):
    with open(os.path.join(ROOT, path), encoding="utf-8") as f:
        return f.read()


def config_problems(header, nvs_src, json_src):
    struct = header[header.index("struct Config {"):]
    struct = struct[:struct.index("};")]
    types = r"(?:char|bool|uint8_t|int8_t|uint16_t|float|int)"
    members = re.findall(r"^\s*" + types + r"\s+(\w+)\s*(?:\[[^\]]*\])*\s*;", struct, re.M)
    nvs = re.findall(r'FIELD\("([^"]+)",\s*\w+,\s*(\w+)\)', nvs_src)
    js = re.findall(r'(?:FIELD|SECRET)\("([^"]+)",\s*(?:\w+,\s*)?(\w+)\)', json_src)
    problems = [f"{m}: not in the NVS table (config.cpp)" for m in members
                if m not in {x[1] for x in nvs} and m not in NOT_IN_NVS_TABLE]
    problems += [f"{m}: not in the JSON table (config_json.cpp)" for m in members
                 if m not in {x[1] for x in js} and m not in NOT_IN_JSON]
    keys = [k for k, _ in nvs]
    problems += [f"NVS key {k!r} longer than 15 characters" for k in keys if len(k) > 15]
    problems += [f"NVS key {k!r} used twice" for k in sorted({k for k in keys if keys.count(k) > 1})]
    jkeys = [k for k, _ in js]
    problems += [f"JSON key {k!r} used twice" for k in sorted({k for k in jkeys if jkeys.count(k) > 1})]
    return problems


def theme_problems(header, src):
    enum = header[header.index("enum ThemeToken"):]
    base = re.findall(r"(COLOR_\w+)", enum[:enum.index("// Semantic tokens")])
    body = src[src.index("switch (theme_id)"):]
    body = body[:body.index("theme_get_name")] if "theme_get_name" in body else body  # the colours only
    # Theme blocks: "case THEME_X:" and the Classic "default: // THEME_CLASSIC" (not the inner
    # "default: break;" of each theme's token switch).
    blocks = re.split(r"\n\s*(?=case THEME_\w+:|default:\s*//\s*THEME_CLASSIC)", body)
    problems = []
    for b in blocks:
        m = re.match(r"\s*(case (THEME_\w+):|default:\s*//\s*THEME_CLASSIC)", b)
        if not m:
            continue
        name = m.group(2) or "default (Classic)"
        have = set(re.findall(r"case (COLOR_\w+):", b))
        problems += [f"{name}: no colour for {t}" for t in base if t not in have]
    return problems


class ConfigTables(unittest.TestCase):
    def test_every_setting_is_in_both_tables(self):
        self.assertEqual(config_problems(read("src/config/config.h"), read("src/config/config.cpp"),
                                         read("src/config/config_json.cpp")), [])

    def test_the_check_finds_a_missing_entry_and_a_long_key(self):
        header = "struct Config {\n  bool a;\n  char b[8];\n};"
        problems = config_problems(header, 'FIELD("a_key_far_too_long", BOOL, a)', 'FIELD("a", BOOL, a)')
        self.assertIn("b: not in the NVS table (config.cpp)", problems)
        self.assertIn("b: not in the JSON table (config_json.cpp)", problems)
        self.assertIn("NVS key 'a_key_far_too_long' longer than 15 characters", problems)


class ThemeTables(unittest.TestCase):
    def test_every_theme_defines_every_base_colour(self):
        self.assertEqual(theme_problems(read("src/ui/theme.h"), read("src/ui/theme.cpp")), [])

    def test_the_check_finds_a_missing_colour(self):
        header = "enum ThemeToken {\n COLOR_A,\n COLOR_B,\n // Semantic tokens\n COLOR_C };"
        src = ("switch (theme_id) {\n case THEME_X:\n"
               " switch (token) {\n case COLOR_A: return 1;\n }\n"
               " default: // THEME_CLASSIC\n switch (token) {\n"
               " case COLOR_A: return 1;\n case COLOR_B: return 2;\n }\n}")
        self.assertEqual(theme_problems(header, src), ["THEME_X: no colour for COLOR_B"])


if __name__ == "__main__":
    unittest.main()
