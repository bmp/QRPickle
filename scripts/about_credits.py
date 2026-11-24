#!/usr/bin/env python3
"""The web console's About tab lists the software QRPickle is built from. This writes that list from
THIRD_PARTY_NOTICES.md (the "In the firmware" table) so the two can't drift; scripts/gzip_data.py
appends it to the staged data/about.txt at every filesystem build. Fonts, icons, images and data
sources are left out here: data/about.txt credits them in its own words."""
import re

SKIP = ("font", "icon", "image", "data", "symbol", "logo")


def _plain(cell):
    cell = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", cell)  # [text](url) -> text
    return cell.replace("`", "").replace("**", "").strip()


def firmware_rows(notices):
    """(component, licence, holder) for each row of the "In the firmware" table."""
    section = notices.split("## In the firmware", 1)[1].split("\n## ", 1)[0] if "## In the firmware" in notices else ""
    rows = []
    for line in section.splitlines():
        if not line.startswith("| ") or line.startswith("| Component") or line.startswith("|---"):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split(" | ")]
        if len(cells) >= 3:
            rows.append((_plain(cells[0]), _plain(cells[1]), _plain(cells[2])))
    return rows


def credits_section(notices):
    lines = ["", "Software (from THIRD_PARTY_NOTICES.md):"]
    for component, licence, holder in firmware_rows(notices):
        if any(k in component.lower() for k in SKIP):
            continue
        lines.append(f"- {component}: {licence}, {holder}")
    return "\n".join(lines) + "\n"
