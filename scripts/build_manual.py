#!/usr/bin/env python3
"""Build the PDF manual shipped in each release ZIP.

Body: the README's user sections + Hardware and Wiring. Appendices (each on a new page): LED
Colours, the README's reference sections (memory map, building from source, libraries) and the
Third-Party Notices.

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
        # The README sizes images for a web page (e.g. 400px in two columns), which overflows a PDF
        # page; in the manual every image fills its table cell / the text width instead.
        return f"![{alt}]({src}){{width=100%}}"
    return IMG_TAG.sub(repl, text)


def rebase_links(text, subdir):
    """Markdown image/link targets in docs/<file> are relative to docs/; make them repo-relative."""
    def repl(m):
        target = m.group(2)
        if re.match(r"^(https?:|#|/)", target):
            return m.group(0)
        return f"{m.group(1)}({os.path.normpath(os.path.join(subdir, target))})"
    return re.sub(r"(!?\[[^\]]*\])\(([^)\s]+)\)", repl, text)


def metadata(name):
    """A string constant from src/core/metadata.h (the firmware's single source of identity)."""
    with open(os.path.join(ROOT, "src", "core", "metadata.h"), encoding="utf-8") as f:
        m = re.search(rf'\b{name}\s*=\s*"([^"]*)"', f.read())
    if not m:
        raise SystemExit(f"{name} not found in src/core/metadata.h")
    return m.group(1)


def typst_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def page_setup(name, call, version, email):
    """Header: name left, call sign right. Footer: version left, page x of y centre, email right."""
    small = "set text(size: 8pt, fill: luma(90))"
    rule = "line(length: 100%, stroke: 0.4pt + luma(160))"
    page_x_of_y = "[Page #counter(page).display() of #counter(page).final().first()]"
    # Plain text: PDF/UA treats headers/footers as artifacts, which may not contain links.
    mail = f"[#{typst_str(email)}]"
    # Nothing on the cover page (page 1).
    header = (f"context if counter(page).get().first() > 1 {{ {small}; grid(columns: (1fr, 1fr), align: (left, right), "
              f"[{name}], [{call}]); v(-4pt); {rule} }}")
    footer = (f"context if counter(page).get().first() > 1 {{ {small}; {rule}; v(-4pt); grid(columns: (1fr, 1fr, 1fr), "
              f"align: (left, center, right), [{version}], {page_x_of_y}, {mail}) }}")
    return f"#set page(\n  header: {header},\n  footer: {footer},\n)\n"


def accessibility(name, call, version):
    """Document metadata and link styling for a tagged, accessible PDF (PDF/UA-1).

    Links: underlined (not colour alone), #005bb5 on white (contrast about 6.6:1, WCAG AA/AAA
    for body text), the link blue of ham.bharathpalavalli.com's light theme.
    """
    title = typst_str(f"{name} {version} Field Manual")
    return (f"#set document(title: {title}, author: {typst_str(call)})\n"
            '#set text(lang: "en")\n'
            '#show link: set text(fill: rgb("#005bb5"))\n'
            "#show link: underline.with(offset: 2pt, stroke: 0.6pt)\n"
            # Tables: booktabs (rules above, below and under the header only; no fills or grid).
            "#set table(stroke: none, inset: (x: 6pt, y: 5pt))\n"
            "#set table.hline(stroke: 0.6pt)\n"
            "#show table.cell.where(y: 0): set text(weight: \"bold\")\n"
            "#show table: t => block(stroke: (top: 1.2pt, bottom: 1.2pt), t)\n")


def absolute_links(text, repo):
    """Relative links (README.md, docs/X.md, License) would be dead in a PDF: point them at GitHub.
    In-page anchors (#...) and images (local files embedded in the PDF) are left alone."""
    def repl(m):
        label, target = m.group(1), m.group(2)
        if re.match(r"^(https?:|mailto:|#)", target):
            return m.group(0)
        return f"{label}(https://github.com/{repo}/blob/main/{target.lstrip('./')})"
    return re.sub(r"(?<!!)(\[[^\]]*\])\(([^)\s]+)\)", repl, text)


# README "## " sections moved to the appendices (title in the README -> appendix title), and
# sections left out because an appendix replaces them.
README_APPENDICES = {
    "Partition System & Memory Map": "Flash Memory Map",
    "Production Pipeline & Flashing": "Building from Source",
    "Libraries & Frameworks": "Libraries & Frameworks",
}
README_DROPPED = {"Third-Party Licences"}   # replaced by the Third-Party Notices appendix
PAGEBREAK = "\n\n```{=typst}\n#pagebreak()\n```\n\n"


def split_sections(text):
    """(preamble, [(title, section text incl. its '## ' heading)]) for the '## ' sections."""
    parts = re.split(r"(?m)^(?=## )", text)
    sections = []
    for part in parts[1:]:
        sections.append((part.splitlines()[0][3:].strip(), part))
    return parts[0], sections


def as_appendix(text, letter, title, top_level):
    """Retitle a document/section as 'Appendix X: title' at level 1, shifting its sub-headings."""
    body = text.split("\n", 1)[1] if "\n" in text else ""
    if top_level == 2:   # a README '## ' section: its '### ' become '## '
        body = re.sub(r"(?m)^#(#+) ", r"\1 ", body)
    return f"# Appendix {letter}: {title}\n{body}"


def read_doc(name):
    with open(os.path.join(ROOT, name), encoding="utf-8") as f:
        text = html_images_to_markdown(f.read())
    subdir = os.path.dirname(name)
    return rebase_links(text, subdir) if subdir else text


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", required=True)
    ap.add_argument("--repo", default="bmp/QRPickle")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    # Cover page: title, the project table (centred by pandoc) and the author's logo; the
    # content starts on page 2. The appendix anchor is pandoc's id for "# Appendix E: ...".
    subtitle = "A lightweight opinionated field friendly HAM Clock"
    details = [
        ("Firmware Package", "QRPickle Tracker Dashboard"),
        ("Software Version", a.version),
        ("Compilation Date", datetime.date.today().isoformat()),
        ("Target Hardware", "ESP32 Cheap Yellow Display (CYD)"),
        ("Source Repository", f'#link("https://github.com/{a.repo}")[{a.repo}]'),
        # pandoc's id for the "# Appendix E: Third-Party Notices" heading
        ("Primary License",
         "MIT License (third-party components: #link(<appendix-e-third-party-notices>)[Appendix E])"),
    ]
    detail_cells = ", ".join(f"text(fill: luma(90))[{k}], [{v}]" for k, v in details)
    # Cover (report style): accent bar beside a left-aligned title, a borderless details list and
    # the author's logo bottom right. The content starts on page 2.
    cover = f"""```{{=typst}}
#v(3cm)
#grid(columns: (6pt, 1fr), column-gutter: 14pt,
  rect(fill: rgb("#a70f0f"), width: 6pt, height: 3cm),
  [#text(fill: rgb("#a70f0f"), size: 36pt, weight: "bold")[QRPickle] \\
   #v(4pt) #text(fill: rgb("#a70f0f"), size: 16pt)[{subtitle}] \\
   #v(6pt) #text(size: 11pt, fill: luma(90))[Field Manual]])
#v(2cm)
#grid(columns: (5cm, 1fr), row-gutter: 10pt, {detail_cells})
#v(1fr)
#align(right)[#image("/docs/pics/vu3glj-logo.png", width: 42%, alt: "VU3GLJ logo")]
#pagebreak()
```

"""
    preamble, sections = split_sections(read_doc("README.md"))
    # The README's title and subtitle are on the cover; its first section becomes "Introduction".
    preamble = re.sub(r"(?m)^# .*\n", "", preamble)
    sections = [("Introduction", "## Introduction\n" + t.split("\n", 1)[1]) if title == subtitle else (title, t)
                for title, t in sections]
    # README sections are '## '; in the manual they are chapters ('# '), like Hardware and the
    # appendices (PDF/UA also requires the first heading to be level 1).
    body = preamble + "".join(re.sub(r"(?m)^#(#+) ", r"\1 ", t) for title, t in sections
                              if title not in README_APPENDICES and title not in README_DROPPED)
    body += "\n\n---\n\n" + read_doc("docs/HARDWARE.md")

    appendices = [("LED Colours", read_doc("docs/LEDColours.md"), 1)]
    by_title = dict(sections)
    for readme_title, title in README_APPENDICES.items():
        appendices.append((title, by_title[readme_title], 2))
    appendices.append(("Third-Party Notices", read_doc("THIRD_PARTY_NOTICES.md"), 1))
    appendix_text = "".join(PAGEBREAK + as_appendix(text, chr(ord("A") + i), title, level)
                            for i, (title, text, level) in enumerate(appendices))
    doc = absolute_links(cover + body + appendix_text, a.repo)

    with tempfile.TemporaryDirectory(dir=ROOT) as tmp:   # inside the repo, so image paths resolve
        md, typ = os.path.join(tmp, "manual.md"), os.path.join(tmp, "manual.typ")
        with open(md, "w", encoding="utf-8") as f:
            f.write(doc)
        # Read Markdown the way GitHub does: lists may follow a paragraph directly, and '---' is
        # only a rule (pandoc would otherwise start YAML metadata or a multiline/simple table).
        reader = ("markdown+lists_without_preceding_blankline"
                  "-yaml_metadata_block-multiline_tables-simple_tables")
        subprocess.run(["pandoc", md, "-f", reader, "-t", "typst", "-o", typ,
                        "--resource-path", ROOT], check=True)
        with open(typ, encoding="utf-8") as f:
            text = f.read().replace("#horizontalrule", "#line(length: 100%, stroke: 0.5pt)")
        # Pandoc wraps tables in figures, which never split across pages; let the screenshot and
        # photo tables continue on the next page instead of running off the bottom.
        # Don't repeat a table's first row on the next page (in the screenshot tables it holds
        # captions for the first images only), and keep code blocks (the wiring diagram) together.
        name, call, email = metadata("FW_NAME"), metadata("AUTHOR_CALL"), metadata("SUPPORT_EMAIL")
        text = (accessibility(name, call, a.version) + page_setup(name, call, a.version, email) +
                "#show figure: set block(breakable: true)\n"
                "#show raw.where(block: true): set block(breakable: false)\n" + text)
        text = text.replace("table.header(", "table.header(repeat: false, ")
        # Typst resolves image paths relative to the .typ file; point them at the repo root, using
        # smaller copies (max 1000 px wide, JPEG) so the PDF stays a few MB instead of ~20 MB.
        # Full-page screenshots are trimmed to their top (at most 1.25x as tall as wide): table rows
        # can't break across pages, so a 3000 px tall screenshot would run off the page.
        magick = shutil.which("magick") or shutil.which("convert")
        rel_tmp = os.path.relpath(tmp, ROOT)

        def image_path(m):
            src = m.group(1)
            if not magick or not os.path.isfile(os.path.join(ROOT, src)):
                return f'image("/{src}"'
            small = os.path.join(rel_tmp, "img", re.sub(r"[^\w.-]", "_", src) + ".jpg")
            os.makedirs(os.path.join(ROOT, rel_tmp, "img"), exist_ok=True)
            subprocess.run([magick, os.path.join(ROOT, src), "-background", "white", "-flatten",
                            "-resize", "1000x>", "-gravity", "North", "-crop", "%[fx:w]x%[fx:min(h,w*1.25)]+0+0",
                            "+repage", "-quality", "82", os.path.join(ROOT, small)], check=True)
            return f'image("/{small}"'
        text = re.sub(r'image\("(?!/)([^"]+)"', image_path, text)
        with open(typ, "w", encoding="utf-8") as f:
            f.write(text)
        subprocess.run(["typst", "compile", "--root", ROOT, "--pdf-standard", "ua-1", typ, os.path.abspath(a.out)],
                       check=True)
    print(f"manual: {a.out}")


if __name__ == "__main__":
    main()
