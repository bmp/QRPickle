// Board reports: links that open GitHub's "Board report" issue form (.github/ISSUE_TEMPLATE/
// board_report.yml) with the details filled in. Used by the web console and the website's Set up over
// USB page; the CYD's QR code builds the same short link in C++ (hw::rules::report_qr_url), and
// test/js/board_report_test.mjs checks both agree. Only board facts and what the user typed go in:
// never settings, WiFi details, keys or the callsign.
const BASE = "https://github.com/bmp/QRPickle/issues/new?template=board_report.yml";
const MAX_URL = 8000;  // GitHub refuses longer links
const WORKS = ["picture", "colours", "orientation", "touch", "backlight", "led", "sensor"];

// The automatic part, from /api/board (or Set up over USB's BOARD reply). Copies only these five.
function autoFromBoard(b) {
    b = b || {};
    const p = b.probes || {};
    return {
        fw: String(b.fw || ""),
        board: String(b.id || ""),
        ids: [b.rddid, b.rdid4, b.rdid4_indexed].map(x => String(x || "------")).join("-"),
        decision: String(b.decision_code || b.decision || ""),
        touch: [p.i2c, p.xpt_own, p.xpt_shared].map(x => (x === undefined ? -1 : x)).join("."),
    };
}

const enc = encodeURIComponent;

// The short link (the CYD's QR code): automatic facts only.
function qrUrl(a) {
    return `${BASE}&fw=${enc(a.fw)}&board=${enc(a.board)}&ids=${enc(a.ids)}&decision=${enc(a.decision)}` +
           `&touch=${enc(a.touch)}`;
}

// The full link: the automatic facts plus what the user filled in; notes are cut to fit MAX_URL.
function reportUrl(a, user) {
    user = user || {};
    const works = WORKS.filter(k => user.works && user.works[k]).join(", ");
    const head = `${qrUrl(a)}&labels=board-report&back=${enc(String(user.back || ""))}&works=${enc(works)}&notes=`;
    let notes = String(user.notes || "");
    let url = head + enc(notes);
    if (url.length > MAX_URL) {
        let lo = 0, hi = notes.length;  // longest prefix that fits with the ellipsis
        while (lo < hi) {
            const mid = Math.ceil((lo + hi) / 2);
            if ((head + enc(Array.from(notes).slice(0, mid).join("") + "…")).length <= MAX_URL) lo = mid;
            else hi = mid - 1;
        }
        url = head + enc(Array.from(notes).slice(0, lo).join("") + "…");
    }
    return url;
}

if (typeof module !== "undefined") {
    module.exports = { BASE, MAX_URL, WORKS, autoFromBoard, qrUrl, reportUrl };
}
