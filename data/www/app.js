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

// Helper to extract and mirror tool descriptors dynamically to the footer layout
function updateDynamicFooter(data) {
    const nameEl = document.getElementById("foot-name");
    const verEl = document.getElementById("foot-version");
    const authcallEl = document.getElementById("foot-authorcall");
    
    if (nameEl && (data.app_name || data.tool_name)) {
        nameEl.innerText = data.app_name || data.tool_name;
    }
    if (verEl && (data.version || data.fw_version)) {
        verEl.innerText = data.version || data.fw_version;
    }

    if (authcallEl && (data.version || data.author_call)) {
        verEl.innerText = data.author_call || data.support_email;
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
            
            if (targetTab === "system") fetchSystemTelemetry();
            if (targetTab === "about") fetchAboutDetails();
            if (targetTab === "cloudota") fetchCloudOTADetails();
        });
    });
}

// --- SYNC CONFIGURATION READ METHOD ---
function loadCurrentConfig() {
    fetch("/api/config")
        .then(res => res.json())
        .then(data => {
            // Update tool matching metadata descriptors if supplied by backend
            updateDynamicFooter(data);

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

            // FIXED: Fallback parser chains map explicit openweather keys strictly matching C++ fields
            const extractedApiKey = data.openweather_api_key || data.owm_api_key || data.apikey || data.api_key || "";
            setElementValue("cfg-apikey", extractedApiKey);
            
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

    const activeApiKey = getElementValue("cfg-apikey");

    const payload = {
        callsign: getElementValue("cfg-callsign"),
        grid: getElementValue("cfg-grid"),
        ssid: getElementValue("cfg-ssid"),
        
        // FIXED: Redundant data descriptors prevent serialization dropouts on backend C++ fields parsing
        apikey: activeApiKey,
        openweather_api_key: activeApiKey,
        owm_api_key: activeApiKey,
        
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
    fetch("/api/profiles") 
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
            setElementValue("prof-edit-timeout", data.timeout ?? 5); 
            setElementValue("prof-edit-aprs-cmt", data.aprs_cmt || "");
            
            // FIXED: Populate the separate profile key edit field securely
            const profileKey = data.openweather_api_key || data.owm_api_key || data.apikey || data.api_key || "";
            setElementValue("prof-edit-apikey", profileKey);
        });
}

function saveProfile() {
    const pName = document.getElementById("new-profile-name").value.trim();
    if (!pName) { alert("Please specify a filename identification moniker for the layout template configuration."); return; }
    
    const activeApiKey = getElementValue("cfg-apikey");

    const configPayload = {
        callsign: getElementValue("cfg-callsign"),
        grid: getElementValue("cfg-grid"),
        ssid: getElementValue("cfg-ssid"),
        password: getElementValue("cfg-password") || "unset",
        
        // FIXED: Unified profile mapping variations
        apikey: activeApiKey,
        openweather_api_key: activeApiKey,
        owm_api_key: activeApiKey,
        
        lat: parseFloat(getElementValue("cfg-lat")),
        lon: parseFloat(getElementValue("cfg-lon")),
        offset: parseFloat(getElementValue("cfg-offset")),
        brightness: parseInt(getElementValue("cfg-brightness")),
        theme_id: parseInt(getElementValue("cfg-theme")),
        scr_to: parseInt(getElementValue("cfg-timeout")), 
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
    .then(res => res.ok ? alert("System operational snapshot profile successfully written to disk flash.") : alert("Server rejected profile snapshot operation."))
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

    const modifiedProfileKey = getElementValue("prof-edit-apikey");

    const configPayload = {
        callsign: getElementValue("prof-edit-callsign"),
        grid: getElementValue("prof-edit-grid"),
        ssid: getElementValue("prof-edit-ssid"),
        password: getElementValue("prof-edit-password"),
        
        // FIXED: Sync profile changes strictly matching layout variants
        apikey: modifiedProfileKey,
        openweather_api_key: modifiedProfileKey,
        owm_api_key: modifiedProfileKey,
        
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

    fetch("/api/profiles/save", { 
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(containerPayload)
    })
    .then(res => res.ok ? alert("Profile changes committed to storage disk file successfully!") : alert("Modifications transaction rejected by target hardware."))
    .catch(err => alert("Error sending profile save changes configuration: " + err));
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