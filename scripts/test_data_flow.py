"""Tests for the data-flow doc generator and its check (scripts/gen_data_flow.py, check_data_flow.py)."""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_data_flow  # noqa: E402
import gen_data_flow  # noqa: E402

# One X(...) row per line in sources.h; built from short pieces here so the file stays within the
# line limit.
SAMPLE = (
    "#define QRP_JOBS(X) \\\n"
    '    X(WEATHER, "weather", 7200000UL, 0, "OpenWeather, HTTPS", "every 2 h", "5 min", '
    '"main loop", "RAM", "Weather") \\\n'
    '    X(SOLAR,   "solar",   10800000UL, 6144, "hamqsl.com, HTTPS", "after each 3-hourly update", '
    '"15 min", "solar_fetch", "RAM, RTC copy", "Band conditions, Dashboard")\n'
)


class DataFlowTest(unittest.TestCase):
    def test_parse_rows(self):
        rows = gen_data_flow.parse(SAMPLE)
        self.assertEqual(len(rows), 2)
        self.assertEqual(rows[1]["name"], "solar")
        self.assertEqual(rows[1]["stack"], 6144)
        self.assertEqual(rows[1]["screens"], "Band conditions, Dashboard")  # commas inside quotes

    def test_render_has_one_line_per_row(self):
        table = gen_data_flow.render(gen_data_flow.parse(SAMPLE))
        lines = [ln for ln in table.splitlines() if ln.startswith("| ")]
        self.assertEqual(len(lines), 3)  # header + 2 rows
        solar = "| solar | hamqsl.com, HTTPS | after each 3-hourly update | 15 min | `solar_fetch`, 6 KB |"
        self.assertIn(solar, table)
        self.assertIn("| weather | OpenWeather, HTTPS | every 2 h | 5 min | main loop |", table)

    def test_check_passes_when_doc_matches(self):
        table = gen_data_flow.render(gen_data_flow.parse(SAMPLE))
        doc = "intro\n" + gen_data_flow.BEGIN + "\n" + table + gen_data_flow.END + "\nmore\n"
        self.assertEqual(check_data_flow.compare(doc, table), [])

    def test_check_names_a_changed_row(self):
        table = gen_data_flow.render(gen_data_flow.parse(SAMPLE))
        old = table.replace("every 2 h", "every 3 h")
        doc = gen_data_flow.BEGIN + "\n" + old + gen_data_flow.END + "\n"
        errors = check_data_flow.compare(doc, table)
        self.assertTrue(errors and "weather" in errors[0])

    def test_check_reports_missing_markers(self):
        self.assertTrue(check_data_flow.compare("no table here", "| x |\n"))


if __name__ == "__main__":
    unittest.main()
