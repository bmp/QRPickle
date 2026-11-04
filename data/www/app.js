// Escape text from the device (APRS messages arrive from the internet) before putting it
// into innerHTML (review 1.1).
function esc(v) {
    return String(v ?? "").replace(/[&<>"']/g, c => ({"&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"}[c]));
}

// Secrets are never sent to the browser (review 1.3). Show that one is stored; leaving the
// field blank keeps it.
// Screen cycling order: the list's rows carry the screen ids (0..6); the device checks the order.
function renderCycleOrder(order) {
    const ol = document.getElementById("cyc-order");
    if (!ol) return;
    const ids = /^[0-6](,[0-6]){6}$/.test(order || "") ? order.split(",") : ["0", "1", "2", "3", "4", "5", "6"];
    for (const id of ids) {
        const li = ol.querySelector(`li[data-id="${id}"]`);
        if (li) ol.appendChild(li);
    }
}

function cycleOrderValue() {
    const ol = document.getElementById("cyc-order");
    return ol ? [...ol.children].map(li => li.dataset.id).join(",") : "0,1,2,3,4,5,6";
}

function moveCycle(btn, dir) {
    const li = btn.closest("li"), ol = li.parentElement;
    if (dir < 0 && li.previousElementSibling) ol.insertBefore(li, li.previousElementSibling);
    if (dir > 0 && li.nextElementSibling) ol.insertBefore(li.nextElementSibling, li);
}

// Secrets are write-only (review 1.3): the device says whether one is saved and, for 6+
// characters, its last 2 ("<key>_tail"); Clear sends "<key>_clear".
const SECRET_FIELDS = [["cfg-password", "password"], ["cfg-apikey", "apikey"],
                       ["cfg-hamalert-pass", "hamalert_pass"], ["cfg-aprs-pass", "aprs_pass"]];
const pendingClear = {};

function markSaved(id, isSet, tail) {
    const el = document.getElementById(id);
    delete pendingClear[id];
    if (el) el.placeholder = !isSet ? "" : tail ? `saved, ends in ••${tail} (leave blank to keep)` : "saved (leave blank to keep)";
}

function clearSecret(id) {
    const el = document.getElementById(id);
    if (!el) return;
    pendingClear[id] = true;
    el.value = "";
    el.placeholder = "will be cleared when you save";
}

// Backups and kept data never carry the tails (they are a hint for this page only).
function stripTails(obj) {
    if (obj && typeof obj === "object") for (const k of Object.keys(obj)) if (k.endsWith("_tail")) delete obj[k];
    return obj;
}

document.addEventListener("DOMContentLoaded", () => {
    initTabsEngine();
    loadCurrentConfig();
    
    // Core Background Telemetry Thread Poller Loop
    setInterval(() => {
        fetchSystemTelemetry();
        fetchAprsMessages();
    }, 3000);
});

// Defensive Value Mappers to safeguard against thread stalls
function setElementValue(id, value, isCheckbox = false) {
    const el = document.getElementById(id);
    if (!el) return; 
    if (isCheckbox) {
        el.checked = !!value;
    } else {
        el.value = value;
    }
}

function getElementValue(id, isCheckbox = false) {
    const el = document.getElementById(id);
    if (!el) return isCheckbox ? false : "";
    return isCheckbox ? el.checked : el.value;
}

// Helper to extract and mirror tool descriptors dynamically to the footer layout
function updateDynamicFooter(data) {
    const nameEl = document.getElementById("foot-name");
    const verEl = document.getElementById("foot-version");
    const authcallEl = document.getElementById("foot-authorcall"); // FIXED: Target the correct element ID

    if (nameEl && (data.app_name || data.fw_name)) {
        nameEl.innerText = data.app_name || data.fw_name;
    }

    if (verEl && (data.version || data.fw_version)) {
        verEl.innerText = data.version || data.fw_version;
    }

    // FIXED: Write the callsign to the correct element, falling back to the configured station callsign if author_call isn't present
    if (authcallEl && (data.author_call || data.callsign)) {
        authcallEl.innerText = data.author_call || data.callsign;
    }
}

// --- TAB SWITCHER INITIALIZATION VECTOR ---
function initTabsEngine() {
    const buttons = document.querySelectorAll(".tab-btn");
    buttons.forEach(btn => {
        btn.addEventListener("click", () => {
            const targetTab = btn.getAttribute("data-tab");
            
            document.querySelectorAll(".tab-btn").forEach(b => b.classList.remove("active"));
            btn.classList.add("active");
            
            document.querySelectorAll(".tab-content").forEach(content => {
                content.classList.remove("active");
                if (content.id === `tab-${targetTab}`) {
                    content.classList.add("active");
                }
            });
            
            if (targetTab === "system") { fetchSystemTelemetry(); refreshLogos(); }
            if (targetTab === "about") fetchAboutDetails();
            if (targetTab === "cloudota") fetchCloudOTADetails();
        });
    });
}

// --- SETTINGS FORM ---
// The Basic/Advanced form edits either the live device settings or, in profile edit mode, a
// stored profile. Keys are the device's (/api/config and profile files share one field table).
let editingProfile = null;

function fillForm(data) {
    setElementValue("cfg-callsign", data.callsign || "");
    setElementValue("cfg-grid", data.grid || "");
    setElementValue("cfg-ssid", data.ssid || "");
    setElementValue("cfg-lat", data.lat ?? 12.97);
    setElementValue("cfg-lon", data.lon ?? 77.59);
    setElementValue("cfg-offset", data.offset ?? 5.5);
    setElementValue("cfg-brightness", data.brightness ?? 180);
    setElementValue("cfg-auto-bright", !!data.auto_bright, true);
    setElementValue("cfg-timeout", data.timeout ?? 5);
    setElementValue("cfg-cycle-en", !!data.cycle_en, true);
    setElementValue("cfg-cycle-s", data.cycle_s ?? 10);
    const cycleMask = data.cycle_screens ?? 127;
    for (let i = 0; i < 7; i++) setElementValue(`cyc-bit${i}`, (cycleMask & (1 << i)) !== 0, true);
    renderCycleOrder(data.cycle_order);
    setElementValue("cfg-theme", data.theme_id ?? 0);

    // Secrets are never sent to the browser (review 1.3): blank fields keep the stored value.
    for (const [id, key] of SECRET_FIELDS) {
        setElementValue(id, "");
        markSaved(id, data[key + "_set"], data[key + "_tail"]);
    }
    setElementValue("cfg-admin-pw", "");
    setElementValue("cfg-admin-pw2", "");

    setElementValue("cfg-aprs-en", data.aprs_en ? "1" : "0");
    setElementValue("cfg-aprs-ssid", data.aprs_ssid ?? 0);
    setElementValue("cfg-aprs-icon", data.aprs_icn || "/[");
    setElementValue("cfg-aprs-cmt", data.aprs_cmt || "");

    const mask = data.fc_slots ?? 15;
    for (let i = 0; i < 8; i++) setElementValue(`fc-bit${i}`, (mask & (1 << i)) !== 0, true);

    const macros = Array.isArray(data.aprs_macros) ? data.aprs_macros : [];
    for (let i = 0; i < 5; i++) setElementValue(`cfg-mac${i}`, macros[i] || "");

    setElementValue("dx_url_p", data.dx_url_p || "");
    setElementValue("dx_port_p", data.dx_port_p ?? 7300);
    setElementValue("dx_url_s", data.dx_url_s || "");
    setElementValue("dx_port_s", data.dx_port_s ?? 7373);
    setElementValue("solar_url", data.solar_url || "");
    setElementValue("cfg-latlon-set", !!data.latlon_set, true);
    const groups = data.band_groups ?? 31;
    for (let i = 0; i < 5; i++) setElementValue(`bg-bit${i}`, (groups & (1 << i)) !== 0, true);
    const satCats = data.sat_categories ?? 15;
    for (let i = 0; i < 5; i++) setElementValue(`sat-bit${i}`, (satCats & (1 << i)) !== 0, true);
    setElementValue("sat-min-el", data.sat_min_el ?? 10);
    setElementValue("pass-alert-en", data.pass_alert_en ?? true, true);
    setElementValue("pass-alert-min", data.pass_alert_min ?? 30);
    setElementValue("space_url", data.space_url || "");
    checkLatLonGrid();
}

// Everything except the admin password, which profiles never hold.
function collectForm() {
    let forecastMask = 0;
    for (let i = 0; i < 8; i++) {
        if (getElementValue(`fc-bit${i}`, true)) forecastMask |= (1 << i);
    }
    const macros = [];
    for (let i = 0; i < 5; i++) macros.push(getElementValue(`cfg-mac${i}`));
    let bandGroups = 0;
    for (let i = 0; i < 5; i++) {
        if (getElementValue(`bg-bit${i}`, true)) bandGroups |= (1 << i);
    }

    const form = {
        callsign: getElementValue("cfg-callsign"),
        grid: getElementValue("cfg-grid"),
        ssid: getElementValue("cfg-ssid"),
        password: getElementValue("cfg-password"),
        apikey: getElementValue("cfg-apikey"),
        lat: parseFloat(getElementValue("cfg-lat")),
        lon: parseFloat(getElementValue("cfg-lon")),
        offset: parseFloat(getElementValue("cfg-offset")),
        brightness: parseInt(getElementValue("cfg-brightness")),
        auto_bright: getElementValue("cfg-auto-bright", true),
        theme_id: parseInt(getElementValue("cfg-theme")),
        timeout: parseInt(getElementValue("cfg-timeout")),
        cycle_en: getElementValue("cfg-cycle-en", true),
        cycle_s: parseInt(getElementValue("cfg-cycle-s")) || 10,
        cycle_screens: [0, 1, 2, 3, 4, 5, 6].reduce((m, i) => m | (getElementValue(`cyc-bit${i}`, true) ? 1 << i : 0), 0) || 127,
        cycle_order: cycleOrderValue(),
        fc_slots: forecastMask,
        dx_url_p: getElementValue("dx_url_p"),
        dx_port_p: parseInt(getElementValue("dx_port_p")),
        dx_url_s: getElementValue("dx_url_s"),
        dx_port_s: parseInt(getElementValue("dx_port_s")),
        solar_url: getElementValue("solar_url").trim(),
        latlon_set: getElementValue("cfg-latlon-set", true),
        band_groups: bandGroups || 31,  // the device also refuses "none"
        sat_categories: [0, 1, 2, 3, 4].reduce((m, i) => m | (getElementValue(`sat-bit${i}`, true) ? 1 << i : 0), 0) || 15,
        sat_min_el: parseInt(getElementValue("sat-min-el")) || 0,
        pass_alert_en: getElementValue("pass-alert-en", true),
        pass_alert_min: parseInt(getElementValue("pass-alert-min")) || 30,
        space_url: getElementValue("space_url").trim(),
        aprs_en: getElementValue("cfg-aprs-en") === "1",
        aprs_pass: getElementValue("cfg-aprs-pass"),
        aprs_ssid: parseInt(getElementValue("cfg-aprs-ssid")),
        aprs_icn: getElementValue("cfg-aprs-icon"),
        aprs_cmt: getElementValue("cfg-aprs-cmt"),
        aprs_macros: macros,
        hamalert_pass: getElementValue("cfg-hamalert-pass")
    };
    for (const [id, key] of SECRET_FIELDS) if (pendingClear[id]) form[key + "_clear"] = true;
    return form;
}

function loadCurrentConfig() {
    fetch("/api/config")
        .then(res => res.json())
        .then(data => {
            updateDynamicFooter(data);
            if (!editingProfile) fillForm(data);
            fetchProfilesList();
        })
        .catch(err => console.error("Could not fetch device configuration variables:", err));
}

function saveActiveConfig() {
    if (editingProfile) { saveFormToProfile(); return; }
    const payload = collectForm();

    // Same rules as config::sanitize(): 8-16 printable characters, no spaces.
    const adminPw = getElementValue("cfg-admin-pw");
    if (adminPw || getElementValue("cfg-admin-pw2")) {
        if (adminPw !== getElementValue("cfg-admin-pw2")) { alert("The two admin passwords do not match."); return; }
        if (!/^[\x21-\x7E]{8,16}$/.test(adminPw)) { alert("Admin password: 8-16 characters, no spaces."); return; }
        payload.admin_pw = adminPw;
    }

    fetch("/api/config/save", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload)
    })
    .then(res => {
        if (!res.ok) { alert("Server rejected update payload."); return; }
        alert(payload.admin_pw
            ? "Configuration saved. The admin password has changed: your browser will ask you to log in again (user \"admin\")."
            : "Configuration saved successfully to NVS memory!");
    })
    .then(() => loadCurrentConfig())
    .catch(err => alert("Transmission line error saving configuration parameters: " + err));
}

function fetchSystemTelemetry() {
    const sysTab = document.getElementById("tab-system");
    if (!sysTab || !sysTab.classList.contains("active")) return;

    fetch("/api/status") 
        .then(res => res.json())
        .then(data => {
            updateDynamicFooter(data);
            document.getElementById("sys-uptime").innerText = (data.uptime || 0) + "s";
            document.getElementById("sys-heap").innerText = (data.heap || 0).toLocaleString() + " B"; 
            document.getElementById("sys-rssi").innerText = (data.rssi || 0) + " dBm";
            document.getElementById("sys-temp").innerText = (data.temp ?? "--.-") + " °C";
            document.getElementById("sys-humidity").innerText = (data.humidity ?? "--.-") + " %";
            document.getElementById("sys-pressure").innerText = (data.pressure ?? "----") + " hPa";

            document.getElementById("sys-lib-lvgl").innerText = data.ver_lvgl || "v9.x";
            document.getElementById("sys-lib-json").innerText = data.ver_json || "v7.x";
            document.getElementById("sys-lib-core").innerText = data.ver_core || "v6.x";
            document.getElementById("sys-lib-idf").innerText = data.ver_idf || "v5.x";
        })
        .catch(err => console.warn("Polling dropped telemetry. Re-syncing line link...", err));
}

function fetchAboutDetails() {
    fetch("/api/about")
        .then(res => res.ok ? res.text() : "No details recorded on disk flash.")
        .then(text => {
            try {
                // If endpoint returns structured payload layout, parse variables cleanly
                const data = JSON.parse(text);
                updateDynamicFooter(data);
                document.getElementById("about-bin").innerText = data.description || text;
            } catch (e) {
                document.getElementById("about-bin").innerText = text;
            }
            
            // FIXED: Retains raw hardcoded link targets definitions and parameters securely
            const link = document.getElementById("foot-link");
            if (link) { link.href = "https://ham.bharathpalavalli.com/"; }
        });
}

// --- FAIL-SAFE OTA MAINTENANCE TRANSFERS ---
async function executeWirelessOTA() {
    const target = document.getElementById("ota-target").value;
    const fileInput = document.getElementById("ota-file-input");
    
    if (fileInput.files.length === 0) {
        alert("Please pick a valid compiled payload file binary target first.");
        return;
    }

    const file = fileInput.files[0];
    const formData = new FormData();
    formData.append("update", file);

    const progressContainer = document.getElementById("ota-progress-container");
    const progressBar = document.getElementById("ota-progress-bar");
    const pctLabel = document.getElementById("ota-pct-lbl");
    const statusLabel = document.getElementById("ota-status-lbl");

    progressContainer.classList.remove("hidden");
    let keep = null;
    if (target === "filesystem") {
        statusLabel.innerText = "Saving profiles and logos before the update...";
        try { keep = await collectKeep(); } catch (e) {
            if (!confirm("Profiles and logos could not be read (" + e + "). Update anyway? They will be erased.")) {
                progressContainer.classList.add("hidden");
                return;
            }
        }
    }
    statusLabel.innerText = "Uploading payload packages to file server...";

    const xhr = new XMLHttpRequest();
    xhr.open("POST", `/api/system/update?target=${target}`, true); 

    xhr.upload.addEventListener("progress", (e) => {
        if (e.lengthComputable) {
            const percentage = Math.round((e.loaded / e.total) * 100);
            progressBar.style.width = percentage + "%";
            pctLabel.innerText = percentage + "%";
            if (percentage === 100) statusLabel.innerText = "Burning layout configurations to flash sectors... Do NOT power down.";
        }
    });

    xhr.onreadystatechange = () => {
        if (xhr.readyState === 4) {
            if (xhr.status === 200) {
                if (keep) {
                    const n = Object.keys(keep.profiles).length, l = Object.keys(keep.logos).length;
                    statusLabel.innerText = `Waiting for the device to restart, then restoring ${n} profile(s) and ${l} logo(s)...`;
                    waitForDevice().then(async up => {
                        const failed = up ? await restoreKeep(keep) : ["device did not come back"];
                        if (failed.length) {
                            alert("Restore incomplete: " + failed.join(", ") + ". Your profiles and logos are downloaded as a file; restore it under Profiles.");
                            offerKeepDownload(keep);
                        } else {
                            alert(`Filesystem updated; ${n} profile(s) and ${l} logo(s) restored.`);
                        }
                        progressContainer.classList.add("hidden");
                    });
                    return;
                }
                statusLabel.innerText = "Upload completed! Rebooting hardware processor core...";
                alert("OTA patch successfully written. The device will now flash reboot.");
                triggerReboot();
            } else {
                statusLabel.innerText = "OTA Transaction Rejection.";
                alert("The server refused the file block link: " + xhr.responseText);
                progressContainer.classList.add("hidden");
            }
        }
    };

    xhr.send(formData);
}

function togglePass(id) {
    const input = document.getElementById(id);
    if (input) input.type = input.type === "password" ? "text" : "password";
}

function triggerReboot() {
    fetch("/api/system/reboot", { method: "POST" })
        .then(() => alert("Reboot signal issued! Reconnecting portal once server initialization pass returns online."))
        .catch(() => alert("Dispatched hardware loop core reset successfully."));
}

// --- PROFILE LAYOUT MANAGER STORAGE PIPELINE ---
// --- PROFILES ---
const PROFILE_NAME_RE = /^[A-Za-z0-9_-]{1,24}$/;

function fetchProfilesList() {
    return fetch("/api/profiles")
        .then(res => res.json())
        .then(data => {
            const dropdown = document.getElementById("profile-select");
            if (!dropdown) return [];
            dropdown.innerHTML = '<option value="">-- No Profile Selected --</option>';
            if (Array.isArray(data)) {
                data.forEach(pName => {
                    dropdown.innerHTML += `<option value="${esc(pName)}">${esc(pName)}</option>`;
                });
            }
            handleProfileSelectionChange();
            return Array.isArray(data) ? data : [];
        });
}

function handleProfileSelectionChange() {
    const selected = !!document.getElementById("profile-select").value;
    document.querySelectorAll(".profile-needs-selection").forEach(b => b.disabled = !selected);
}

function postProfile(name, config, restore = false) {
    return fetch("/api/profiles/save", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ name, config, restore })
    });
}

function saveProfile() {
    const pName = document.getElementById("new-profile-name").value.trim();
    if (!PROFILE_NAME_RE.test(pName)) { alert("Profile name: 1-24 letters, digits, - or _ (no spaces)."); return; }
    // Blank secret fields take the device's current ones.
    postProfile(pName, collectForm())
        .then(res => res.ok ? alert(`Profile "${pName}" saved.`) : alert("Server rejected the profile."))
        .then(() => {
            document.getElementById("new-profile-name").value = "";
            fetchProfilesList();
        })
        .catch(err => alert("Error saving the profile: " + err));
}

function applySelectedProfile() {
    const pName = document.getElementById("profile-select").value;
    if (!pName || !confirm(`Apply profile "${pName}" to the device now?`)) return;
    fetch(`/api/profiles/load?name=${encodeURIComponent(pName)}`, { method: "POST" })
        .then(res => res.ok ? alert("Profile applied. Reloading the settings...") : alert("Profile could not be applied."))
        .then(() => window.location.reload());
}

function editSelectedProfile() {
    const pName = document.getElementById("profile-select").value;
    if (!pName) return;
    fetch(`/api/profiles/get?name=${encodeURIComponent(pName)}`)
        .then(res => { if (!res.ok) throw new Error("not found"); return res.json(); })
        .then(data => {
            editingProfile = pName;
            fillForm(data);
            document.getElementById("profile-edit-name").innerText = pName;
            document.getElementById("profile-edit-banner").classList.remove("hidden");
            document.querySelectorAll(".admin-pw-group").forEach(el => el.classList.add("hidden"));
            document.getElementById("btn-save-config").innerText = "Save to Profile";
            document.querySelector('.tab-btn[data-tab="basic"]').click();
        })
        .catch(err => alert("Could not load the profile: " + err));
}

function stopEditingProfile() {
    editingProfile = null;
    document.getElementById("profile-edit-banner").classList.add("hidden");
    document.querySelectorAll(".admin-pw-group").forEach(el => el.classList.remove("hidden"));
    document.getElementById("btn-save-config").innerText = "Save Configuration";
    loadCurrentConfig();
}

function saveFormToProfile() {
    const pName = editingProfile;
    // Blank secret fields keep the profile's own ones.
    postProfile(pName, collectForm())
        .then(res => {
            if (!res.ok) { alert("Server rejected the profile changes."); return; }
            alert(`Profile "${pName}" saved. The device settings are unchanged.`);
            stopEditingProfile();
        })
        .catch(err => alert("Error saving the profile: " + err));
}

function deleteSelectedProfile() {
    const pName = document.getElementById("profile-select").value;
    if (!pName || !confirm(`Delete profile "${pName}"?`)) return;
    fetch(`/api/profiles/delete?name=${encodeURIComponent(pName)}`, { method: "POST" })
        .then(res => res.ok ? null : alert("Profile could not be deleted."))
        .then(() => fetchProfilesList());
}

// --- SPLASH LOGOS (logo.js converts; the device checks and stores) ---
const pendingLogos = { left: null, right: null };

async function refreshLogos() {
    try {
        const s = await (await fetch("/api/logos")).json();
        for (const side of ["left", "right"]) {
            const el = document.getElementById(`logo-state-${side}`);
            if (el) el.textContent = s[side] && s[side].custom ? `custom, ${s[side].w} x ${s[side].h}` : "built-in";
        }
    } catch (e) { /* device busy: leave "--" */ }
}

function pickLogo(side, input) {
    const file = input.files[0];
    if (!file) return;
    const reader = new FileReader();  // data: URL (the CSP allows data: images, not blob:)
    reader.onload = () => {
        const img = new Image();
        img.onload = () => {
            const { w, h } = logoFitBox(img.naturalWidth || 60, img.naturalHeight || 60);
            const c = document.createElement("canvas");
            c.width = w; c.height = h;
            const ctx = c.getContext("2d");
            ctx.imageSmoothingQuality = "high";
            ctx.drawImage(img, 0, 0, w, h);
            const rgba = ctx.getImageData(0, 0, w, h).data;
            pendingLogos[side] = logoEncode(rgba, w, h);
            for (const tone of ["dark", "light"]) {
                const p = document.getElementById(`logo-prev-${side}-${tone}`).getContext("2d");
                p.clearRect(0, 0, 60, 60);
                p.drawImage(c, Math.floor((60 - w) / 2), Math.floor((60 - h) / 2));
            }
            document.getElementById(`logo-up-${side}`).disabled = false;
        };
        img.onerror = () => alert("This file could not be read as an image.");
        img.src = reader.result;
    };
    reader.readAsDataURL(file);
}

async function postLogo(side, bytes) {
    const form = new FormData();
    form.append("logo", new Blob([bytes], { type: "application/octet-stream" }), `${side}.bin`);
    const res = await fetch(`/api/logos/upload?side=${side}`, { method: "POST", body: form });
    if (!res.ok) throw new Error((await res.json().catch(() => ({}))).error || `HTTP ${res.status}`);
}

async function uploadLogo(side) {
    if (!pendingLogos[side]) return;
    try {
        await postLogo(side, pendingLogos[side]);
        alert("Logo saved. It shows from the next restart.");
    } catch (e) {
        alert("Logo not saved: " + e.message);
    }
    refreshLogos();
}

async function deleteLogo(side) {
    if (!confirm(`Use the built-in ${side} logo again?`)) return;
    await fetch(`/api/logos/delete?side=${side}`, { method: "POST" });
    refreshLogos();
}

const b64 = bytes => btoa(String.fromCharCode(...bytes));
const unb64 = s => Uint8Array.from(atob(s), c => c.charCodeAt(0));

// Profiles and logos live in LittleFS: a filesystem update erases them. Read them first (profiles
// as the API gives them: without their own secrets) and write them back after the restart.
async function collectKeep() {
    const keep = { profiles: {}, logos: {} };
    for (const name of await fetchProfilesList()) {
        const res = await fetch(`/api/profiles/get?name=${encodeURIComponent(name)}`);
        if (res.ok) keep.profiles[name] = stripTails(await res.json());
    }
    for (const side of ["left", "right"]) {
        const res = await fetch(`/api/logos/get?side=${side}`);
        if (res.ok) keep.logos[side] = b64(new Uint8Array(await res.arrayBuffer()));
    }
    return keep;
}

async function restoreKeep(keep) {
    const failed = [];
    for (const [name, cfg] of Object.entries(keep.profiles || {})) {
        if (!PROFILE_NAME_RE.test(name)) continue;
        const res = await postProfile(name, cfg, true);
        if (!res.ok) failed.push(name);
    }
    for (const [side, data] of Object.entries(keep.logos || {})) {
        if (side !== "left" && side !== "right") continue;
        try { await postLogo(side, unb64(data)); } catch (e) { failed.push(`${side} logo`); }
    }
    return failed;
}

function offerKeepDownload(keep) {
    const kept = { format: "qrpickle-backup", version: 1, created: new Date().toISOString(), settings: null, ...keep };
    const a = document.createElement("a");
    a.href = URL.createObjectURL(new Blob([JSON.stringify(kept, null, 2)], { type: "application/json" }));
    a.download = `qrpickle-kept-${new Date().toISOString().slice(0, 10)}.json`;
    a.click();
    setTimeout(() => URL.revokeObjectURL(a.href), 1000);
}

async function waitForDevice(maxMs = 120000) {
    const end = Date.now() + maxMs;
    await new Promise(r => setTimeout(r, 15000));  // the device restarts after the upload
    while (Date.now() < end) {
        try { if ((await fetch("/api/status", { cache: "no-store" })).ok) return true; } catch (e) { /* not up yet */ }
        await new Promise(r => setTimeout(r, 3000));
    }
    return false;
}

// Backup: settings + all profiles as the device reports them (secrets masked, review 1.3).
async function downloadBackup() {
    try {
        const settings = stripTails(await (await fetch("/api/config")).json());
        for (const k of ["fw_name", "fw_version", "author_call"]) delete settings[k];
        const profiles = {};
        for (const name of await fetchProfilesList()) {
            const res = await fetch(`/api/profiles/get?name=${encodeURIComponent(name)}`);
            if (res.ok) profiles[name] = stripTails(await res.json());
        }
        const logos = {};
        for (const side of ["left", "right"]) {
            const res = await fetch(`/api/logos/get?side=${side}`);
            if (res.ok) logos[side] = b64(new Uint8Array(await res.arrayBuffer()));
        }
        const backup = { format: "qrpickle-backup", version: 1, created: new Date().toISOString(), settings, profiles, logos };
        const a = document.createElement("a");
        a.href = URL.createObjectURL(new Blob([JSON.stringify(backup, null, 2)], { type: "application/json" }));
        a.download = `qrpickle-backup-${new Date().toISOString().slice(0, 10)}.json`;
        a.click();
        setTimeout(() => URL.revokeObjectURL(a.href), 1000);
    } catch (err) {
        alert("Backup failed: " + err);
    }
}

async function restoreBackup(input) {
    const file = input.files[0];
    input.value = "";
    if (!file) return;
    let backup;
    try {
        backup = JSON.parse(await file.text());
    } catch (err) {
        alert("This file is not valid JSON."); return;
    }
    // settings: null = a file kept during a filesystem update (profiles and logos only).
    if (backup.format !== "qrpickle-backup" || backup.settings === undefined || typeof backup.profiles !== "object") {
        alert("This is not a QRPickle backup file."); return;
    }
    const names = Object.keys(backup.profiles).filter(n => PROFILE_NAME_RE.test(n));
    const logoSides = Object.keys(backup.logos || {}).filter(s => s === "left" || s === "right");
    const what = [backup.settings ? "the device settings" : null, `${names.length} profile(s)`,
                  logoSides.length ? `${logoSides.length} splash logo(s)` : null].filter(Boolean).join(", ");
    if (!confirm(`Restore ${what} from this backup? Passwords and keys stored on the device are kept.`)) return;
    const failed = await restoreKeep({ profiles: backup.profiles, logos: backup.logos || {} });
    if (backup.settings) {
        const res = await fetch("/api/config/save", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(backup.settings)
        });
        if (!res.ok) failed.push("device settings");
    }
    alert(failed.length ? "Restore finished with errors: " + failed.join(", ") : "Backup restored.");
    window.location.reload();
}

function fetchCloudOTADetails(forceCheck = false) {
    if (forceCheck) {
        document.getElementById("ota-remote-ver").innerText = "Checking...";
        document.getElementById("ota-release-notes").innerText = "Pinging GitHub API...";
    }

    const url = forceCheck ? "/api/cloud_ota/check?force=true" : "/api/cloud_ota/check";

    fetch(url)
    .then(res => res.json())
    .then(data => {
        document.getElementById("ota-local-ver").innerText = data.local_ver;
        document.getElementById("ota-remote-ver").innerText = data.latest_ver;
        document.getElementById("ota-release-notes").innerText = data.notes;

        const btn = document.getElementById("btn-cloud-flash");
        if (data.available) {
            btn.style.display = "block";
        } else {
            btn.style.display = "none";
            document.getElementById("ota-release-notes").innerText = "You are currently running the latest firmware version.";
        }
    })
    .catch(err => console.warn("Could not reach OTA endpoint:", err));
}

function triggerCloudFlash() {
    if(!confirm("WARNING: This will halt all active background telemetry while the flash memory is rewritten. Proceed?")) return;

    document.getElementById("ota-release-notes").innerText = "Streaming firmware directly from GitHub servers... DO NOT TURN OFF.";
    document.getElementById("btn-cloud-flash").style.display = "none";

    fetch("/api/cloud_ota/flash", { method: "POST" })
    .then(() => alert("Cloud update initiated. Device will reboot automatically upon completion."))
    .catch(err => alert("Transmission failed."));
}

// --- AUTO GRID SQUARE CALCULATOR ---
function calcGridSquare(lat, lon) {
    if (isNaN(lat) || isNaN(lon)) return "";

    lon += 180;
    lat += 90;

    const A = String.fromCharCode(65 + Math.floor(lon / 20));
    const B = String.fromCharCode(65 + Math.floor(lat / 10));

    lon = lon % 20;
    lat = lat % 10;

    const C = Math.floor(lon / 2).toString();
    const D = Math.floor(lat / 1).toString();

    lon = (lon % 2) * 60;
    lat = (lat % 1) * 60;

    const E = String.fromCharCode(97 + Math.floor(lon / 5)); // lowercase
    const F = String.fromCharCode(97 + Math.floor(lat / 2.5)); // lowercase

    return `${A}${B}${C}${D}${E}${F}`;
}

function attachGridAutoCalc(latId, lonId, gridId) {
    const latEl = document.getElementById(latId);
    const lonEl = document.getElementById(lonId);
    const gridEl = document.getElementById(gridId);

    if (!latEl || !lonEl || !gridEl) return;

    const updateGrid = () => {
        const lat = parseFloat(latEl.value);
        const lon = parseFloat(lonEl.value);
        const grid = calcGridSquare(lat, lon);
        if (grid) gridEl.value = grid;
    };

        latEl.addEventListener("input", updateGrid);
        lonEl.addEventListener("input", updateGrid);
}

// Band conditions use lat/lon only when ticked; typing one ticks it. Warn when lat/lon lie outside
// the grid square (the grid can also be edited on its own).
function checkLatLonGrid() {
    const warn = document.getElementById("latlon-grid-warn");
    if (!warn) return;
    const grid = (getElementValue("cfg-grid") || "").trim();
    const calc = calcGridSquare(parseFloat(getElementValue("cfg-lat")), parseFloat(getElementValue("cfg-lon")));
    const n = grid.length >= 6 ? 6 : 4;
    const inside = !calc || grid.length < 4 || calc.slice(0, n).toUpperCase() === grid.slice(0, n).toUpperCase();
    warn.textContent = inside ? "" : `Latitude/longitude lie outside grid square ${grid}.`;
}
document.addEventListener("DOMContentLoaded", () => {
    for (const id of ["cfg-lat", "cfg-lon"]) {
        const el = document.getElementById(id);
        if (el) el.addEventListener("input", () => { setElementValue("cfg-latlon-set", true, true); checkLatLonGrid(); });
    }
    const gridEl = document.getElementById("cfg-grid");
    if (gridEl) gridEl.addEventListener("input", checkLatLonGrid);
});

// Attach the auto-calculators after the DOM loads
document.addEventListener("DOMContentLoaded", () => {
    attachGridAutoCalc("cfg-lat", "cfg-lon", "cfg-grid");           // Basic Settings Tab
});

// --- APRS MESSAGING WEB CLIENT ---
function sendAprsMessage() {
    const target = getElementValue("aprs-msg-target").trim();
    const body = getElementValue("aprs-msg-body").trim();
    const statusBox = document.getElementById("aprs-msg-status");

    if (!target || !body) {
        alert("Both Target Callsign and Message Payload are required.");
        return;
    }

    statusBox.innerText = `Queuing message to ${target} via TCP/IP...`;

    fetch("/api/aprs/send", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ target: target, message: body })
    })
    .then(res => res.json())
    .then(data => {
        if (data.status === "success") {
            statusBox.innerText = `[SUCCESS] Payload injected into TX queue for ${target}:\n${body}`;
            document.getElementById("aprs-msg-body").value = ""; // Clear text body on success
        } else {
            statusBox.innerText = `[FAILED] ${data.error || "Unknown backend error."}`;
        }
    })
    .catch(err => {
        statusBox.innerText = `[NETWORK ERROR] Could not reach device: ${err}`;
    });
}

function fetchAprsMessages() {
    const tab = document.getElementById("tab-aprsmsg");
    if (!tab || !tab.classList.contains("active")) return;

    fetch("/api/aprs/messages")
    .then(res => res.json())
    .then(data => {
        const inbox = document.getElementById("aprs-msg-inbox");
        if (data.length === 0) {
            inbox.innerHTML = '<span style="color: var(--text-muted)">Inbox empty. Listening for incoming messages...</span>';
            return;
        }

        let html = "";
        data.forEach(msg => {
            html += `<div style="padding: 6px; border-bottom: 1px solid var(--border); margin-bottom: 4px;">
            <strong style="color: var(--accent);">${esc(msg.from)}</strong>: ${esc(msg.text)}
            </div>`;
        });
        inbox.innerHTML = html;
    })
    .catch(err => console.warn("Failed to fetch APRS messages", err));
}
