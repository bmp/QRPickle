// Setup over USB (pure functions, no DOM): the settings the page sends, their chunks and the line
// protocol with the board (firmware: src/services/setup_serial_proto.h). Used by pages/setup.html;
// tested by test/js/setup_usb_test.mjs.
const MAX_DATA = 160;          // bytes of JSON per DATA line (the board's USB buffer holds 256)
const READY_WAIT_MS = 15000;   // the board may restart when the port opens
const BUSY_RETRY_MS = 2000, BUSY_GIVE_UP_MS = 30000;

const utf8Length = s => new TextEncoder().encode(s).length;

// Only filled fields (empty ones keep what the board has); units and the clock always.
function buildSettings(f) {
    const out = {};
    for (const k of ["callsign", "grid", "ssid", "password", "apikey", "hamalert_pass", "aprs_pass"]) {
        const v = (f[k] ?? "").toString().trim();
        if (v) out[k] = v;
    }
    const lat = parseFloat(f.lat), lon = parseFloat(f.lon);
    if (Number.isFinite(lat) && Number.isFinite(lon)) Object.assign(out, { lat, lon, latlon_set: true });
    const offset = parseFloat(f.offset);
    if (Number.isFinite(offset)) out.offset = offset;
    out.clock_12h = !!f.clock_12h;
    for (const k of ["unit_temp", "unit_pres", "unit_wind", "unit_dist"]) out[k] = f.metric ? 0 : (parseInt(f[k]) || 0);
    return out;
}

// Splits text into pieces of at most max UTF-8 bytes without cutting a letter.
function chunks(text, max = MAX_DATA) {
    const out = [];
    let cur = "", len = 0;
    for (const ch of text) {
        const n = utf8Length(ch);
        if (len + n > max) { out.push(cur); cur = ""; len = 0; }
        cur += ch;
        len += n;
    }
    if (cur || !out.length) out.push(cur);
    return out;
}

function parseReply(line) {
    const m = /^QRPSETUP (\S+) ?(.*)$/.exec(line || "");
    if (!m) return null;
    const [, cmd, rest] = m;
    if (cmd === "READY") return { type: "ready", version: rest };
    if (cmd === "ACK") return { type: "ack", seq: rest };
    if (cmd === "ERR") return { type: "err", code: rest };
    if (cmd === "RESTART") return { type: "restart" };
    if (cmd === "SAVED") {
        const s = /^(\S+) KEPT (\S+) ADMIN (\S+)$/.exec(rest);
        if (!s) return null;
        const list = x => (x === "-" ? [] : x.split(","));
        return { type: "saved", saved: list(s[1]), kept: list(s[2]), admin: s[3] };
    }
    return null;
}

// Reads lines until a protocol reply (other lines are the board's log) or the deadline.
async function nextReply(port, ms, now) {
    const end = now() + ms;
    while (now() < end) {
        const r = parseReply(await port.readLine(Math.min(250, end - now())));
        if (r) return r;
    }
    return null;
}

// port: { write(line), readLine(timeoutMs) -> line or null }. Resolves to
// { version, saved, kept, admin, restart }; rejects with a message for the page.
async function runSetup(port, settings, { now = () => Date.now(), sleep = ms => new Promise(r => setTimeout(r, ms)),
                                          log = () => {} } = {}) {
    let ready = null;
    const helloEnd = now() + READY_WAIT_MS;
    while (!ready && now() < helloEnd) {
        await port.write("QRPSETUP HELLO");
        const r = await nextReply(port, 1000, now);
        if (r?.type === "ready") ready = r;
    }
    if (!ready) throw new Error("No answer from the board: its firmware is too old for setup over USB (v0.3.1 or newer), " +
                                "or another program has the port open.");
    log(`Board answered: QRPickle ${ready.version}`);

    const json = JSON.stringify(settings);
    const parts = chunks(json);
    const busyEnd = now() + BUSY_GIVE_UP_MS;
    for (;;) {
        await port.write(`QRPSETUP BEGIN ${utf8Length(json)}`);
        let r = await nextReply(port, 3000, now);
        if (r?.type !== "ack" || r.seq !== "BEGIN") throw new Error(`The board refused the start (${r?.code || "no answer"}).`);
        for (let i = 0; i < parts.length; i++) {
            await port.write(`QRPSETUP DATA ${i} ${parts[i]}`);
            r = await nextReply(port, 3000, now);
            if (r?.type !== "ack" || r.seq !== String(i)) throw new Error(`Sending failed at part ${i + 1} (${r?.code || "no answer"}).`);
        }
        await port.write("QRPSETUP APPLY");
        r = await nextReply(port, 5000, now);
        if (r?.type === "err" && r.code === "busy") {
            if (now() >= busyEnd) throw new Error("The board stayed busy with a download; try again in a minute.");
            log("The board is busy with a download; trying again...");
            await sleep(BUSY_RETRY_MS);
            if (now() < busyEnd) continue;
            throw new Error("The board stayed busy with a download; try again in a minute.");
        }
        if (r?.type !== "saved") throw new Error(`The board did not save the settings (${r?.code || "no answer"}).`);
        const restart = (await nextReply(port, 1000, now))?.type === "restart";
        return { version: ready.version, saved: r.saved, kept: r.kept, admin: r.admin, restart };
    }
}

if (typeof module !== "undefined") module.exports = { MAX_DATA, buildSettings, chunks, parseReply, runSetup };
