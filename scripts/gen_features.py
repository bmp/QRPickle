#!/usr/bin/env python3
"""The feature list, from docs/features.toml (the single source).

  README.md           the "Features and Data Sources" section between the generated markers
  website home page   home_html(), called by scripts/make_pages_site.py (fills {{FEATURES}})

`--check` (also scripts/check_features.py, before every build and in CI) fails when README.md is
out of date. Run: python3 -I scripts/gen_features.py [--check]
"""
import html
import os
import re
import sys
import tomllib

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SOURCE = "docs/features.toml"
README = "README.md"
BEGIN = "<!-- BEGIN generated: features (scripts/gen_features.py from docs/features.toml) -->"
END = "<!-- END generated: features -->"


def load(root=ROOT):
    with open(os.path.join(root, SOURCE), "rb") as f:
        return tomllib.load(f)


def readme_section(d):
    out = [d["intro"], "", "| Screen | What it shows | Data from |", "| :--- | :--- | :--- |"]
    out += [f"| **{s['name']}** | {s['shows']} | {s['data']} |" for s in d["screen"]]
    for g in d["group"]:
        out += ["", f"**{g['title']}**"]
        out += [f"* **{i['name']}:** {i['text']}" if "name" in i else f"* {i['text']}" for i in g["item"]]
    return "\n".join(out)


def md_inline(text):
    """The Markdown the list uses (links, **bold**, `code`) as HTML; everything else escaped."""
    t = html.escape(text, quote=False)
    t = re.sub(r"\[([^\]]+)\]\(([^)\s]+)\)", lambda m: f'<a href="{html.escape(m.group(2))}">{m.group(1)}</a>', t)
    t = re.sub(r"\*\*(.+?)\*\*", r"<strong>\1</strong>", t)
    return re.sub(r"`([^`]+)`", r"<code>\1</code>", t)


def home_html(d):
    items = d["screen"] + [i for g in d["group"] for i in g["item"]]
    lines = ['<ul class="features">']
    lines += [f"  <li><strong>{md_inline(i['home_name'])}:</strong> {md_inline(i['home'])}</li>"
              for i in items if "home" in i]
    return "\n".join(lines + ["</ul>"])


def validate(d):
    errors = []
    for s in d.get("screen", []):
        errors += [f"screen '{s.get('name')}': '{k}' missing" for k in ("name", "shows", "data") if k not in s]
    for g in d.get("group", []):
        errors += [f"group '{g.get('title')}': item without text" for i in g.get("item", []) if "text" not in i]
    for i in d.get("screen", []) + [i for g in d.get("group", []) for i in g.get("item", [])]:
        if ("home" in i) != ("home_name" in i):
            errors.append(f"'{i.get('name')}': home and home_name go together")
    return errors


def run(root=ROOT, check=False):
    d = load(root)
    errors = validate(d)
    if errors:
        return errors
    path = os.path.join(root, README)
    with open(path, encoding="utf-8") as f:
        text = f.read()
    if BEGIN not in text or END not in text:
        return [f"{README}: feature markers missing"]
    head, rest = text.split(BEGIN, 1)
    new = head + BEGIN + "\n" + readme_section(d) + "\n" + END + rest.split(END, 1)[1]
    if new != text:
        if check:
            return [f"{README}: feature list out of date (edit {SOURCE}, run python3 -I scripts/gen_features.py)"]
        with open(path, "w", encoding="utf-8") as f:
            f.write(new)
    return []


if __name__ == "__main__":
    errs = run(check="--check" in sys.argv)
    for e in errs:
        print(f"[features] {e}", file=sys.stderr)
    sys.exit(1 if errs else 0)
