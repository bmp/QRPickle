#!/usr/bin/env python3
"""Build QRPickle's "space feed": one small JSON with amateur satellites (orbits, transmitters) and
ARISS events (school contacts, SSTV), for the CYD to compute passes itself.

Runs in GitHub Actions every 6 hours (.github/workflows/space-feed.yml) and is committed to the
`space-feed` branch. The CYD downloads it; it never sends its location anywhere.

Sources (credited in the feed and on the Pages site):
  - SatNOGS DB transmitters and satellites (CC BY-SA 4.0): which satellites carry FM voice
    repeaters, linear transponders, SSTV or voice beacons, with frequencies and modes.
  - CelesTrak: orbital elements (TLE), the "amateur" group plus single satellites by catalogue number.
  - ARISS: upcoming school contacts and SSTV events (web pages, parsed).

    python3 tools/space_feed/build_feed.py --out feed.json [--offline DIR]

--offline reads transmitters.json, satellites.json, amateur.tle, contacts.html and sstv.html from
DIR instead of downloading (tests and development).
"""
import argparse
import calendar
import html
import json
import os
import re
import sys
import time
import urllib.request

SATNOGS_TX = "https://db.satnogs.org/api/transmitters/?format=json"
SATNOGS_SATS = "https://db.satnogs.org/api/satellites/?format=json"
CELESTRAK_GROUP = "https://celestrak.org/NORAD/elements/gp.php?GROUP=amateur&FORMAT=tle"
CELESTRAK_ONE = "https://celestrak.org/NORAD/elements/gp.php?CATNR={}&FORMAT=tle"
ARISS_CONTACTS = "https://www.ariss.org/upcoming-educational-contacts.html"
ARISS_SSTV = "https://www.ariss.org/upcoming-sstv-events.html"
USER_AGENT = "QRPickle-space-feed/1 (+https://github.com/bmp/QRPickle)"

ISS = 25544
MAX_SATS = 40
MAX_SINGLE_TLE_FETCHES = 30
# Category letters in the feed: f = FM voice repeater, l = linear transponder, s = SSTV,
# v = voice beacon, a = APRS digipeater. The order is the selection priority after the ISS.
PRIORITY = "flsva"
CREDIT = ("Transmitters: SatNOGS DB (db.satnogs.org), CC BY-SA 4.0. Orbits: CelesTrak (celestrak.org). "
          "Events: ARISS (ariss.org). Built by QRPickle; this feed is CC BY-SA 4.0.")
SKIP_SERVICES = {"Earth Exploration", "Meteorological", "Military", "Government", "Commercial"}
# SatNOGS entries that match a category but aren't amateur satellites (TIROS-10: a 1960 weather
# satellite with a "TV" transmitter listed as SSTV).
DENY = {1430}
MONTHS = {m: i for i, m in enumerate(calendar.month_name) if m}
MONTHS.update({m: i for i, m in enumerate(calendar.month_abbr) if m})
SSTV_MODES = r"(Robot\s*36|Robot\s*72|PD\s*\d{2,3}|Martin\s*M\d|Scottie\s*(?:DX|\d))"


def fetch(url, timeout=60):
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(req, timeout=timeout) as r:  # noqa: S310 (fixed https URLs)
        return r.read().decode("utf-8", errors="replace")


# ---- Satellites -------------------------------------------------------------------------------

def category(tx):
    """The feed category letter of a SatNOGS transmitter, or None."""
    d = (tx.get("description") or "").lower()
    m = (tx.get("mode") or "").upper()
    ty = tx.get("type")
    if "SSTV" in m or "sstv" in d:
        return "s"
    if ty == "Transponder":
        return "l"
    if ty == "Transceiver" and m in ("FM", "FMN") and tx.get("uplink_low"):
        return "f"
    if ("digitalker" in d or "voice" in d) and m in ("FM", "FMN", "AM", "USB"):
        return "v"
    if ty == "Transceiver" and ("APRS" in m or "AFSK" in m or "aprs" in d or "digi" in d):
        return "a"
    return None


def display_name(sat):
    """The name operators use: an OSCAR designation (SO-50, AO-7, FO-29, TO-108) when SatNOGS lists
    one, else a short designator (RS-44), else SatNOGS' name."""
    names = [sat.get("name", "")] + [n.strip() for n in (sat.get("names") or "").split(",")]
    for pattern in (r"[A-Z]{1,2}O-\d{1,3}", r"[A-Z]{1,3}-\d{1,3}"):
        for alt in names:
            if re.fullmatch(pattern, alt):
                return alt
    return sat.get("name", "")


def usable(tx):
    return (tx.get("status") == "active" and tx.get("alive") and tx.get("norad_cat_id")
            and str(tx.get("service")) not in SKIP_SERVICES)


def parse_tle(text):
    """{norad: (name, line1, line2)} from three-line TLE text."""
    lines = [ln.rstrip() for ln in text.splitlines() if ln.strip()]
    out = {}
    for i in range(len(lines) - 2):
        if lines[i + 1].startswith("1 ") and lines[i + 2].startswith("2 "):
            try:
                out[int(lines[i + 1][2:7])] = (lines[i].strip(), lines[i + 1], lines[i + 2])
            except ValueError:
                pass
    return out


def select_satellites(transmitters, satellites, categories="flsv"):
    """[(norad, name, category letters, [tx...])], the ISS first, then the categories in turn."""
    in_orbit = {s["norad_cat_id"]: s for s in satellites
                if s.get("norad_cat_id") and s.get("status") == "in orbit"}
    per = {}
    for tx in transmitters:
        if not usable(tx):
            continue
        c = category(tx)
        nid = tx["norad_cat_id"]
        if c is None or c not in categories or nid not in in_orbit or nid in DENY:
            continue
        cats, txs = per.setdefault(nid, (set(), []))
        cats.add(c)
        txs.append({k: v for k, v in {
            "d": tx.get("downlink_low"), "u": tx.get("uplink_low"), "m": tx.get("mode"),
            "x": (tx.get("description") or "")[:40], "c": c,
        }.items() if v})

    def rank(item):
        nid, (cats, txs) = item
        return (min(PRIORITY.index(c) for c in cats), -len(txs), in_orbit[nid]["name"])

    # The ISS first, then the categories take turns (FM, linear, SSTV, voice, APRS), so a category
    # with many satellites can't crowd the others out. The cap is applied by build(), after
    # satellites without an orbit have been dropped.
    queues = {c: [] for c in PRIORITY}
    for item in sorted(per.items(), key=rank):
        if item[0] != ISS:
            queues[min(item[1][0], key=PRIORITY.index)].append(item)
    ordered = [(ISS, per[ISS])] if ISS in per else []
    while any(queues.values()):
        for c in PRIORITY:
            if queues[c]:
                ordered.append(queues[c].pop(0))
    return [(nid, display_name(in_orbit[nid]), "".join(sorted(cats, key=PRIORITY.index)), txs)
            for nid, (cats, txs) in ordered]


# ---- ARISS events -----------------------------------------------------------------------------

def page_text(raw):
    t = re.sub(r"<script.*?</script>|<style.*?</style>", "", raw, flags=re.S | re.I)
    t = re.sub(r"<br\s*/?>|</p>|</div>|</li>|</h\d>", "\n", t, flags=re.I)
    t = html.unescape(re.sub(r"<[^>]+>", " ", t))
    return [re.sub(r"\s+", " ", ln).strip() for ln in t.splitlines() if ln.strip()]


CONTACT_RE = (r"Contact is go for:\s*\w{3}\s+(\d{4})-(\d{2})-(\d{2})\s+"
              r"(\d{1,2}):(\d{2})(?::(\d{2}))?\s*UTC")
MONTH_RE = r"\b(" + "|".join(sorted(MONTHS, key=len, reverse=True)) + r")\.?\s+(\d{1,2})(?:,?\s+(\d{4}))?"


def parse_contacts(raw):
    """ARISS school contacts: [{"k": "contact", "s", "e", "t", "x"}]; a contact lasts ~10 min."""
    lines = page_text(raw)
    out = []
    for i, ln in enumerate(lines):
        m = re.search(CONTACT_RE, ln)
        if not m or i == 0:
            continue
        y, mo, d, hh, mm, ss = (int(g) if g else 0 for g in m.groups())
        start = calendar.timegm((y, mo, d, hh, mm, ss))
        who = re.match(r"(.*?),?\s+(direct|telebridge)\s+via\s+(\S+)\s*\((.+?)\)\s*$", lines[i - 1])
        if who:
            title = who.group(1).strip(" ,")
            where = "telebridge, not heard locally" if who.group(2) == "telebridge" else "direct"
            detail = f"{where}, via {who.group(3)}; {who.group(4).strip()}"
        else:
            title, detail = lines[i - 1][:60], ""
        out.append({"k": "contact", "s": start, "e": start + 600, "t": title[:60], "x": detail[:80],
                    "sat": ISS, "f": 145800000, "m": "FM"})
    return out


def _date_time(line, default_year, heading_month):
    """(epoch) from a line with a month name, day, optional year and HH:MM UTC; None if absent."""
    mm = re.search(MONTH_RE, line)
    tm = re.search(r"\b(\d{1,2}):(\d{2})\s*UTC", line)
    if not mm or not tm:
        return None
    month, day = MONTHS[mm.group(1)], int(mm.group(2))
    year = int(mm.group(3)) if mm.group(3) else default_year + (1 if month < heading_month - 6 else 0)
    try:
        return calendar.timegm((year, month, day, int(tm.group(1)), int(tm.group(2)), 0))
    except ValueError:
        return None


def parse_sstv(raw):
    """ARISS SSTV events: [{"k": "sstv", "s", "e", "t", "x", "f", "m"}]; events without readable
    start and end times are skipped (the page is free text)."""
    lines = page_text(raw)
    out = []
    heads = [(i, re.match(r"(\d{4})-(\d{2})-\d{2}\s+(.*SSTV.*)$", ln)) for i, ln in enumerate(lines)]
    heads = [(i, m) for i, m in heads if m]
    for n, (i, h) in enumerate(heads):
        block = lines[i + 1:heads[n + 1][0] if n + 1 < len(heads) else min(len(lines), i + 15)]
        year, hmonth = int(h.group(1)), int(h.group(2))
        start = end = None
        for ln in block:
            low = ln.lower()
            if start is None and re.search(r"\b(begin|begins|start)", low):
                start = _date_time(ln, year, hmonth)
            elif end is None and re.search(r"\b(conclude|concludes|end|ends)\b", low):
                end = _date_time(ln, year, hmonth)
        if not start or not end or end <= start:
            continue
        text = " ".join(block)
        f = re.search(r"\b(\d{3}\.\d{1,3})\s*M", text)
        mode = re.search(SSTV_MODES, text, re.I)
        freq = int(round(float(f.group(1)) * 1e6)) if f else 0  # 0: not stated on the page
        mode_s = re.sub(r"\s+", " ", mode.group(1)) if mode else ""
        pattern = re.search(r"Transmission[^.]*?(\d+\s*seconds? on[^.]*?off)", text, re.I)
        if freq and mode_s:
            detail = f"{freq / 1e6:.3f} MHz FM, {mode_s}"
        else:
            detail = "frequency and mode: see ariss.org"
        detail += f"; {pattern.group(1)}" if pattern else ""
        out.append({"k": "sstv", "s": start, "e": end, "t": h.group(3)[:60], "x": detail[:80],
                    "sat": ISS, "f": freq, "m": mode_s})
    return out


# ---- Feed -------------------------------------------------------------------------------------

def build(sources, now=None, single_tle=None):
    """The feed dict. sources: transmitters (list), satellites (list), tle (text), contacts (html),
    sstv (html). single_tle(norad) returns TLE text for satellites missing from the group."""
    now = int(now if now is not None else time.time())
    tles = parse_tle(sources["tle"])
    sats = []
    fetched = 0
    for nid, name, cats, txs in select_satellites(sources["transmitters"], sources["satellites"]):
        if nid not in tles and single_tle and fetched < MAX_SINGLE_TLE_FETCHES:
            fetched += 1
            try:
                tles.update(parse_tle(single_tle(nid)))
            except OSError as e:
                print(f"TLE for {nid} not fetched: {e}", file=sys.stderr)
        if nid not in tles:
            continue  # no orbit, no passes
        _, l1, l2 = tles[nid]
        sats.append({"id": nid, "n": name[:24], "c": cats, "t1": l1, "t2": l2, "tx": txs[:6]})
        if len(sats) >= MAX_SATS:
            break
    events = [e for e in parse_contacts(sources["contacts"]) + parse_sstv(sources["sstv"])
              if e["e"] >= now - 86400]
    events.sort(key=lambda e: e["s"])
    return {"v": 1, "generated": now, "credit": CREDIT, "sats": sats, "events": events}


def load_offline(d):
    def rd(name):
        with open(os.path.join(d, name), encoding="utf-8") as f:
            return f.read()
    return {"transmitters": json.loads(rd("transmitters.json")), "satellites": json.loads(rd("satellites.json")),
            "tle": rd("amateur.tle"), "contacts": rd("contacts.html"), "sstv": rd("sstv.html")}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--offline", help="directory with the five source files (no downloads)")
    a = ap.parse_args()
    if a.offline:
        sources, single = load_offline(a.offline), None
    else:
        sources = {"transmitters": json.loads(fetch(SATNOGS_TX)), "satellites": json.loads(fetch(SATNOGS_SATS)),
                   "tle": fetch(CELESTRAK_GROUP), "contacts": fetch(ARISS_CONTACTS), "sstv": fetch(ARISS_SSTV)}

        def single(nid):
            time.sleep(1)  # gentle on CelesTrak
            return fetch(CELESTRAK_ONE.format(nid))
    feed = build(sources, single_tle=single)
    if not feed["sats"]:
        sys.exit("no satellites in the feed; not writing it")
    with open(a.out, "w", encoding="utf-8") as f:
        json.dump(feed, f, separators=(",", ":"))
    print(f"{a.out}: {len(feed['sats'])} satellites, {len(feed['events'])} events, {os.path.getsize(a.out)} bytes")


if __name__ == "__main__":
    main()
