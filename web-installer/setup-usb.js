// Setup over USB (pure functions, no DOM): the settings the page sends, their chunks and the line
// protocol with the board (firmware: src/services/setup_serial_proto.h). Used by pages/setup.html;
// tested by test/js/setup_usb_test.mjs.
const MAX_DATA = 160;          // bytes of JSON per DATA line (the board's USB buffer holds 256)
const READY_WAIT_MS = 40000;   // opening the port may restart the board: boot + up to 20 s WiFi wait
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
    const board = (f.board ?? "").toString().trim();  // v0.3.2: "auto" or a board id (boards.json)
    if (board) out.board = board;
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
    if (cmd === "BOARD") {
        try { return { type: "board", info: JSON.parse(rest) }; } catch (e) { return null; }
    }
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

// HELLO until the board answers READY (opening the port may restart it); rejects when it never does.
async function hello(port, now) {
    const helloEnd = now() + READY_WAIT_MS;
    while (now() < helloEnd) {
        await port.write("QRPSETUP HELLO");
        const r = await nextReply(port, 1000, now);
        if (r?.type === "ready") return r;
    }
    throw new Error("No answer from the board: its firmware is too old for setup over USB (v0.3.1 or newer), " +
                    "or another program has the port open.");
}

// What the board detected (v0.3.2 and newer): { version, info } with info = the BOARD reply's JSON
// (as /api/board), or null when the firmware doesn't know INFO (v0.3.1).
async function readBoard(port, { now = () => Date.now() } = {}) {
    const ready = await hello(port, now);
    await port.write("QRPSETUP INFO");
    const r = await nextReply(port, 3000, now);
    return { version: ready.version, info: r?.type === "board" ? r.info : null };
}

// port: { write(line), readLine(timeoutMs) -> line or null }. Resolves to
// { version, saved, kept, admin, restart }; rejects with a message for the page.
async function runSetup(port, settings, { now = () => Date.now(), sleep = ms => new Promise(r => setTimeout(r, ms)),
                                          log = () => {} } = {}) {
    const ready = await hello(port, now);
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

// A Web Serial port (already open) as the { write, readLine } runSetup() uses. The text decoder is
// piped from port.readable; closeLinePort() must undo that before the port can close.
function openLinePort(port) {
    const enc = new TextEncoder();
    const decoder = new TextDecoderStream();
    const piped = port.readable.pipeTo(decoder.writable).catch(() => {});  // ends when the reader is cancelled
    const reader = decoder.readable.getReader();
    const writer = port.writable.getWriter();
    let buf = "", pending = null;
    return {
        reader, writer, piped,
        write: line => writer.write(enc.encode(line + "\n")),
        async readLine(ms) {
            const end = Date.now() + ms;
            for (;;) {
                const i = buf.indexOf("\n");
                if (i >= 0) {
                    const line = buf.slice(0, i).replace(/\r$/, "");
                    buf = buf.slice(i + 1);
                    return line;
                }
                const left = end - Date.now();
                if (left <= 0) return null;
                pending ??= reader.read();
                let timer;
                const r = await Promise.race([pending, new Promise(res => { timer = setTimeout(() => res(null), left); })]);
                clearTimeout(timer);
                if (!r) return null;  // timed out; the read stays pending for the next call
                pending = null;
                if (r.done) return null;
                buf += r.value;
            }
        },
    };
}

// Closes the port, also after an error or when a line port was never made (an earlier failure
// left it open). Chrome refuses to close a port while its streams are locked.
async function closeLinePort(lp, port) {
    if (lp) {
        try { await lp.reader.cancel(); } catch (e) { /* already closed */ }
        await lp.piped;
        try { lp.reader.releaseLock(); } catch (e) { /* released */ }
        try { lp.writer.releaseLock(); } catch (e) { /* released */ }
    }
    try { await port.close(); } catch (e) { /* not open */ }
}

// "ok", "insecure" (Chrome hides Web Serial on http pages other than localhost) or "unsupported".
function serialSupport(win) {
    if (win.navigator && "serial" in win.navigator) return "ok";
    return win.isSecureContext === false ? "insecure" : "unsupported";
}

const escapeHtml = t => String(t).replace(/[&<>"]/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));

// What the page shows after runSetup() (HTML).
function resultText(r) {
    const head = r.saved.length
        ? `<strong>Saved on the board</strong> (QRPickle ${escapeHtml(r.version)}): ${escapeHtml(r.saved.join(", "))}.`
        : `<strong>Nothing to change: the board already has these values</strong> (QRPickle ${escapeHtml(r.version)}).`;
    const kept = r.kept.length ? `<br><strong>Not changed</strong> (check the value): ${escapeHtml(r.kept.join(", "))}.` : "";
    const login = `<br>Web console login: user <code>admin</code>, password <code>${escapeHtml(r.admin)}</code> ` +
                  "(also on the board's Network screen).";
    const next = r.restart
        ? "<br>The board restarts now to join your WiFi. Its address then shows on the Network screen; open it in your browser."
        : "<br>Open the address on the board's Network screen in your browser for every other setting.";
    return head + kept + login + next;
}

if (typeof module !== "undefined") {
    module.exports = { MAX_DATA, buildSettings, chunks, parseReply, runSetup, readBoard, openLinePort, closeLinePort,
                       resultText, escapeHtml, serialSupport };
}
