#!/usr/bin/env python3
"""Print the CHANGELOG.md section for a version (the release notes), without its '## vX.Y.Z' line.

    python3 scripts/release_notes.py v0.2.2 > release_notes.txt

Exits with an error if the section is missing or empty, so a release can't go out without notes.
The first line of the section is the one-line summary devices show (first 127 characters).
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def notes_for(version, changelog):
    m = re.search(rf"(?ms)^## {re.escape(version)}\b[^\n]*\n(.*?)(?=^## v|\Z)", changelog)
    return m.group(1).strip() if m else ""


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: release_notes.py vX.Y.Z")
    version = sys.argv[1]
    with open(os.path.join(ROOT, "CHANGELOG.md"), encoding="utf-8") as f:
        notes = notes_for(version, f.read())
    if not notes:
        sys.exit(f"CHANGELOG.md has no section '## {version}': add the release notes first")
    print(notes)


if __name__ == "__main__":
    main()
