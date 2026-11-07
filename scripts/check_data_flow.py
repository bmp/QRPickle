#!/usr/bin/env python3
"""Pre-build guard: docs/DATA_FLOW.md must match src/services/sources.h.

The jobs table in the doc is generated from the code (scripts/gen_data_flow.py). This fails the
build (or exits 1 when run directly) when someone changed a refresh interval, retry, task stack or
screen in sources.h without regenerating the doc, so the doc can't drift from the code.
"""
import os
import sys

try:
    _HERE = os.path.dirname(os.path.abspath(__file__))
except NameError:  # PlatformIO runs extra scripts without __file__
    Import("env")  # noqa: F821
    _HERE = os.path.join(env.subst("$PROJECT_DIR"), "scripts")  # noqa: F821
sys.path.insert(0, _HERE)
import gen_data_flow  # noqa: E402


def _rows(table):
    rows = {}
    for line in table.splitlines():
        if line.startswith("| ") and not line.startswith("| Job ") and not line.startswith("|---"):
            rows[line.split("|")[1].strip()] = line
    return rows


def compare(doc, table):
    """Errors (empty when the doc's generated section equals `table`)."""
    if gen_data_flow.BEGIN not in doc or gen_data_flow.END not in doc:
        return ["docs/DATA_FLOW.md: generated-table markers missing"]
    section = doc.split(gen_data_flow.BEGIN, 1)[1].split(gen_data_flow.END, 1)[0].strip("\n") + "\n"
    if section == table:
        return []
    have, want = _rows(section), _rows(table)
    errors = []
    for name in sorted(set(have) | set(want)):
        if have.get(name) != want.get(name):
            errors.append(f"row '{name}': doc {'missing' if name not in have else 'differs'}"
                          if name in want else f"row '{name}': in the doc but not in sources.h")
    return errors or ["table layout differs"]


def check(root):
    with open(os.path.join(root, "src", "services", "sources.h")) as f:
        table = gen_data_flow.render(gen_data_flow.parse(f.read()))
    with open(os.path.join(root, "docs", "DATA_FLOW.md")) as f:
        return compare(f.read(), table)


def report(errors):
    if errors:
        print("[check_data_flow] FAILED: docs/DATA_FLOW.md doesn't match src/services/sources.h:")
        for e in errors:
            print("  - " + e)
        print("  Fix: python3 -I scripts/gen_data_flow.py (then review the doc's text around the table)")
    else:
        print("[check_data_flow] OK")


try:
    Import("env")  # noqa: F821  (PlatformIO pre-build)
    _errors = check(env.subst("$PROJECT_DIR"))  # noqa: F821
    report(_errors)
    if _errors:
        env.Exit(1)  # noqa: F821
except NameError:
    if __name__ == "__main__":
        _errors = check(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        report(_errors)
        sys.exit(1 if _errors else 0)
