#!/usr/bin/env python3
"""Build the PDF manual shipped in each release ZIP (README + Hardware and Wiring).

Usage (from the repo root; needs pandoc and typst):
    python3 scripts/build_manual.py --version v0.2.1 --repo bmp/QRPickle --out QRPickle_Documentation_v0.2.1.pdf

Pandoc drops raw HTML when writing Typst, so the README's <img> tags (screenshot tables) are
rewritten as Markdown images first, and image paths are made relative to the repo root.
"""
import argparse
import datetime
import os
import re
import shutil
import subprocess
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMG_TAG = re.compile(r'<img\s+([^>]*?)/?>', re.I)
ATTR = re.compile(r'(\w+)="([^"]*)"')


def html_images_to_markdown(text):
    def repl(m):
        attrs = dict(ATTR.findall(m.group(1)))
        src, alt = attrs.get("src", ""), attrs.get("alt", "")
        width = attrs.get("width", "")
        size = f"{{width={width}px}}" if width.isdigit() else ""
        return f"![{alt}]({src}){size}"
    return IMG_TAG.sub(repl, text)


def rebase_links(text, subdir):
    """Markdown image/link targets in docs/<file> are relative to docs/; make them repo-relative."""
    def repl(m):
        target = m.group(2)
        if re.match(r"^(https?:|#|/)", target):
            return m.group(0)
        return f"{m.group(1)}({os.path.normpath(os.path.join(subdir, target))})"
    return re.sub(r"(!?\[[^\]]*\])\(([^)\s]+)\)", repl, text)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", required=True)
    ap.add_argument("--repo", default="bmp/QRPickle")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    header = f"""# QRPickle Field Manual & System Documentation

| Project Property | System Specification |
| :--- | :--- |
| **Firmware Package** | QRPickle Tracker Dashboard |
| **Software Version** | {a.version} |
| **Compilation Date** | {datetime.date.today().isoformat()} |
| **Target Hardware** | ESP32 Cheap Yellow Display (CYD) |
| **Source Repository** | [{a.repo}](https://github.com/{a.repo}) |
| **Primary License** | MIT License (third-party components: THIRD_PARTY_NOTICES.md) |

---

"""
    with open(os.path.join(ROOT, "README.md"), encoding="utf-8") as f:
        readme = html_images_to_markdown(f.read())
    with open(os.path.join(ROOT, "docs", "HARDWARE.md"), encoding="utf-8") as f:
        hardware = rebase_links(html_images_to_markdown(f.read()), "docs")
    doc = header + readme + "\n\n---\n\n" + hardware

    with tempfile.TemporaryDirectory(dir=ROOT) as tmp:   # inside the repo, so image paths resolve
        md, typ = os.path.join(tmp, "manual.md"), os.path.join(tmp, "manual.typ")
        with open(md, "w", encoding="utf-8") as f:
            f.write(doc)
        subprocess.run(["pandoc", md, "-f", "markdown", "-t", "typst", "-o", typ,
                        "--resource-path", ROOT], check=True)
        with open(typ, encoding="utf-8") as f:
            text = f.read().replace("#horizontalrule", "#line(length: 100%, stroke: 0.5pt)")
        # Typst resolves image paths relative to the .typ file; point them at the repo root, using
        # smaller copies (max 1000 px wide, JPEG) so the PDF stays a few MB instead of ~20 MB.
        magick = shutil.which("magick") or shutil.which("convert")
        rel_tmp = os.path.relpath(tmp, ROOT)

        def image_path(m):
            src = m.group(1)
            if not magick or not os.path.isfile(os.path.join(ROOT, src)):
                return f'image("/{src}"'
            small = os.path.join(rel_tmp, "img", re.sub(r"[^\w.-]", "_", src) + ".jpg")
            os.makedirs(os.path.join(ROOT, rel_tmp, "img"), exist_ok=True)
            subprocess.run([magick, os.path.join(ROOT, src), "-background", "white", "-flatten",
                            "-resize", "1000x>", "-quality", "82", os.path.join(ROOT, small)], check=True)
            return f'image("/{small}"'
        text = re.sub(r'image\("(?!/)([^"]+)"', image_path, text)
        with open(typ, "w", encoding="utf-8") as f:
            f.write(text)
        subprocess.run(["typst", "compile", "--root", ROOT, typ, os.path.abspath(a.out)], check=True)
    print(f"manual: {a.out}")


if __name__ == "__main__":
    main()
