// Unit test of data/www/board_report.js (board report links for GitHub's issue form).
// Run: node test/js/board_report_test.mjs
import assert from "node:assert/strict";
import { createRequire } from "node:module";
const require = createRequire(import.meta.url);
const { reportUrl, qrUrl, autoFromBoard, BASE, MAX_URL } = require("../../data/www/board_report.js");

// What /api/board returns on the owner's board (plus fields a report must never copy).
const board = {
    id: "2432S028R-2USB", model: "ESP32-2432S028R", name: "ESP32-2432S028R (2.8\", USB-C + micro-USB)",
    status: "tested", decision: "detected", setting: "auto", rddid: "8181B3", rdid4: "FFFFFF", rdid4_indexed: "000000",
    probes: { i2c: 0, xpt_own: 1, xpt_shared: -1 }, fw: "v0.3.2", decision_code: "detected",
};
const status = { ssid: "HomeNet", password: "hunter22", apikey: "abcdef123456", callsign: "VU2XYZ",
                 aprs_pass: "12345", hamalert_pass: "pw", board };

// ---- the QR link equals the firmware's (test/test_boards test_qr_url_exact_and_short) ---------------
const auto = autoFromBoard(board);
assert.equal(qrUrl({ ...auto, decision: "anyway", touch: "0.1.-1" }),
             "https://github.com/bmp/QRPickle/issues/new?template=board_report.yml&fw=v0.3.2" +
             "&board=2432S028R-2USB&ids=8181B3-FFFFFF-000000&decision=anyway&touch=0.1.-1");

// ---- builds the form link: fixed order, issue form field ids ---------------------------------------
const url = reportUrl(auto, { back: "ESP32-2432S028", works: { picture: true, touch: true, sensor: false }, notes: "ok" });
assert.ok(url.startsWith(BASE + "&"), url);
const q = new URL(url).searchParams;
assert.equal(q.get("template"), "board_report.yml");
assert.equal(q.get("board"), "2432S028R-2USB");
assert.equal(q.get("ids"), "8181B3-FFFFFF-000000");
assert.equal(q.get("decision"), "detected");
assert.equal(q.get("back"), "ESP32-2432S028");
assert.equal(q.get("works"), "picture, touch");
assert.equal(q.get("notes"), "ok");
assert.equal(q.get("labels"), "board-report");

// ---- encodes and caps user text ------------------------------------------------------------------
const odd = "a & b # c % d\nline2 😀 é";
const u2 = reportUrl(auto, { back: odd, works: {}, notes: odd });
assert.equal(new URL(u2).searchParams.get("notes"), odd, "round-trips through URLSearchParams");
for (const notes of ["n".repeat(9000), "😀".repeat(2500)]) {  // plain, and 12 encoded bytes per emoji
    const long = reportUrl(auto, { back: "x", works: {}, notes });
    assert.ok(long.length <= MAX_URL, `link ${long.length} <= ${MAX_URL}`);
    const cut = new URL(long).searchParams.get("notes");
    assert.ok(cut.endsWith("…"), "cut notes end with an ellipsis");
    assert.ok(!cut.includes("\uFFFD"), "no broken letters");
    assert.ok(long.length > MAX_URL - 40, "cut close to the limit, not far below it");
}
assert.ok(!new URL(reportUrl(auto, { notes: "n".repeat(5000) })).searchParams.get("notes").endsWith("…"), "fits: not cut");

// ---- the QR link has no user text and stays short ------------------------------------------------
const qr = qrUrl(autoFromBoard(board));
assert.ok(qr.length <= 170, `qr ${qr.length}`);
assert.ok(!qr.includes("back=") && !qr.includes("notes="));

// ---- never includes secrets, whatever the source object holds --------------------------------------
for (const link of [reportUrl(autoFromBoard(status.board), { back: "", works: {}, notes: "" }), qrUrl(autoFromBoard(board))]) {
    for (const secret of [status.ssid, status.password, status.apikey, status.callsign, status.aprs_pass, status.hamalert_pass])
        assert.ok(!link.includes(secret), `secret ${secret} must not appear`);
}
// autoFromBoard copies only the board facts it knows, never other keys
assert.deepEqual(Object.keys(autoFromBoard({ ...board, ssid: "x", password: "y" })).sort(),
                 ["board", "decision", "fw", "ids", "touch"]);

console.log("board_report_test: all passed");
