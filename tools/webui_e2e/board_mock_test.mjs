// Web console Board section (v0.3.2) against a stand-in for the CYD's API: no device needed.
// Serves data/www as the CYD does and answers /api/board, /api/status and friends from a small state
// machine, then drives the page in headless Chromium (DevTools protocol, like device_test.mjs).
//
//   chromium --headless=new --remote-debugging-port=9223 --user-data-dir="$(mktemp -d)" about:blank &
//   node tools/webui_e2e/board_mock_test.mjs 9223 [screenshot-dir]
//   kill %1
// Expect "RESULT ... fail=0".
import http from "node:http";
import { readFile, writeFile, mkdir } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../../data/www");
const DEVTOOLS = `http://127.0.0.1:${process.argv[2] || 9223}`;
const sleep = ms => new Promise(r => setTimeout(r, ms));
let pass = 0, fail = 0;
function check(ok, what) {
    console.log(`${ok ? "PASS" : "FAIL"} ${what}`);
    ok ? pass++ : fail++;
}

// ---- the stand-in CYD -------------------------------------------------------------------------
const OWN = { id: "2432S028R-2USB", model: "ESP32-2432S028R", name: "ESP32-2432S028R (2.8\", USB-C + micro-USB)", status: "tested" };
const BIG = { id: "3248S035R", model: "ESP32-3248S035R", name: "ESP32-3248S035R (3.5\")", status: "mapped" };
let state;
function reset(over = {}) {
    state = { ...OWN, decision: "detected", decision_code: "detected", setting: "auto", setting_unknown: false,
              warn_mismatch: false, readable: true, rddid: "8181B3", rdid4: "FFFFFF", rdid4_indexed: "FFFFFF",
              probes: { i2c: -1, xpt_own: -1, xpt_shared: -1 }, candidates: [OWN.id], backlight_on: true,
              touch_on: true, sensors_on: true, touch_note: "", fw: "v0.3.2", ...over };
}
reset();
const posts = [];
// Secrets the real /api/status never sends; here they're bait: no report link may contain them.
const SECRETS = { ssid: "HomeNet-Secret", password: "hunter2hunter2", apikey: "apikey123456", callsign: "VU2XYZ" };

const TYPES = { ".html": "text/html", ".js": "application/javascript", ".css": "text/css", ".json": "application/json", ".svg": "image/svg+xml" };
const server = http.createServer(async (req, res) => {
    const url = new URL(req.url, "http://x");
    const json = (code, obj) => { res.writeHead(code, { "Content-Type": "application/json" }); res.end(JSON.stringify(obj)); };
    if (url.pathname === "/api/board" && req.method === "GET") return json(200, state);
    if (url.pathname === "/api/board" && req.method === "POST") {
        let body = "";
        for await (const c of req) body += c;
        const v = JSON.parse(body).board;
        posts.push(v);
        const known = v === "auto" || [OWN.id, BIG.id].includes(v.replace(/!$/, ""));
        if (!known) return json(400, { status: "unknown_board" });
        const restart = v !== state.setting;
        if (v === "auto") reset();
        else if (v.startsWith(BIG.id)) {
            const anyway = v.endsWith("!");
            reset({ ...BIG, setting: v, decision: anyway ? "chosen anyway" : "mismatch", decision_code: anyway ? "anyway" : "mismatch",
                    warn_mismatch: !anyway });
        }
        return json(200, restart ? { status: "success", restart: true } : { status: "success" });
    }
    if (url.pathname === "/api/status") return json(200, { uptime: 10, heap: 80000, ...SECRETS, board: { id: state.id } });
    if (url.pathname === "/api/config") return json(200, { callsign: "VU2XYZ", grid: "MK82", theme_id: 0, brightness: 180 });
    if (url.pathname.startsWith("/api/")) return json(200, url.pathname.includes("profiles") || url.pathname.includes("messages") ? [] : {});
    const file = url.pathname === "/" ? "index.html" : url.pathname.slice(1);
    try {
        const data = await readFile(path.join(ROOT, path.normalize(file)));
        res.writeHead(200, { "Content-Type": TYPES[path.extname(file)] || "application/octet-stream" });
        res.end(data);
    } catch {
        res.writeHead(404);
        res.end();
    }
});
await new Promise(r => server.listen(0, "127.0.0.1", r));
const BASE = `http://127.0.0.1:${server.address().port}`;

// ---- Chromium over the DevTools protocol --------------------------------------------------------
const target = await (await fetch(`${DEVTOOLS}/json/new?about:blank`, { method: "PUT" })).json();
const ws = new WebSocket(target.webSocketDebuggerUrl);
await new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });
let nextId = 1;
const pending = new Map();
ws.onmessage = ev => {
    const m = JSON.parse(ev.data);
    if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); }
    if (m.method === "Page.javascriptDialogOpening") send("Page.handleJavaScriptDialog", { accept: true });
};
function send(method, params = {}) {
    const id = nextId++;
    return new Promise(res => { pending.set(id, res); ws.send(JSON.stringify({ id, method, params })); });
}
async function js(expression) {
    const r = await send("Runtime.evaluate", { expression, awaitPromise: true, returnByValue: true });
    if (r.result?.exceptionDetails) throw new Error(r.result.exceptionDetails.exception?.description || expression);
    return r.result?.result?.value;
}
const SHOTS = process.argv[3];
async function shot(name) {  // the Board section, for review (optional)
    if (!SHOTS) return;
    await mkdir(SHOTS, { recursive: true });
    await js(`document.querySelector('.sys-section-title + #board-note, #board-note').scrollIntoView()`);
    const clip = await js(`(() => { const a = document.getElementById("board-note").previousElementSibling.getBoundingClientRect();
        const c = document.getElementById("board-report"); const z = (c.classList.contains("hidden") ? document.querySelector(".board-setting") : c).getBoundingClientRect();
        return { x: 0, y: a.top + scrollY - 10, width: document.documentElement.clientWidth, height: z.bottom - a.top + 20, scale: 1 }; })()`);
    const r = await send("Page.captureScreenshot", { format: "png", clip, captureBeyondViewport: true });
    await writeFile(path.join(SHOTS, `${name}.png`), Buffer.from(r.result.data, "base64"));
}
const text = id => js(`document.getElementById(${JSON.stringify(id)}).innerText`);
const visible = id => js(`!document.getElementById(${JSON.stringify(id)}).classList.contains("hidden")`);
async function openSystemTab() {
    await send("Page.navigate", { url: BASE + "/" });
    await sleep(1500);
    await js(`window.__opened = []; window.open = (u) => { window.__opened.push(u); return null; }; ` +
             `document.querySelector('.tab-btn[data-tab="system"]').click()`);
    await sleep(800);
}

await send("Page.enable");
await send("Runtime.enable");
await send("Emulation.setDeviceMetricsOverride", { width: 1000, height: 1600, deviceScaleFactor: 1, mobile: false });
try {
    // ---- 1. A tested, detected board -----------------------------------------------------------
    await openSystemTab();
    check((await text("board-model")).includes("ESP32-2432S028R (2.8\", USB-C + micro-USB)"), "model shown with size and feature");
    check(await text("board-status") === "Tested", "status Tested");
    check(await text("board-decision") === "Detected from the screen", "how it was found");
    check(await text("board-ids") === "8181B3 / FFFFFF / FFFFFF", "screen ID answers");
    check(!await visible("board-note"), "no notice for a tested, detected board");
    const opts = await js(`[...document.getElementById("board-select").options].map(o => o.value)`);
    check(opts[0] === "auto" && opts.length === 11, `picker: auto + 10 boards (${opts.length})`);
    check(await js(`document.getElementById("board-select").value`) === "auto", "picker shows the setting (auto)");
    check(await js(`document.querySelector('#board-select option[value="3248S035R"]').textContent.includes("(untested)")`),
          "untested boards are marked in the picker");
    await shot("1-board-tested");

    // ---- 2. Choosing a board the screen contradicts: warning, then "anyway" ------------------------
    await js(`document.getElementById("board-select").value = "3248S035R"; saveBoard()`);
    await sleep(17000);  // waitForDevice() gives a restarting CYD 15 s, then polls /api/status
    check(posts.at(-1) === "3248S035R", "POST /api/board with the chosen id");
    check((await text("board-msg")).includes("back"), "says the CYD is back after the restart");
    check(await visible("board-note") && (await text("board-note")).includes("reports a different chip"), "mismatch warning shown");
    await shot("2-board-mismatch");
    await js(`[...document.querySelectorAll("#board-note button")].find(b => b.textContent.includes("anyway")).click()`);
    await sleep(17000);
    check(posts.at(-1) === "3248S035R!", "Use it anyway posts <id>!");
    check((await text("board-note")).includes("hasn't been tested"), "untested notice after accepting");
    check(await text("board-decision") === "Your choice (kept despite the warning)", "decision text for anyway");
    await js(`saveBoard()`);  // saving the same board again keeps the "!"
    await sleep(500);
    check(posts.at(-1) === "3248S035R!", "re-saving keeps the accepted warning");

    // ---- 3. The report form and link ----------------------------------------------------------------
    await js(`toggleBoardReport(true)`);
    check(await visible("board-report"), "report card opens");
    check((await text("board-report-auto")).includes("v0.3.2"), "automatic part shows the firmware");
    await shot("3-board-report");
    await js(`document.getElementById("board-back").value = "ESP32-3248S035R & co #1"; ` +
             `document.querySelector('[data-work="picture"]').checked = true; document.querySelector('[data-work="touch"]').checked = true; ` +
             `document.getElementById("board-notes").value = "colours look 100% wrong\\nsecond line 😀"; openBoardReport()`);
    const link = (await js(`window.__opened`))[0] || "";
    const q = new URL(link).searchParams;
    check(link.startsWith("https://github.com/bmp/QRPickle/issues/new?template=board_report.yml&"), "opens GitHub's board report form");
    check(q.get("board") === "3248S035R" && q.get("decision") === "anyway", "board and decision in the link");
    check(q.get("back") === "ESP32-3248S035R & co #1", "number on the back round-trips");
    check(q.get("works") === "picture, touch", "what works");
    check(q.get("notes") === "colours look 100% wrong\nsecond line 😀", "notes round-trip");
    check(Object.values(SECRETS).every(s => !link.includes(s) && !link.includes(encodeURIComponent(s))), "no secrets in the link");

    // ---- 4. Not recognised, a stale setting, no touch ------------------------------------------------
    reset({ decision: "ambiguous", decision_code: "ambiguous", candidates: ["2432S032R", "2432S032C"], status: "mapped",
            setting_unknown: true, touch_on: false, touch_note: "touch not supported yet (CST816S)" });
    await openSystemTab();
    const note = await text("board-note");
    check(note.includes("wasn't recognised") && note.includes("2432S032R, 2432S032C"), "not-recognised notice lists the candidates");
    check(note.includes("doesn't know; it was ignored"), "stale setting explained");
    check(note.includes("touch not supported yet"), "touch note shown");
    await shot("4-board-not-recognised");

    // ---- 5. Back to auto -----------------------------------------------------------------------------
    reset({ setting: "3248S035R" });
    await openSystemTab();
    await js(`document.getElementById("board-select").value = "auto"; saveBoard()`);
    await sleep(17000);
    check(posts.at(-1) === "auto" && !await visible("board-note"), "back to auto, no notice");
} catch (e) {
    console.log("FAIL exception:", e.message);
    fail++;
} finally {
    ws.close();
    server.close();
}
console.log(`RESULT pass=${pass} fail=${fail}`);
process.exit(fail ? 1 : 0);
