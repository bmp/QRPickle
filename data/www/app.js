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

function togglePassVisibility(id) {
    const el = document.getElementById(id);
    el.type = el.type === "password" ? "text" : "password";
}

function loadCurrentConfig() {
    fetch('/api/config')
    .then(res => res.json())
    .then(data => {
        document.getElementById('cfg-callsign').value = data.callsign;
        document.getElementById('cfg-grid').value = data.grid;
        document.getElementById('cfg-ssid').value = data.ssid;
        document.getElementById('cfg-password').value = data.password;
        document.getElementById('cfg-offset').value = data.offset;
        document.getElementById('cfg-brightness').value = data.brightness;
        document.getElementById('cfg-theme').value = data.theme_id;
    }).catch(err => console.error("Config read block fail:", err));
    refreshProfilesList();
}

function saveActiveConfig() {
    const payload = {
        callsign: document.getElementById('cfg-callsign').value,
        grid: document.getElementById('cfg-grid').value,
        ssid: document.getElementById('cfg-ssid').value,
        password: document.getElementById('cfg-password').value,
        offset: parseFloat(document.getElementById('cfg-offset').value),
        brightness: parseInt(document.getElementById('cfg-brightness').value),
        theme_id: parseInt(document.getElementById('cfg-theme').value)
    };

    fetch('/api/config/save', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(payload)
    })
    .then(res => res.json())
    .then(data => {
        if (data.status === 'success') {
            alert('Active layout configuration updated on device instantly!');
        }
    }).catch(err => alert("Transmission timeout syncing config vectors."));
}

function refreshProfilesList() {
    fetch('/api/profiles')
    .then(res => res.json())
    .then(list => {
        const bin = document.getElementById('profile-bin');
        bin.innerHTML = '';
        if (list.length === 0) {
            bin.innerHTML = '<span style="font-size:12px; color:var(--text-muted);">No profiles saved on flash partition layout.</span>';
            return;
        }
        list.forEach(p => {
            const chip = document.createElement('div');
            chip.className = 'profile-chip';
            chip.innerHTML = `📦 ${p.replace(/_/g, ' ')}`;
            chip.addEventListener('click', () => loadProfile(p));
            bin.appendChild(chip);
        });
    });
}

// FIXED: Packages web form fields into JSON payload to write straight to disk without applying live changes
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
            offset: parseFloat(document.getElementById('cfg-offset').value),
            brightness: parseInt(document.getElementById('cfg-brightness').value),
            theme_id: parseInt(document.getElementById('cfg-theme').value)
        }
    };

    fetch('/api/profiles/save', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(payload)
    })
    .then(res => res.json())
    .then(data => {
        nameInput.value = '';
        refreshProfilesList();
    });
}

function loadProfile(name) {
    if (confirm(`Inject and execute deployment profile "${name.replace(/_/g, ' ')}"? Hardware will save and trigger a system power cycle.`)) {
        fetch(`/api/profiles/load?name=${encodeURIComponent(name)}`, { method: 'POST' })
        .then(res => res.json())
        .then(data => {
            alert('Profile committed safely! Connection terminal severed while device reboots execution context.');
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
    if (field.type === "password") {
        field.type = "text";
    } else {
        field.type = "password";
    }
}

function loadCurrentConfig() {
    fetch('/api/config')
    .then(res => res.json())
    .then(data => {
        document.getElementById('cfg-callsign').value = data.callsign;
        document.getElementById('cfg-grid').value = data.grid;
        document.getElementById('cfg-ssid').value = data.ssid;
        document.getElementById('cfg-password').value = data.password;
        document.getElementById('cfg-apikey').value = data.apikey || ""; // FIXED: Loads OWM Key
        document.getElementById('cfg-lat').value = data.lat || 0;
        document.getElementById('cfg-lon').value = data.lon || 0;
        document.getElementById('cfg-offset').value = data.offset;
        document.getElementById('cfg-brightness').value = data.brightness;
        document.getElementById('cfg-theme').value = data.theme_id;
    }).catch(err => console.error("Config read block fail:", err));
    refreshProfilesList();
}

function saveActiveConfig() {
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
        theme_id: parseInt(document.getElementById('cfg-theme').value)
    };

    fetch('/api/config/save', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(payload)
    })
    .then(res => res.json())
    .then(data => {
        if (data.status === 'success') {
            alert('Active layout configuration updated on device instantly!');
        }
    }).catch(err => alert("Transmission timeout syncing config vectors."));
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
            apikey: document.getElementById('cfg-apikey').value, // FIXED: Bundles token cleanly into saved profile JSON string files
            lat: parseFloat(document.getElementById('cfg-lat').value),
            lon: parseFloat(document.getElementById('cfg-lon').value),
            offset: parseFloat(document.getElementById('cfg-offset').value),
            brightness: parseInt(document.getElementById('cfg-brightness').value),
            theme_id: parseInt(document.getElementById('cfg-theme').value)
        }
    };

    fetch('/api/profiles/save', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(payload)
    })
    .then(res => res.json())
    .then(data => {
        nameInput.value = '';
        refreshProfilesList();
    });
}
