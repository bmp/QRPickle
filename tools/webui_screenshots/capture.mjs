// Regenerates docs/screenshots/*.png from the web console served by mock_server.py, using a
// headless Chromium over the DevTools protocol (no npm packages). See README.md in this folder.
//   node tools/webui_screenshots/capture.mjs [devtools-port] [app-url]
import { writeFileSync } from "node:fs";

const DEVTOOLS = `http://127.0.0.1:${process.argv[2] || 9223}`;
const APP = process.argv[3] || "http://127.0.0.1:8765/";
const OUT = new URL("../../docs/screenshots/", import.meta.url);
const WIDTH = 1128;   // CSS px; captured at 2x like the previous screenshots

// name -> JavaScript run in the page before the capture
const SHOTS = {
    "WebUI.png":            `document.querySelector('.tab-btn[data-tab="basic"]').click()`,
    "AdvancedSettings.png": `document.querySelector('.tab-btn[data-tab="advanced"]').click()`,
    "Profiles.png":         `document.querySelector('.tab-btn[data-tab="profiles"]').click();
                             const s = document.getElementById("profile-select"); s.value = "Field_Day";
                             handleProfileSelectionChange()`,
    "ProfileEdit.png":      `const s = document.getElementById("profile-select"); s.value = "Field_Day";
                             editSelectedProfile()`,
    "SysInfo.png":          `document.querySelector('.tab-btn[data-tab="system"]').click()`,
    "OTA.png":              `document.querySelector('.tab-btn[data-tab="cloudota"]').click()`,
    "APRS_Msg.png":         `document.querySelector('.tab-btn[data-tab="aprsmsg"]').click(); fetchAprsMessages()`,
};

const sleep = ms => new Promise(r => setTimeout(r, ms));
const target = await (await fetch(`${DEVTOOLS}/json/new?about:blank`, { method: "PUT" })).json();
const ws = new WebSocket(target.webSocketDebuggerUrl);
await new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });
let nextId = 1;
const pending = new Map();
ws.onmessage = ev => {
    const msg = JSON.parse(ev.data);
    if (msg.id && pending.has(msg.id)) {
        const { resolve, reject } = pending.get(msg.id);
        pending.delete(msg.id);
        msg.error ? reject(new Error(msg.error.message)) : resolve(msg.result);
    }
};
const send = (method, params = {}) => new Promise((resolve, reject) => {
    const id = nextId++;
    pending.set(id, { resolve, reject });
    ws.send(JSON.stringify({ id, method, params }));
});

await send("Page.enable");
await send("Runtime.enable");
for (const [name, script] of Object.entries(SHOTS)) {
    await send("Emulation.setDeviceMetricsOverride", { width: WIDTH, height: 800, deviceScaleFactor: 2, mobile: false });
    await send("Page.navigate", { url: APP });
    await sleep(1500);
    const r = await send("Runtime.evaluate", { expression: script, awaitPromise: false });
    if (r.exceptionDetails) throw new Error(`${name}: ${r.exceptionDetails.text}`);
    await sleep(1500);
    const { cssContentSize } = await send("Page.getLayoutMetrics");
    const height = Math.ceil(cssContentSize.height);
    await send("Emulation.setDeviceMetricsOverride", { width: WIDTH, height, deviceScaleFactor: 2, mobile: false });
    await sleep(300);
    const shot = await send("Page.captureScreenshot", { format: "png" });
    writeFileSync(new URL(name, OUT), Buffer.from(shot.data, "base64"));
    console.log(`${name}  ${WIDTH * 2} x ${height * 2}`);
}
ws.close();
await fetch(`${DEVTOOLS}/json/close/${target.id}`);
