document.addEventListener("DOMContentLoaded", () => {
    initTabs();
    loadCurrentConfig();
    runTelemetryPoller();
    setInterval(runTelemetryPoller, 2000);
});

function initTabs() {
    document.querySelectorAll(".tab-btn").forEach(btn => {
        btn.addEventListener("click", (e) => {
            const targetTab = e.target.getAttribute("data-tab");
            document.querySelectorAll(".tab-btn").forEach(b => b.classList.remove("active"));
            document.querySelectorAll(".tab-content").forEach(c => c.classList.remove("active"));
            e.target.classList.add("active");
            document.getElementById("tab-" + targetTab).classList.add("active");
            if (targetTab === "about") fetchAboutText();
        });
    });
}

function loadCurrentConfig() {
    fetch('/api/config')
    .then(res => res.json())
    .then(data => {
        document.getElementById('cfg-callsign').value = data.callsign;
        document.getElementById('cfg-grid').value = data.grid;
        document.getElementById('cfg-ssid').value = data.ssid;
        document.getElementById('cfg-password').value = data.password;
        document.getElementById('cfg-apikey').value = data.apikey || "";
        document.getElementById('cfg-lat').value = data.lat || 0;
        document.getElementById('cfg-lon').value = data.lon || 0;
        document.getElementById('cfg-offset').value = data.offset;
        document.getElementById('cfg-brightness').value = data.brightness;
        document.getElementById('cfg-timeout').value = data.timeout; 
        document.getElementById('cfg-theme').value = data.theme_id;
        
        document.getElementById('dx_url_p').value = data.dx_url_p || "";
        document.getElementById('dx_port_p').value = data.dx_port_p || "";
        document.getElementById('dx_url_s').value = data.dx_url_s || "";
        document.getElementById('dx_port_s').value = data.dx_port_s || "";

        // Bind APRS Variables
        document.getElementById('cfg-aprs-en').value = (data.aprs_en === true || data.aprs_en === 1) ? "1" : "0";
        document.getElementById('cfg-aprs-pass').value = data.aprs_pass || "";
        document.getElementById('cfg-aprs-ssid').value = data.aprs_ssid || 0;

        const mask = data.fc_slots || 0x0F;
        for (let i = 0; i < 8; i++) {
            document.getElementById(`fc-bit${i}`).checked = (mask & (1 << i)) !== 0;
        }
    }).catch(err => console.error("Config read fail:", err));
    refreshProfilesDropdown();
}

function saveActiveConfig() {
    let fcMask = 0;
    for (let i = 0; i < 8; i++) {
        if (document.getElementById(`fc-bit${i}`).checked) {
            fcMask |= (1 << i);
        }
    }
    
    const payload = {
        callsign: document.getElementById('cfg-callsign').value,
        grid: document.getElementById('cfg-grid').value,
        ssid: document.getElementById('cfg-ssid').value,
        password: document.getElementById('cfg-password').value,
        apikey: document.getElementById('cfg-apikey').value,
        lat: parseFloat(document.getElementById('cfg-lat').value),
        lon: parseFloat(document.getElementById('cfg-lon').value),
        offset: parseFloat(document.getElementById('cfg-offset').value),
        brightness: parseInt(document.getElementById('cfg-brightness').value),
        timeout: parseInt(document.getElementById('cfg-timeout').value), 
        fc_slots: fcMask,
        theme_id: parseInt(document.getElementById('cfg-theme').value),
        dx_url_p: document.getElementById('dx_url_p').value,
        dx_port_p: parseInt(document.getElementById('dx_port_p').value) || 7373,
        dx_url_s: document.getElementById('dx_url_s').value,
        dx_port_s: parseInt(document.getElementById('dx_port_s').value) || 7373,
        
        // Pack APRS Variables into JSON
        aprs_en: parseInt(document.getElementById('cfg-aprs-en').value) === 1,
        aprs_pass: document.getElementById('cfg-aprs-pass').value,
        aprs_ssid: parseInt(document.getElementById('cfg-aprs-ssid').value) || 0
    };

    fetch('/api/config/save', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(payload)
    })
    .then(res => res.json())
    .then(data => {
        if (data.status === 'success') alert('Configuration hot-swapped onto device instantly!');
    }).catch(() => alert("Timeout syncing layout config."));
}

function refreshProfilesDropdown() {
    fetch('/api/profiles')
    .then(res => res.json())
    .then(list => {
        const select = document.getElementById('profile-select');
        select.innerHTML = '<option value="">-- Choose a Profile --</option>';
        list.forEach(p => {
            const opt = document.createElement('option');
            opt.value = p;
            opt.textContent = p.replace(/_/g, ' ');
            select.appendChild(opt);
        });
        document.getElementById('profile-inspect-panel').classList.add('hidden');
    });
}

function handleProfileSelectionChange() {
    const select = document.getElementById('profile-select');
    const panel = document.getElementById('profile-inspect-panel');
    if (!select.value) {
        panel.classList.add('hidden');
        return;
    }

    fetch(`/api/profiles/get?name=${encodeURIComponent(select.value)}`)
    .then(res => res.json())
    .then(data => {
        document.getElementById('prof-edit-callsign').value = data.callsign;
        document.getElementById('prof-edit-grid').value = data.grid;
        document.getElementById('prof-edit-ssid').value = data.ssid;
        document.getElementById('prof-edit-password').value = data.password;
        document.getElementById('prof-edit-offset').value = data.offset;
        document.getElementById('prof-edit-brightness').value = data.brightness;
        document.getElementById('prof-edit-timeout').value = data.timeout; 
        document.getElementById('prof-edit-theme').value = data.theme_id;
        
        document.getElementById('prof-edit-aprs-en').value = (data.aprs_en === true || data.aprs_en === 1) ? "1" : "0";
        document.getElementById('prof-edit-aprs-ssid').value = data.aprs_ssid || 0;
        
        panel.classList.remove('hidden');
    }).catch(() => alert("Error unrolling profile description."));
}

function saveProfileChanges() {
    const select = document.getElementById('profile-select');
    if (!select.value) return;

    const payload = {
        name: select.value,
        config: {
            callsign: document.getElementById('prof-edit-callsign').value,
            grid: document.getElementById('prof-edit-grid').value,
            ssid: document.getElementById('prof-edit-ssid').value,
            password: document.getElementById('prof-edit-password').value,
            offset: parseFloat(document.getElementById('prof-edit-offset').value),
            brightness: parseInt(document.getElementById('prof-edit-brightness').value),
            timeout: parseInt(document.getElementById('prof-edit-timeout').value), 
            theme_id: parseInt(document.getElementById('prof-edit-theme').value),
            aprs_en: parseInt(document.getElementById('prof-edit-aprs-en').value) === 1,
            aprs_ssid: parseInt(document.getElementById('prof-edit-aprs-ssid').value) || 0
        }
    };

    fetch('/api/profiles/save', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(payload)
    })
    .then(res => res.json())
    .then(() => {
        alert('Changes successfully saved to profile layout file.');
    }).catch(() => alert("Error writing updates to profile."));
}

function saveProfile() {
    const nameInput = document.getElementById('new-profile-name');
    if (!nameInput.value.trim()) return;

    const payload = {
        name: nameInput.value.trim(),
        config: {
            callsign: document.getElementById('cfg-callsign').value,
            grid: document.getElementById('cfg-grid').value,
            ssid: document.getElementById('cfg-ssid').value,
            password: document.getElementById('cfg-password').value,
            apikey: document.getElementById('cfg-apikey').value,
            lat: parseFloat(document.getElementById('cfg-lat').value),
            lon: parseFloat(document.getElementById('cfg-lon').value),
            offset: parseFloat(document.getElementById('cfg-offset').value),
            brightness: parseInt(document.getElementById('cfg-brightness').value),
            timeout: parseInt(document.getElementById('cfg-timeout').value), 
            theme_id: parseInt(document.getElementById('cfg-theme').value),
            aprs_en: parseInt(document.getElementById('cfg-aprs-en').value) === 1,
            aprs_pass: document.getElementById('cfg-aprs-pass').value,
            aprs_ssid: parseInt(document.getElementById('cfg-aprs-ssid').value) || 0
        }
    };

    fetch('/api/profiles/save', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(payload)
    })
    .then(res => res.json())
    .then(() => {
        nameInput.value = '';
        refreshProfilesDropdown();
        alert('Profile compilation committed to disk partition.');
    });
}

function applySelectedProfile() {
    const select = document.getElementById('profile-select');
    if (!select.value) return;

    if (confirm(`Inject operational deployment profile "${select.value.replace(/_/g, ' ')}"? Hardware layers will swap instantly.`)) {
        fetch(`/api/profiles/load?name=${encodeURIComponent(select.value)}`, { method: 'POST' })
        .then(res => res.json())
        .then(() => {
            alert('Profile parameters injected successfully! Device hardware layers updated hot-swapped without power cycle.');
            window.location.reload();
        });
    }
}

function runTelemetryPoller() {
    fetch('/api/status')
    .then(res => res.json())
    .then(d => {
        document.getElementById('sys-uptime').textContent = d.uptime + 's';
        document.getElementById('sys-heap').textContent = d.heap.toLocaleString() + ' B';
        document.getElementById('sys-rssi').textContent = d.rssi + ' dBm';
        document.getElementById('sys-temp').textContent = d.temp.toFixed(1) + ' °C';
        document.getElementById('sys-humidity').textContent = d.humidity.toFixed(1) + ' %';
        document.getElementById('sys-pressure').textContent = d.pressure.toFixed(1) + ' hPa';
        document.getElementById('sys-lib-lvgl').textContent = d.ver_lvgl;
        document.getElementById('sys-lib-json').textContent = d.ver_json;
        document.getElementById('sys-lib-core').textContent = d.ver_core;
        document.getElementById('sys-lib-idf').textContent = d.ver_idf;
        document.getElementById('foot-name').textContent = d.fw_name;
        document.getElementById('foot-version').textContent = d.fw_version;
        document.getElementById('foot-link').textContent = d.author_call;
    }).catch(() => {});
}

function fetchAboutText() {
    fetch('/api/about').then(res => res.text()).then(text => {
        document.getElementById('about-bin').textContent = text;
    });
}

function triggerReboot() {
    if (confirm('Issue absolute execution command override to force-reboot the device?')) {
        fetch('/api/system/reboot', { method: 'POST' });
        alert('Reboot execution command broadcasted.');
    }
}

function togglePass(id) {
    const field = document.getElementById(id);
    field.type = field.type === "password" ? "text" : "password";
}

function executeWirelessOTA() {
    const fileInput = document.getElementById('ota-file-input');
    const targetSelect = document.getElementById('ota-target');
    const progressContainer = document.getElementById('ota-progress-container');
    const progressBar = document.getElementById('ota-progress-bar');
    const pctLabel = document.getElementById('ota-pct-lbl');
    const statusLabel = document.getElementById('ota-status-lbl');

    if (fileInput.files.length === 0) {
        alert('Please select a compiled binary output artifact file first.');
        return;
    }

    const file = fileInput.files[0];
    const target = targetSelect.value;

    if (!confirm(`Are you sure you want to write "${file.name}" to device flash partition [${target.toUpperCase()}]? Connection will be severed.`)) {
        return;
    }

    const formData = new FormData();
    formData.append("update", file);

    const xhr = new XMLHttpRequest();
    
    xhr.upload.addEventListener("progress", (e) => {
        if (e.lengthComputable) {
            progressContainer.classList.remove('hidden');
            const percent = Math.round((e.loaded / e.total) * 100);
            progressBar.style.width = percent + '%';
            pctLabel.textContent = percent + '%';
            statusLabel.textContent = `Streaming bytes: ${Math.round(e.loaded/1024)} KB / ${Math.round(e.total/1024)} KB`;
        }
    });

    xhr.addEventListener("load", () => {
        if (xhr.status === 200) {
            statusLabel.textContent = "Flash verification secure! Rebooting target hardware...";
            progressBar.style.backgroundColor = "#3FB950";
            alert("Upgrade deployed successfully! The hardware terminal is now swapping boot vectors and rebooting.");
            setTimeout(() => { window.location.reload(); }, 3000);
        } else {
            progressBar.style.backgroundColor = "#FF3333";
            statusLabel.textContent = "Error: Partition rejected flash image payload.";
            try {
                const resp = JSON.parse(xhr.responseText);
                alert(`OTA Failure: ${resp.error || 'Signature rejected'}`);
            } catch {
                alert("OTA Error: Connection timed out or structure validation failure.");
            }
        }
    });

    xhr.addEventListener("error", () => {
        progressBar.style.backgroundColor = "#FF3333";
        statusLabel.textContent = "Network transaction interrupted.";
        alert("CRITICAL: Network terminal link dropped during transmission.");
    });

    xhr.open("POST", `/api/system/update?target=${target}`);
    xhr.send(formData);
}