// End-to-end test of the web console ON A DEVICE, in a real (headless) Chromium page:
// admin password change, profiles (save, edit in form, apply, delete), backup and restore.
// Every result is cross-checked through the REST API. The device's settings and admin password
// are restored at the end. See README.md in this folder.
//   QRP_ADMIN_PW=... node tools/webui_e2e/device_test.mjs <device-ip> [devtools-port]
import { createHash, randomBytes } from "node:crypto";

const IP = process.argv[2];
const DEVTOOLS = `http://127.0.0.1:${process.argv[3] || 9223}`;
const ORIGINAL_PW = process.env.QRP_ADMIN_PW;
if (!IP || !ORIGINAL_PW) { console.error("usage: QRP_ADMIN_PW=... node device_test.mjs <device-ip> [devtools-port]"); process.exit(2); }
const BASE = `http://${IP}`;
const TEST_PROFILE = "E2E_Test";
const TEST_PW = "E2e!" + randomBytes(4).toString("hex");   // 12 printable chars, no spaces

let password = ORIGINAL_PW;   // what the browser answers to the login prompt
let passed = 0, failed = 0;
const check = (ok, what) => { ok ? passed++ : failed++; console.log(`${ok ? "PASS" : "FAIL"}  ${what}`); };
const sleep = ms => new Promise(r => setTimeout(r, ms));

// ---- REST API with Digest auth (MD5, qop=auth), for cross-checks --------------------------
async function api(path, { method = "GET", body, pw = password } = {}) {
    const url = BASE + path;
    const opts = { method, headers: body ? { "Content-Type": "application/json" } : {}, body: body && JSON.stringify(body) };
    let res = await fetch(url, opts);
    if (res.status !== 401) return res;
    const h = res.headers.get("www-authenticate") || "";
    const p = Object.fromEntries([...h.matchAll(/(\w+)="?([^",]+)"?/g)].map(m => [m[1], m[2]]));
    const md5 = s => createHash("md5").update(s).digest("hex");
    const cnonce = randomBytes(8).toString("hex"), nc = "00000001";
    const ha1 = md5(`admin:${p.realm}:${pw}`), ha2 = md5(`${method}:${path}`);
    const response = md5(`${ha1}:${p.nonce}:${nc}:${cnonce}:auth:${ha2}`);
    opts.headers.Authorization = `Digest username="admin", realm="${p.realm}", nonce="${p.nonce}", uri="${path}", ` +
        `algorithm=MD5, qop=auth, nc=${nc}, cnonce="${cnonce}", response="${response}"` + (p.opaque ? `, opaque="${p.opaque}"` : "");
    return fetch(url, opts);
}
const apiJson = async (path, o) => (await api(path, o)).json();

// ---- Chromium over the DevTools protocol --------------------------------------------------
const target = await (await fetch(`${DEVTOOLS}/json/new?about:blank`, { method: "PUT" })).json();
const ws = new WebSocket(target.webSocketDebuggerUrl);
await new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });
let nextId = 1;
const pending = new Map();
const dialogs = [];
ws.onmessage = ev => {
    const m = JSON.parse(ev.data);
    if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); return; }
    if (m.method === "Fetch.requestPaused") send("Fetch.continueRequest", { requestId: m.params.requestId });
    if (m.method === "Fetch.authRequired") send("Fetch.continueWithAuth", { requestId: m.params.requestId,
        authChallengeResponse: { response: "ProvideCredentials", username: "admin", password } });
    if (m.method === "Page.javascriptDialogOpening") {
        dialogs.push(m.params.message);
        send("Page.handleJavaScriptDialog", { accept: true });   // OK to alerts and confirms
    }
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
async function dialog(match, timeout = 10000) {
    const start = Date.now();
    while (Date.now() - start < timeout) {
        const i = dialogs.findIndex(d => match.test(d));
        if (i >= 0) return dialogs.splice(i, 1)[0];
        await sleep(100);
    }
    return null;
}
async function open() {
    await send("Page.navigate", { url: BASE + "/" });
    await sleep(3500);
}

await send("Page.enable");
await send("Runtime.enable");
await send("Fetch.enable", { handleAuthRequests: true, patterns: [{ urlPattern: "*" }] });

const original = await apiJson("/api/config");
const SECRET_KEYS = ["password", "apikey", "aprs_pass", "hamalert_pass"];
try {
    // ---- 1. Admin password ----------------------------------------------------------------
    await open();
    check(await js(`!!document.getElementById("cfg-admin-pw")`), "page loads after login; admin password fields present");

    await js(`setElementValue("cfg-admin-pw", "abc12345"); setElementValue("cfg-admin-pw2", "abc12346"); saveActiveConfig()`);
    check(!!await dialog(/do not match/), "mismatched passwords rejected in the browser");
    await js(`setElementValue("cfg-admin-pw", "short"); setElementValue("cfg-admin-pw2", "short"); saveActiveConfig()`);
    check(!!await dialog(/8-16 characters/), "short password rejected in the browser");
    check((await api("/api/status", { pw: ORIGINAL_PW })).status === 200, "admin password unchanged after rejected inputs");

    await js(`setElementValue("cfg-admin-pw", ${JSON.stringify(TEST_PW)}); setElementValue("cfg-admin-pw2", ${JSON.stringify(TEST_PW)}); saveActiveConfig()`);
    password = TEST_PW;   // the next prompt must be answered with the new password
    check(!!await dialog(/admin password has changed/), "valid password saved; page says to log in again");
    await sleep(2000);
    check((await api("/api/status", { pw: ORIGINAL_PW })).status === 401, "old password refused");
    check((await api("/api/status", { pw: TEST_PW })).status === 200, "new password accepted");
    await open();
    check(await js(`document.getElementById("cfg-callsign").value`) === original.callsign, "console works after re-login with the new password");

    await js(`setElementValue("cfg-admin-pw", ${JSON.stringify(ORIGINAL_PW)}); setElementValue("cfg-admin-pw2", ${JSON.stringify(ORIGINAL_PW)}); saveActiveConfig()`);
    password = ORIGINAL_PW;
    await dialog(/admin password has changed/);
    await sleep(2000);
    check((await api("/api/status", { pw: ORIGINAL_PW })).status === 200, "admin password changed back through the UI");

    // ---- 2. Profiles ----------------------------------------------------------------------
    await open();
    await js(`document.getElementById("new-profile-name").value = ${JSON.stringify(TEST_PROFILE)}; saveProfile()`);
    check(!!await dialog(/saved/), "Save New Profile");
    await sleep(1500);
    check((await apiJson("/api/profiles")).includes(TEST_PROFILE), "profile listed by the device");

    const newTheme = (original.theme_id + 1) % 7, newMacro = "E2E macro " + Date.now() % 10000;
    await js(`document.getElementById("profile-select").value = ${JSON.stringify(TEST_PROFILE)}; handleProfileSelectionChange(); editSelectedProfile()`);
    await sleep(1500);
    check(await js(`!document.getElementById("profile-edit-banner").classList.contains("hidden")`), "edit mode: banner shown");
    check(await js(`document.getElementById("btn-save-config").innerText`) === "Save to Profile", "edit mode: button says Save to Profile");
    check(await js(`[...document.querySelectorAll(".admin-pw-group")].every(e => e.classList.contains("hidden"))`), "edit mode: admin password fields hidden");
    await js(`setElementValue("cfg-theme", ${newTheme}); setElementValue("cfg-mac0", ${JSON.stringify(newMacro)}); saveActiveConfig()`);
    check(!!await dialog(/device settings are unchanged/), "Save to Profile");
    await sleep(1500);
    const live1 = await apiJson("/api/config");
    const prof = await apiJson(`/api/profiles/get?name=${TEST_PROFILE}`);
    check(live1.theme_id === original.theme_id && live1.aprs_macros[0] === original.aprs_macros[0], "device settings unchanged by the profile edit");
    check(prof.theme_id === newTheme && prof.aprs_macros[0] === newMacro, "profile holds the edited theme and APRS macro");
    check(prof.password_set === original.password_set, "profile kept the WiFi password from the snapshot");
    check(await js(`document.getElementById("profile-edit-banner").classList.contains("hidden")`), "edit mode ends after saving");

    await js(`document.getElementById("profile-select").value = ${JSON.stringify(TEST_PROFILE)}; applySelectedProfile()`);
    check(!!await dialog(/Apply profile/), "Apply asks for confirmation");
    check(!!await dialog(/Profile applied/), "Apply to Device");
    await sleep(4000);
    const live2 = await apiJson("/api/config");
    check(live2.theme_id === newTheme && live2.aprs_macros[0] === newMacro, "applied profile is live (theme, APRS macro)");
    check((await api("/api/status")).status === 200, "device still reachable on WiFi after applying");

    // ---- 3. Backup and restore ------------------------------------------------------------
    await open();
    await js(`window.__blob = null; const o = URL.createObjectURL; URL.createObjectURL = b => { window.__blob = b; return o.call(URL, b); }; downloadBackup()`);
    await sleep(4000);
    const backupText = await js(`window.__blob ? window.__blob.text() : null`);
    const backup = backupText && JSON.parse(backupText);
    check(backup?.format === "qrpickle-backup" && backup.profiles?.[TEST_PROFILE], "Download Backup: settings and the test profile");
    const leaks = backup ? [backup.settings, ...Object.values(backup.profiles)].flatMap(c => SECRET_KEYS.filter(k => c[k])) : ["no backup"];
    check(leaks.length === 0, "backup contains no passwords or keys");

    await js(`document.getElementById("profile-select").value = ${JSON.stringify(TEST_PROFILE)}; deleteSelectedProfile()`);
    await dialog(/Delete profile/);
    await sleep(2000);
    check(!(await apiJson("/api/profiles")).includes(TEST_PROFILE), "Delete");

    await js(`restoreBackup({ files: [new File([${JSON.stringify(backupText)}], "backup.json")], value: "" })`);
    check(!!await dialog(/Restore the device settings/), "Restore asks for confirmation");
    check(!!await dialog(/Backup restored\./, 20000), "Restore from Backup");
    await sleep(4000);
    check((await apiJson("/api/profiles")).includes(TEST_PROFILE), "restored profile is back");
    check((await api("/api/status")).status === 200, "device still reachable on WiFi after restoring");
} finally {
    // ---- Clean up: original settings, admin password, no test profile ----------------------
    password = ORIGINAL_PW;
    for (const pw of [ORIGINAL_PW, TEST_PW]) {
        if ((await api("/api/status", { pw })).status === 200) {
            await api("/api/config/save", { method: "POST", body: { ...original, admin_pw: ORIGINAL_PW }, pw });
            break;
        }
    }
    await sleep(2000);
    await api(`/api/profiles/delete?name=${TEST_PROFILE}`, { method: "POST" });
    const after = await apiJson("/api/config");
    const same = JSON.stringify(after) === JSON.stringify(original);
    check(same, "device settings restored exactly");
    ws.close();
    await fetch(`${DEVTOOLS}/json/close/${target.id}`);
}
console.log(`RESULT pass=${passed} fail=${failed}`);
process.exit(failed ? 1 : 0);
