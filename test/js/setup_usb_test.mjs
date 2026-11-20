// Unit test of web-installer/setup-usb.js (Setup over USB: settings, chunks, protocol).
// Run: node test/js/setup_usb_test.mjs
import assert from "node:assert/strict";
import { createRequire } from "node:module";
const require = createRequire(import.meta.url);
const { buildSettings, chunks, parseReply, runSetup, MAX_DATA } = require("../../web-installer/setup-usb.js");

const bytes = s => new TextEncoder().encode(s).length;

// ---- buildSettings: only filled fields; units and clock always ---------------------------------
let s = buildSettings({ callsign: " vu2abc ", grid: "", ssid: "Home", password: "", lat: "12.9", lon: "",
                        offset: "5.5", clock_12h: true, metric: true, unit_temp: "1" });
assert.deepEqual(s, { callsign: "vu2abc", ssid: "Home", offset: 5.5, clock_12h: true,
                      unit_temp: 0, unit_pres: 0, unit_wind: 0, unit_dist: 0 });
s = buildSettings({ lat: "12.9", lon: "77.6", metric: false, unit_temp: "1", unit_pres: "1", unit_wind: "2",
                    unit_dist: "1", clock_12h: false, apikey: "k" });
assert.deepEqual(s, { lat: 12.9, lon: 77.6, latlon_set: true, apikey: "k", clock_12h: false,
                      unit_temp: 1, unit_pres: 1, unit_wind: 2, unit_dist: 1 });

// ---- chunks: at most 160 bytes, whole UTF-8 letters, rejoin exactly ---------------------------
const ssid = "x".repeat(150) + "é€" + "y".repeat(400);  // "é" sits across the first 160-byte boundary
const json = JSON.stringify({ ssid });
const parts = chunks(json);
assert.ok(parts.every(p => bytes(p) <= MAX_DATA), "every chunk fits");
assert.equal(parts.join(""), json, "chunks rejoin to the JSON");
assert.ok(parts.length >= 4);
assert.deepEqual(chunks("{}"), ["{}"]);

// ---- parseReply ---------------------------------------------------------------------------------
assert.deepEqual(parseReply("QRPSETUP READY v0.3.1"), { type: "ready", version: "v0.3.1" });
assert.deepEqual(parseReply("QRPSETUP ACK BEGIN"), { type: "ack", seq: "BEGIN" });
assert.deepEqual(parseReply("QRPSETUP ACK 3"), { type: "ack", seq: "3" });
assert.deepEqual(parseReply("QRPSETUP ERR busy"), { type: "err", code: "busy" });
assert.deepEqual(parseReply("QRPSETUP SAVED callsign,ssid KEPT grid ADMIN abcd2345"),
                 { type: "saved", saved: ["callsign", "ssid"], kept: ["grid"], admin: "abcd2345" });
assert.deepEqual(parseReply("QRPSETUP SAVED - KEPT - ADMIN abcd2345"),
                 { type: "saved", saved: [], kept: [], admin: "abcd2345" });
assert.deepEqual(parseReply("QRPSETUP RESTART"), { type: "restart" });
assert.equal(parseReply("[Wi-Fi] Disconnected, reason 8"), null);

// ---- runSetup against a fake board ------------------------------------------------------------
// The fake answers like the firmware; time is simulated (readLine with nothing to read advances
// the clock by its timeout instead of waiting).
function fakeBoard({ readyAfterMs = 0, busyTimes = 0, silent = false, restart = false } = {}) {
    let clock = 0, busy = busyTimes, json = "", want = 0, seq = 0;
    const out = [], sent = [];
    return {
        sent, now: () => clock, advance: ms => { clock += ms; },
        async write(line) {
            sent.push(line);
            if (silent) return;
            const m = line.match(/^QRPSETUP (\w+) ?(\d+)? ?(.*)$/s);
            if (m[1] === "HELLO") { if (clock >= readyAfterMs) out.push("[boot log line]", "QRPSETUP READY v0.3.1"); }
            else if (m[1] === "BEGIN") { want = +m[2]; json = ""; seq = 0; out.push("QRPSETUP ACK BEGIN"); }
            else if (m[1] === "DATA") {
                if (+m[2] !== seq) { out.push("QRPSETUP ERR seq"); return; }
                json += m[3]; out.push(`QRPSETUP ACK ${seq++}`);
            } else if (m[1] === "APPLY") {
                if (bytes(json) !== want) { out.push("QRPSETUP ERR length"); return; }
                if (busy-- > 0) { out.push("QRPSETUP ERR busy"); return; }
                this.applied = JSON.parse(json);
                out.push(`QRPSETUP SAVED ${Object.keys(this.applied).join(",")} KEPT - ADMIN abcd2345`);
                if (restart) out.push("QRPSETUP RESTART");
            }
        },
        async readLine(timeoutMs) {
            if (out.length) return out.shift();
            clock += timeoutMs;
            return null;
        },
    };
}
const opts = b => ({ now: b.now, sleep: async ms => b.advance(ms), log: () => {} });

let b = fakeBoard({ readyAfterMs: 3000, restart: true });
let r = await runSetup(b, { callsign: "VU2ABC", ssid: "x".repeat(200) }, opts(b));
assert.deepEqual(b.applied, { callsign: "VU2ABC", ssid: "x".repeat(200) });
assert.deepEqual(r, { version: "v0.3.1", saved: ["callsign", "ssid"], kept: [], admin: "abcd2345", restart: true });
const order = b.sent.filter(l => !l.endsWith("HELLO")).map(l => l.split(" ").slice(1, 3).join(" "));
assert.deepEqual(order.slice(0, 2), ["BEGIN " + bytes(JSON.stringify(b.applied)), "DATA 0"]);
assert.equal(order.at(-1), "APPLY");

b = fakeBoard({ busyTimes: 2 });  // a download running: the whole transfer is sent again
r = await runSetup(b, { unit_temp: 1 }, opts(b));
assert.deepEqual(r.saved, ["unit_temp"]);
assert.equal(b.sent.filter(l => l === "QRPSETUP APPLY").length, 3);
assert.equal(r.restart, false);

b = fakeBoard({ silent: true });  // old firmware: no answer
await assert.rejects(runSetup(b, { unit_temp: 1 }, opts(b)), /too old|no answer/i);

b = fakeBoard({ busyTimes: 100 });  // busy for more than 30 s
await assert.rejects(runSetup(b, { unit_temp: 1 }, opts(b)), /busy/i);

console.log("setup_usb_test: all passed");
