#!/usr/bin/env python3
"""Writes the jobs table of docs/DATA_FLOW.md from src/services/sources.h (the one place in the code
that lists every background data source and job). scripts/check_data_flow.py fails the build when
the doc no longer matches. Run after changing sources.h:  python3 -I scripts/gen_data_flow.py
"""
import os
import re
import sys

BEGIN = "<!-- BEGIN generated: sources (scripts/gen_data_flow.py) -->"
END = "<!-- END generated: sources -->"
FIELDS = ["id", "name", "interval_ms", "stack", "connection", "refresh", "retry", "task", "kept", "screens"]
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCES_H = os.path.join(ROOT, "src", "services", "sources.h")
DOC = os.path.join(ROOT, "docs", "DATA_FLOW.md")

_ROW = re.compile(r"^\s*X\((.*)\)\s*\\?\s*$")
_ARG = re.compile(r'\s*(?:"((?:[^"\\]|\\.)*)"|([^,]+?))\s*(?:,|$)')


def parse(text):
    """Rows of the QRP_JOBS(X) table, one X(...) per line."""
    rows = []
    for line in text.splitlines():
        m = _ROW.match(line)
        if not m:
            continue
        args = [b.strip() if b else q for q, b in _ARG.findall(m.group(1))]
        if len(args) != len(FIELDS):
            raise ValueError(f"sources.h: {len(args)} fields instead of {len(FIELDS)}: {line.strip()}")
        row = dict(zip(FIELDS, args, strict=True))
        row["interval_ms"] = int(re.sub(r"[^0-9]", "", row["interval_ms"]) or 0)
        row["stack"] = int(re.sub(r"[^0-9]", "", row["stack"]) or 0)
        rows.append(row)
    return rows


def _kb(n):
    kb = n / 1024
    return f"{kb:g} KB"


def render(rows):
    out = ["| Job | Connection | Refresh | Retry | Task | Kept in | Screens |",
           "|---|---|---|---|---|---|---|"]
    for r in rows:
        task = f"`{r['task']}`, {_kb(r['stack'])}" if r["stack"] else r["task"]
        cells = [r["name"], r["connection"], r["refresh"], r["retry"], task, r["kept"], r["screens"]]
        out.append("| " + " | ".join(cells) + " |")
    return "\n".join(out) + "\n"


def write(doc_path=DOC, sources_path=SOURCES_H):
    with open(sources_path) as f:
        table = render(parse(f.read()))
    with open(doc_path) as f:
        doc = f.read()
    if BEGIN not in doc or END not in doc:
        raise SystemExit(f"{doc_path}: markers missing ({BEGIN} ... {END})")
    head, rest = doc.split(BEGIN, 1)
    _, tail = rest.split(END, 1)
    with open(doc_path, "w") as f:
        f.write(head + BEGIN + "\n" + table + END + tail)
    print(f"[gen_data_flow] wrote {len(table.splitlines()) - 2} rows into {os.path.relpath(doc_path, ROOT)}")


if __name__ == "__main__":
    write()
    sys.exit(0)
