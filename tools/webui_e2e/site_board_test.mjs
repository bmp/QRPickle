// Website board check (v0.3.2): the Install page's "Before installing" dialog and the Set up over USB
// page's board list, on a locally built site (scripts/make_pages_site.py --out <dir>). No board needed.
//
//   python3 -m http.server -d <site-out> 8765 &
//   chromium --headless=new --remote-debugging-port=9223 --user-data-dir="$(mktemp -d)" about:blank &
//   node tools/webui_e2e/site_board_test.mjs http://localhost:8765 9223 [screenshot-dir]
import { writeFile, mkdir } from "node:fs/promises";
import path from "node:path";

const SITE = process.argv[2] || "http://localhost:8765";
const DEVTOOLS = `http://127.0.0.1:${process.argv[3] || 9223}`;
const SHOTS = process.argv[4];
const sleep = ms => new Promise(r => setTimeout(r, ms));
let pass = 0, fail = 0;
const check = (ok, what) => { console.log(`${ok ? "PASS" : "FAIL"} ${what}`); ok ? pass++ : fail++; };

const target = await (await fetch(`${DEVTOOLS}/json/new?about:blank`, { method: "PUT" })).json();
const ws = new WebSocket(target.webSocketDebuggerUrl);
await new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });
let nextId = 1;
const pending = new Map();
ws.onmessage = ev => { const m = JSON.parse(ev.data); if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); } };
const send = (method, params = {}) => new Promise(res => { const id = nextId++; pending.set(id, res); ws.send(JSON.stringify({ id, method, params })); });
async function js(expression) {
    const r = await send("Runtime.evaluate", { expression, awaitPromise: true, returnByValue: true });
    if (r.result?.exceptionDetails) throw new Error(r.result.exceptionDetails.exception?.description || expression);
    return r.result?.result?.value;
}
async function shot(name) {
    if (!SHOTS) return;
    await mkdir(SHOTS, { recursive: true });
    const r = await send("Page.captureScreenshot", { format: "png" });
    await writeFile(path.join(SHOTS, `${name}.png`), Buffer.from(r.result.data, "base64"));
}

await send("Page.enable");
await send("Runtime.enable");
await send("Emulation.setDeviceMetricsOverride", { width: 1100, height: 1300, deviceScaleFactor: 1, mobile: false });
try {
    // ---- Install: the dialog comes first -----------------------------------------------------------
    await send("Page.navigate", { url: `${SITE}/install/` });
    await sleep(2500);
    await js(`document.querySelector('#install [slot="activate"]').click()`);
    await sleep(300);
    check(await js(`document.getElementById("board-check").open`), "Install opens the board check first");
    check(await js(`document.querySelectorAll("#board-check-list tr").length`) === 10, "lists the 10 boards");
    check(await js(`/Tested|Untested|Reported/.test(document.getElementById("board-check-list").innerText)`), "statuses shown");
    check(await js(`document.querySelector("#board-check img").complete && document.querySelector("#board-check img").naturalWidth > 0`),
          "photo of the back loads");
    await shot("5-install-board-check");
    await js(`document.getElementById("board-check-cancel").click()`);
    check(!await js(`document.getElementById("board-check").open`), "Cancel closes it");
    // Continue: the dialog closes and the click reaches ESP Web Tools (count clicks on its button). Its
    // port chooser would block a headless browser: the port request fails as if the user cancelled.
    await js(`if (navigator.serial) navigator.serial.requestPort = () => Promise.reject(new DOMException("No port selected", "NotFoundError"));
              window.__clicks = 0; document.querySelector('#install [slot="activate"]').addEventListener("click", () => window.__clicks++);
              document.querySelector('#install [slot="activate"]').click()`);
    await sleep(200);
    await js(`document.getElementById("board-check-continue").click()`);
    await sleep(200);
    // The first click stops in front of the button (dialog); only Continue's click reaches it.
    check(!await js(`document.getElementById("board-check").open`) && await js(`window.__clicks`) === 1,
          "Continue closes it and passes the click on to the installer");

    // ---- Set up over USB: the board box ------------------------------------------------------------
    await send("Page.navigate", { url: `${SITE}/setup/` });
    await sleep(2000);
    const opts = await js(`[...document.getElementById("board").options].map(o => o.value)`);
    check(opts[0] === "auto" && opts.length === 11, `board choice: auto + 10 boards (${opts.length})`);
    check(await js(`!document.getElementById("setup-form").contains(document.getElementById("board"))`),
          "the board card is separate from the setup form");
    check(await js(`!!document.getElementById("board-send") && !!document.getElementById("board-check")`),
          "the board card has its own check and send buttons");
    check(await js(`document.getElementById("board-report").classList.contains("hidden")`), "report hidden until the board was read");
    await js(`document.getElementById("board-photo-link").click()`);
    check(!await js(`document.getElementById("board-photo").classList.contains("hidden")`), "where to look: photo shows");
    await shot("6-setup-board");
} catch (e) {
    console.log("FAIL exception:", e.message);
    fail++;
}
ws.close();
console.log(`RESULT pass=${pass} fail=${fail}`);
process.exit(fail ? 1 : 0);
