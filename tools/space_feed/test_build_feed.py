"""Tests for build_feed.py with small hand-made samples in the formats of the sources (not copies
of them). Run: python3 -m unittest discover -s tools/space_feed"""
import calendar
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_feed as b  # noqa: E402

ISS_TLE = ("ISS (ZARYA)\n1 25544U 98067A   26282.82140208  .00006479  00000+0  12652-3 0  9999\n"
           "2 25544  51.6314  92.7969 0006774 242.6225 117.4075 15.48793003589525\n")
SO50_TLE = ("SAUDISAT 1C (SO-50)\n1 27607U 02058C   26282.50000000  .00000100  00000+0  30000-4 0  9991\n"
            "2 27607  64.5500 100.0000 0050000  50.0000 310.0000 14.80000000123456\n")


def tx(nid, typ, mode, desc, up=None, status="active", alive=True, service="Amateur"):
    return {"norad_cat_id": nid, "type": typ, "mode": mode, "description": desc, "downlink_low": 145800000,
            "uplink_low": up, "status": status, "alive": alive, "service": service}


SATS = [{"norad_cat_id": 25544, "name": "ISS", "names": "ZARYA", "status": "in orbit"},
        {"norad_cat_id": 27607, "name": "SAUDISAT 1C", "names": "SO-50, SaudiSat-1C", "status": "in orbit"},
        {"norad_cat_id": 24278, "name": "JAS-2", "names": "FO-29", "status": "in orbit"},
        {"norad_cat_id": 99999, "name": "GONE", "names": "", "status": "re-entered"}]


class Categories(unittest.TestCase):
    def test_category(self):
        self.assertEqual(b.category(tx(1, "Transmitter", "SSTV", "Mode V SSTV")), "s")
        self.assertEqual(b.category(tx(1, "Transmitter", "FM", "Robot 36 SSTV images")), "s")
        self.assertEqual(b.category(tx(1, "Transponder", "USB", "Mode V/U linear")), "l")
        self.assertEqual(b.category(tx(1, "Transceiver", "FM", "FM repeater", up=145850000)), "f")
        self.assertIsNone(b.category(tx(1, "Transceiver", "FM", "FM, no uplink")))
        self.assertEqual(b.category(tx(1, "Transmitter", "FM", "Digitalker voice beacon")), "v")
        self.assertIsNone(b.category(tx(1, "Transmitter", "GMSK", "Telemetry 9k6")))

    def test_names(self):
        self.assertEqual(b.display_name(SATS[1]), "SO-50")
        self.assertEqual(b.display_name(SATS[2]), "FO-29")  # OSCAR name beats "JAS-2"
        self.assertEqual(b.display_name({"name": "DOSAAF-85", "names": "RS-44"}), "RS-44")  # the name operators use
        self.assertEqual(b.display_name({"name": "ISS", "names": "ZARYA"}), "ISS")

    def test_selection(self):
        txs = [tx(27607, "Transceiver", "FM", "FM repeater", up=145850000, service="Unknown"),
               tx(25544, "Transceiver", "FM", "Mode V/V FM (crew R2+3)", up=144490000),
               tx(25544, "Transmitter", "SSTV", "Mode V Imaging"),
               tx(24278, "Transponder", "USB", "linear", up=145900000, status="inactive"),
               tx(99999, "Transceiver", "FM", "FM repeater", up=1),
               tx(27607, "Transceiver", "FM", "Meteorological", up=1, service="Meteorological")]
        sel = b.select_satellites(txs, SATS)
        self.assertEqual([s[0] for s in sel], [25544, 27607])  # ISS first; inactive, re-entered dropped
        self.assertEqual(sel[0][2], "fs")
        self.assertEqual(len(sel[1][3]), 1)  # the Meteorological transmitter is skipped

    def test_tle(self):
        t = b.parse_tle(ISS_TLE + SO50_TLE + "garbage\n")
        self.assertEqual(sorted(t), [25544, 27607])
        self.assertTrue(t[25544][1].startswith("1 25544U"))


CONTACTS = """<h2>Upcoming Educational Contacts</h2><p>As of October 4, 2026</p>
<p>Wick High School, Wick, United Kingdom, direct via GB4WHS&nbsp; (Joshua Kutryk KJ5BFU)</p>
<p>Contact is go for: Thu 2026-10-08 11:30 UTC 26 deg</p>
<p>EcoTarium, Worcester, MA telebridge via AB1OC (Luke Delaney KJ5PPF)</p>
<p>Contact is go for: Thu 2026-10-15 15:25:33 UTC 40 deg (***)</p>"""

SSTV = """<p>Upcoming SSTV events</p>
<p>2026-09-23 SSTV Expedition 75 Series 33 featuring "Student Education"</p>
<p>Begins Friday October 2, 2026 9:00 UTC.</p><p>Concludes Tuesday October 6, 2026 at 15:55 UTC .</p>
<p>Images will be transmitted at 437.550 MHz (+/- Doppler shift), decode with Robot 36 mode.</p>
<p>Transmission 36 seconds on 2 minutes off</p>
<p>2026-04-09 SSTV Series 31 commemorates several space achievements</p>
<p>The SSTV event should begin shortly after 11:15 UTC on April 10.</p>
<p>The SSTV event should conclude around 13:55 UTC on April 14.</p>
<p>2026-03-24 SSTV Series 31 plan</p><p>Start: April 10 (start time TBD)</p><p>End: April 14 (end time TBD)</p>"""


class Events(unittest.TestCase):
    def test_contacts(self):
        ev = b.parse_contacts(CONTACTS)
        self.assertEqual(len(ev), 2)
        self.assertEqual(ev[0]["s"], calendar.timegm((2026, 10, 8, 11, 30, 0)))
        self.assertEqual(ev[0]["t"], "Wick High School, Wick, United Kingdom")
        self.assertIn("direct, via GB4WHS", ev[0]["x"])
        self.assertEqual(ev[1]["s"], calendar.timegm((2026, 10, 15, 15, 25, 33)))
        self.assertIn("not heard locally", ev[1]["x"])

    def test_sstv(self):
        ev = b.parse_sstv(SSTV)
        self.assertEqual(len(ev), 2)  # the plan without times is skipped
        self.assertEqual(ev[0]["s"], calendar.timegm((2026, 10, 2, 9, 0, 0)))
        self.assertEqual(ev[0]["e"], calendar.timegm((2026, 10, 6, 15, 55, 0)))
        self.assertEqual((ev[0]["f"], ev[0]["m"]), (437550000, "Robot 36"))
        self.assertIn("36 seconds on 2 minutes off", ev[0]["x"])
        self.assertEqual(ev[1]["s"], calendar.timegm((2026, 4, 10, 11, 15, 0)))  # year from the heading
        self.assertEqual(ev[1]["f"], 0)
        self.assertIn("see ariss.org", ev[1]["x"])

    def test_build_filters_old_events_and_needs_tles(self):
        txs = [tx(25544, "Transmitter", "SSTV", "Mode V Imaging"),
               tx(27607, "Transceiver", "FM", "FM repeater", up=145850000)]
        src = {"transmitters": txs, "satellites": SATS, "tle": ISS_TLE, "contacts": CONTACTS, "sstv": SSTV}
        now = calendar.timegm((2026, 10, 12, 0, 0, 0))
        feed = b.build(src, now=now, single_tle=lambda nid: SO50_TLE if nid == 27607 else "")
        self.assertEqual([s["n"] for s in feed["sats"]], ["ISS", "SO-50"])  # SO-50's TLE fetched singly
        self.assertEqual([e["t"][:9] for e in feed["events"]], ["EcoTarium"])  # past events dropped
        self.assertIn("CC BY-SA 4.0", feed["credit"])
        feed2 = b.build(src, now=now)  # no single fetch: SO-50 has no orbit and is left out
        self.assertEqual([s["n"] for s in feed2["sats"]], ["ISS"])


if __name__ == "__main__":
    unittest.main()
