// Unit test of web-installer/setup-usb.js (Setup over USB: settings, chunks, protocol).
// Run: node test/js/setup_usb_test.mjs
import assert from "node:assert/strict";
import { createRequire } from "node:module";
const require = createRequire(import.meta.url);
const { buildSettings, chunks, parseReply, runSetup, MAX_DATA, openLinePort, closeLinePort, resultText } =
    require("../../web-installer/setup-usb.js");
const { serialSupport, readBoard } = require("../../web-installer/setup-usb.js");

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
function fakeBoard({ readyAfterMs = 0, busyTimes = 0, silent = false, restart = false, info = undefined } = {}) {
    let clock = 0, busy = busyTimes, json = "", want = 0, seq = 0;
    const out = [], sent = [];
    return {
        sent, now: () => clock, advance: ms => { clock += ms; },
        async write(line) {
            sent.push(line);
            if (silent) return;
            const m = line.match(/^QRPSETUP (\w+) ?(\d+)? ?(.*)$/s);
            if (m[1] === "HELLO") { if (clock >= readyAfterMs) out.push("[boot log line]", `QRPSETUP READY ${info ? "v0.3.2" : "v0.3.1"}`); }
            else if (m[1] === "INFO") out.push(info ? `QRPSETUP BOARD ${JSON.stringify(info)}` : "QRPSETUP ERR command");
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

// ---- v0.3.2: the board's details (INFO) and the board choice --------------------------------------
const INFO = { id: "2432S028R-2USB", name: "ESP32-2432S028R (2.8\", USB-C + micro-USB)", status: "tested",
               decision_code: "detected", rddid: "8181B3", rdid4: "FFFFFF", rdid4_indexed: "FFFFFF", fw: "v0.3.2" };
assert.deepEqual(parseReply(`QRPSETUP BOARD ${JSON.stringify(INFO)}`), { type: "board", info: INFO });
assert.equal(parseReply("QRPSETUP BOARD {not json"), null);
b = fakeBoard({ info: INFO, readyAfterMs: 2000 });
assert.deepEqual(await readBoard(b, opts(b)), { version: "v0.3.2", info: INFO });
assert.ok(b.sent.includes("QRPSETUP INFO"));
b = fakeBoard();  // v0.3.1: doesn't know INFO
assert.deepEqual(await readBoard(b, opts(b)), { version: "v0.3.1", info: null });
b = fakeBoard({ silent: true });
await assert.rejects(readBoard(b, opts(b)), /too old|no answer/i);
// The choice goes with the other settings; "auto" and "" are left out unless chosen explicitly.
assert.equal(buildSettings({ board: "3248S035R" }).board, "3248S035R");
assert.equal(buildSettings({ board: "auto" }).board, "auto");
assert.equal("board" in buildSettings({ board: "" }), false);
b = fakeBoard({ info: INFO });
r = await runSetup(b, buildSettings({ callsign: "VU2ABC", board: "3248S035R" }), opts(b));
assert.equal(b.applied.board, "3248S035R");

// ---- the Web Serial port: lines in, lines out, and a clean close ------------------------------
// Like Chrome's SerialPort: open() twice throws, close() throws while a stream is locked.
class FakeSerialPort {
    constructor() { this.isOpen = false; this.written = ""; }
    async open() {
        if (this.isOpen) throw new Error("Failed to execute 'open' on 'SerialPort': The port is already open.");
        this.isOpen = true;
        this.readable = new ReadableStream({ start: c => { this.ctrl = c; } });
        this.writable = new WritableStream({ write: ch => { this.written += new TextDecoder().decode(ch); } });
    }
    async close() {
        if (this.readable.locked || this.writable.locked) throw new TypeError("Cannot cancel a locked stream");
        this.isOpen = false;
        this.readable = this.writable = null;
    }
    send(text) { this.ctrl.enqueue(new TextEncoder().encode(text)); }
}

const port = new FakeSerialPort();
await port.open();
let lp = openLinePort(port);
port.send("boot log\r\nQRPSETUP READY v0.3.1\nQRPSETUP AC");
assert.equal(await lp.readLine(500), "boot log");
assert.equal(await lp.readLine(500), "QRPSETUP READY v0.3.1");
assert.equal(await lp.readLine(50), null);  // half a line: wait for the rest
port.send("K 0\n");
assert.equal(await lp.readLine(500), "QRPSETUP ACK 0");
await lp.write("QRPSETUP HELLO");
assert.equal(port.written, "QRPSETUP HELLO\n");
assert.equal(await lp.readLine(30), null);  // a read stays pending when the close comes
await closeLinePort(lp, port);
assert.equal(port.isOpen, false, "the port is closed after a run");
await port.open();  // a second run on the same port works
lp = openLinePort(port);
await closeLinePort(lp, port);
assert.equal(port.isOpen, false);
await port.open();  // left open by an earlier failure: closeLinePort without a line port closes it
await closeLinePort(null, port);
assert.equal(port.isOpen, false);

// ---- what the page says ---------------------------------------------------------------------------
assert.match(resultText({ version: "v0.3.1", saved: [], kept: [], admin: "x", restart: false }),
             /Nothing to change: the board already has these values/);
assert.match(resultText({ version: "v0.3.1", saved: ["grid"], kept: ["offset"], admin: "x", restart: false }),
             /Saved on the board.*grid.*Not changed.*offset/s);

// ---- can this page use Web Serial? -------------------------------------------------------------
// Chrome hides navigator.serial on pages that aren't secure (http://0.0.0.0, a LAN address):
// say that, not "use Chrome".
assert.equal(serialSupport({ isSecureContext: true, navigator: { serial: {} } }), "ok");
assert.equal(serialSupport({ isSecureContext: false, navigator: {} }), "insecure");
assert.equal(serialSupport({ isSecureContext: true, navigator: {} }), "unsupported");

console.log("setup_usb_test: all passed");
