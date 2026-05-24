document.addEventListener("DOMContentLoaded", () => {
    initTabsEngine();
    loadCurrentConfig();
    
    // Core Background Telemetry Thread Poller Loop
    setInterval(fetchSystemTelemetry, 3000);
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
            
            if (targetTab === "system") fetchSystemTelemetry();
            if (targetTab === "about") fetchAboutDetails();
        });
    });
}

// --- SYNC CONFIGURATION READ METHOD ---
function loadCurrentConfig() {
    fetch("/api/config")
        .then(res => res.json())
        .then(data => {
            // Basic Panel JSON parameters sync
            setElementValue("cfg-callsign", data.callsign || "");
            setElementValue("cfg-grid", data.grid || "");
            setElementValue("cfg-ssid", data.ssid || "");
            setElementValue("cfg-password", ""); 
            setElementValue("cfg-lat", data.lat ?? 12.97);
            setElementValue("cfg-lon", data.lon ?? 77.59);
            setElementValue("cfg-offset", (data.offset ?? 5.5) * 2.0 / 2.0); 
            setElementValue("cfg-brightness", data.brightness ?? 180);
            setElementValue("cfg-timeout", data.timeout ?? 5);
            setElementValue("cfg-theme", data.theme_id ?? 0);

            // Advanced Panel JSON parameters sync
            setElementValue("cfg-apikey", data.apikey || "");
            setElementValue("cfg-hamalert-pass", data.hamalert_pass || "");
            setElementValue("cfg-aprs-en", data.aprs_en ? "1" : "0");
            setElementValue("cfg-aprs-pass", data.aprs_pass || "");
            setElementValue("cfg-aprs-ssid", data.aprs_ssid ?? 0);
            setElementValue("cfg-aprs-icon", data.aprs_icon || "/[");
            setElementValue("cfg-aprs-cmt", data.aprs_cmt || "");

            // Decode Bitmask integers for Weather slots configuration checkboxes
            const mask = data.fc_slots ?? 15;
            for (let i = 0; i < 8; i++) {
                setElementValue(`fc-bit${i}`, (mask & (1 << i)) !== 0, true);
            }

            // Extract values for dynamic C-String Macro arrays
            if (data.aprs_macros && Array.isArray(data.aprs_macros)) {
                for (let i = 0; i < 5; i++) {
                    setElementValue(`cfg-mac${i}`, data.aprs_macros[i] || "");
                }
            }

            // DX Node endpoints definitions
            setElementValue("dx_url_p", data.dx_url_p || "");
            setElementValue("dx_port_p", data.dx_port_p ?? 7300);
            setElementValue("dx_url_s", data.dx_url_s || "");
            setElementValue("dx_port_s", data.dx_port_s ?? 7373);

            // Re-index stored files dropdown elements
            fetchProfilesList();
        })
        .catch(err => console.error("Could not fetch device configuration variables:", err));
}

// --- SYNC CONFIGURATION WRITE METHOD ---
function saveActiveConfig() {
    let forecastMask = 0;
    for (let i = 0; i < 8; i++) {
        if (getElementValue(`fc-bit${i}`, true)) forecastMask |= (1 << i);
    }

    const macrosArray = [];
    for (let i = 0; i < 5; i++) {
        macrosArray.push(getElementValue(`cfg-mac${i}`));
    }

    const payload = {
        callsign: getElementValue("cfg-callsign"),
        grid: getElementValue("cfg-grid"),
        ssid: getElementValue("cfg-ssid"),
        apikey: getElementValue("cfg-apikey"),
        lat: parseFloat(getElementValue("cfg-lat")),
        lon: parseFloat(getElementValue("cfg-lon")),
        offset: parseFloat(getElementValue("cfg-offset")),
        brightness: parseInt(getElementValue("cfg-brightness")),
        theme_id: parseInt(getElementValue("cfg-theme")),
        timeout: parseInt(getElementValue("cfg-timeout")),
        fc_slots: forecastMask,
        dx_url_p: getElementValue("dx_url_p"),
        dx_port_p: parseInt(getElementValue("dx_port_p")),
        dx_url_s: getElementValue("dx_url_s"),
        dx_port_s: parseInt(getElementValue("dx_port_s")),
        aprs_en: getElementValue("cfg-aprs-en") === "1",
        aprs_pass: getElementValue("cfg-aprs-pass"),
        aprs_ssid: parseInt(getElementValue("cfg-aprs-ssid")),
        aprs_icon: getElementValue("cfg-aprs-icon"),
        aprs_cmt: getElementValue("cfg-aprs-cmt"),
        aprs_macros: macrosArray,
        hamalert_pass: getElementValue("cfg-hamalert-pass")
    };

    const passValue = getElementValue("cfg-password");
    if (passValue) payload.password = passValue;

    fetch("/api/config/save", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload)
    })
    .then(res => res.ok ? alert("Configuration saved successfully to NVS memory!") : alert("Server rejected update payload."))
    .then(() => loadCurrentConfig())
    .catch(err => alert("Transmission line error saving configuration parameters: " + err));
}

// --- TELEMETRY READOUT AGENT ---
function fetchSystemTelemetry() {
    const sysTab = document.getElementById("tab-system");
    if (!sysTab || !sysTab.classList.contains("active")) return;

    fetch("/api/status") // FIXED: Routes to the correct status endpoint matching web_server.cpp
        .then(res => res.json())
        .then(data => {
            document.getElementById("sys-uptime").innerText = (data.uptime || 0) + "s";
            document.getElementById("sys-heap").innerText = (data.heap || 0).toLocaleString() + " B"; // FIXED fields mapping keys
            document.getElementById("sys-rssi").innerText = (data.rssi || 0) + " dBm";
            document.getElementById("sys-temp").innerText = (data.temp ?? "--.-") + " °C";
            document.getElementById("sys-humidity").innerText = (data.humidity ?? "--.-") + " %";
            document.getElementById("sys-pressure").innerText = (data.pressure ?? "----") + " hPa";

            document.getElementById("sys-lib-lvgl").innerText = data.ver_lvgl || "v8.3.x";
            document.getElementById("sys-lib-json").innerText = data.ver_json || "v7.x";
            document.getElementById("sys-lib-core").innerText = data.ver_core || "v6.x";
            document.getElementById("sys-lib-idf").innerText = data.ver_idf || "v4.4.x";
        })
        .catch(err => console.warn("Polling dropped telemetry. Re-syncing line link...", err));
}

function fetchAboutDetails() {
    fetch("/api/about")
        .then(res => res.ok ? res.text() : "No details recorded on disk flash.")
        .then(text => {
            document.getElementById("about-bin").innerText = text;
            
            // Mirror background info elements to footer tags safely
            document.getElementById("foot-name").innerText = "QRPickle";
            document.getElementById("foot-version").innerText = "v1.0.0";
            const link = document.getElementById("foot-link");
            if (link) { link.innerText = "Operator Console Portal"; link.href = "https://ham.bharathpalavalli.com/"; }
        });
}

// --- FAIL-SAFE OTA MAINTENANCE TRANSFERS ---
function executeWirelessOTA() {
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
    statusLabel.innerText = "Uploading payload packages to file server...";

    const xhr = new XMLHttpRequest();
    // FIXED: Formats query parameters strictly expected by the backend update hook architecture
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
function fetchProfilesList() {
    fetch("/api/profiles") // FIXED: Maps directly to the profile array extraction handler endpoint
        .then(res => res.json())
        .then(data => {
            const dropdown = document.getElementById("profile-select");
            if (!dropdown) return;
            dropdown.innerHTML = '<option value="">-- No Profile Selected --</option>';
            if (Array.isArray(data)) {
                data.forEach(pName => {
                    dropdown.innerHTML += `<option value="${pName}">${pName}</option>`;
                });
            }
        });
}

function handleProfileSelectionChange() {
    const pName = document.getElementById("profile-select").value;
    const inspectPanel = document.getElementById("profile-inspect-panel");
    
    if (!pName) {
        if (inspectPanel) inspectPanel.classList.add("hidden");
        return;
    }

    fetch(`/api/profiles/get?name=${encodeURIComponent(pName)}`)
        .then(res => res.json())
        .then(data => {
            if (inspectPanel) inspectPanel.classList.remove("hidden");
            setElementValue("prof-edit-callsign", data.callsign || "");
            setElementValue("prof-edit-grid", data.grid || "");
            setElementValue("prof-edit-ssid", data.ssid || "");
            setElementValue("prof-edit-password", data.password || "");
            setElementValue("prof-edit-offset", data.offset ?? 0);
            setElementValue("prof-edit-brightness", data.brightness ?? 180);
            setElementValue("prof-edit-theme", data.theme_id ?? 0);
            setElementValue("prof-edit-aprs-en", data.aprs_en ? "1" : "0");
            setElementValue("prof-edit-aprs-ssid", data.aprs_ssid ?? 0);
            setElementValue("prof-edit-aprs-icn", data.aprs_icn || "/[");
            setElementValue("prof-edit-timeout", data.timeout ?? 5); // Fallback evaluation alignment helper
            setElementValue("prof-edit-aprs-cmt", data.aprs_cmt || "");
        });
}

function saveProfile() {
    const pName = document.getElementById("new-profile-name").value.trim();
    if (!pName) { alert("Please specify a filename identification moniker for the layout template configuration."); return; }
    
    // Packages local state profiles using layout mappings expected by save_profile_from_json
    const configPayload = {
        callsign: getElementValue("cfg-callsign"),
        grid: getElementValue("cfg-grid"),
        ssid: getElementValue("cfg-ssid"),
        password: getElementValue("cfg-password") || "unset",
        apikey: getElementValue("cfg-apikey"),
        lat: parseFloat(getElementValue("cfg-lat")),
        lon: parseFloat(getElementValue("cfg-lon")),
        offset: parseFloat(getElementValue("cfg-offset")),
        brightness: parseInt(getElementValue("cfg-brightness")),
        theme_id: parseInt(getElementValue("cfg-theme")),
        scr_to: parseInt(getElementValue("cfg-timeout")), // Maps to p_data structure bounds expectations
        aprs_en: getElementValue("cfg-aprs-en") === "1",
        aprs_ssid: parseInt(getElementValue("cfg-aprs-ssid")),
        aprs_pass: getElementValue("cfg-aprs-pass"),
        aprs_cmt: getElementValue("cfg-aprs-cmt"),
        aprs_icn: getElementValue("cfg-aprs-icon")
    };

    const containerPayload = { name: pName, config: configPayload };

    fetch("/api/profiles/save", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(containerPayload)
    })
    .then(res => res.ok ? alert("System operational snapshot profile successfully written to disk flash memory layout.") : alert("Server rejected the profile snapshot operation."))
    .then(() => {
        document.getElementById("new-profile-name").value = "";
        fetchProfilesList();
    })
    .catch(err => alert("Error processing background snapshot file sync array: " + err));
}

function applySelectedProfile() {
    const pName = document.getElementById("profile-select").value;
    if (!pName) return;
    
    fetch(`/api/profiles/load?name=${encodeURIComponent(pName)}`, { method: "POST" })
        .then(res => res.ok ? alert("Profile configuration applied live! Re-synchronizing console view parameters...") : alert("Profile transition failure."))
        .then(() => window.location.reload());
}

function saveProfileChanges() {
    const pName = document.getElementById("profile-select").value;
    if (!pName) return;

    // Compiles mutations into the expected structure format required by the backend
    const configPayload = {
        callsign: getElementValue("prof-edit-callsign"),
        grid: getElementValue("prof-edit-grid"),
        ssid: getElementValue("prof-edit-ssid"),
        password: getElementValue("prof-edit-password"),
        apikey: getElementValue("prof-edit-apikey"),
        lat: parseFloat(getElementValue("prof-edit-lat")),
        lon: parseFloat(getElementValue("prof-edit-lon")),
        offset: parseFloat(getElementValue("prof-edit-offset")),
        brightness: parseInt(getElementValue("prof-edit-brightness")),
        theme_id: parseInt(getElementValue("prof-edit-theme")),
        scr_to: parseInt(getElementValue("prof-edit-timeout")), 
        aprs_en: getElementValue("prof-edit-aprs-en") === "1",
        aprs_ssid: parseInt(getElementValue("prof-edit-aprs-ssid")),
        aprs_pass: getElementValue("prof-edit-aprs-pass"),
        aprs_cmt: getElementValue("prof-edit-aprs-cmt"),
        aprs_icn: getElementValue("prof-edit-aprs-icn")
    };

    const containerPayload = { name: pName, config: configPayload };

    fetch("/api/profiles/save", { // Standardizes profile mutations across unified backend destination route
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(containerPayload)
    })
    .then(res => res.ok ? alert("Profile changes committed to storage disk file successfully!") : alert("Modifications transaction rejected by target hardware."))
    .catch(err => alert("Error sending profile save changes configuration: " + err));
}