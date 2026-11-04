// Unit test of web-installer/install-plan.js (the installer page's plan and manifest).
// Run: node test/js/install_plan_test.mjs
import assert from "node:assert/strict";
import { createRequire } from "node:module";
const require = createRequire(import.meta.url);
const { MODES, AREAS, planInstall, manifestFor } = require("../../web-installer/install-plan.js");

const PARTS = [  // as versions.json lists them (scripts/make_pages_site.py)
    { path: "bootloader.bin", offset: "0x1000" }, { path: "partitions.bin", offset: "0x8000" },
    { path: "boot_app0.bin", offset: "0xE000" }, { path: "firmware.bin", offset: "0x10000" },
    { path: "littlefs.bin", offset: "0x390000" }];
const V = { version: "v0.2.6", manifest: "v0.2.6/manifest.json",
            sizes: { "bootloader.bin": 17536, "partitions.bin": 3072, "boot_app0.bin": 8192,
                     "firmware.bin": 1580880, "littlefs.bin": 458752 } };
const NVS = AREAS.find(a => a.name.startsWith("settings"));
const custom = (rows) => PARTS.map((p, i) => ({ path: p.path, text: rows[i] ?? p.offset, ticked: rows[i] !== null }));

// Full install: all five images at their addresses, nothing flagged.
let p = planInstall("full", V, PARTS);
assert.deepEqual(p.parts.map(x => [x.path, x.offset]),
    [["bootloader.bin", 0x1000], ["partitions.bin", 0x8000], ["boot_app0.bin", 0xE000],
     ["firmware.bin", 0x10000], ["littlefs.bin", 0x390000]]);
assert.deepEqual(p.errors, []);
assert.deepEqual(p.warnings, []);

// Firmware only: never touches the settings (nvs) or the web console files; otadata only via boot_app0.
p = planInstall("firmware", V, PARTS);
assert.deepEqual(p.parts.map(x => x.path), ["boot_app0.bin", "firmware.bin"]);
for (const x of p.parts) assert.ok(x.end <= NVS.start || x.offset >= NVS.end, `${x.path} overlaps nvs`);
assert.ok(p.parts.every(x => x.end <= 0x390000));
assert.deepEqual(p.warnings, []);

// Custom: a firmware at the settings area warns; overlaps, the 4 MB end and bad addresses are errors.
p = planInstall("custom", V, PARTS, custom([null, null, null, "0x9000", null]));
assert.ok(p.warnings.some(w => /settings \(nvs\)/.test(w)));
p = planInstall("custom", V, PARTS, custom([null, null, null, "0x10000", "0x10000"]));
assert.ok(p.errors.some(e => /overlaps/.test(e)));
p = planInstall("custom", V, PARTS, custom([null, null, null, null, "0x3F0000"]));
assert.ok(p.errors.some(e => /past the end of the 4 MB flash/.test(e)));
p = planInstall("custom", V, PARTS, custom([null, null, null, "10000", null]));
assert.ok(p.errors.some(e => /not a valid address/.test(e)));
p = planInstall("custom", V, PARTS, custom([null, null, null, null, null]));
assert.ok(p.errors.some(e => /Tick at least one image/.test(e)));
p = planInstall("custom", { ...V, sizes: {} }, PARTS, custom([null, null, null, "0x10000", null]));
assert.ok(p.errors.some(e => /size isn't known/.test(e)));

// Every mode's manifest asks before erasing (without it ESP Web Tools erases the whole chip on
// boards without Improv Serial: the 2026-10-12 wipe) and uses absolute image URLs.
for (const m of MODES) {
    const rows = m.id === "custom" ? custom(["0x1000", null, null, null, null]) : undefined;
    const plan = planInstall(m.id, V, PARTS, rows);
    const man = manifestFor(m.id, V, plan.parts, "https://example.org/install/v0.2.6/manifest.json");
    assert.equal(man.new_install_prompt_erase, true, m.id);
    assert.equal(man.name, m.dialog);
    assert.ok(man.builds[0].parts.every(x => x.path.startsWith("https://example.org/install/v0.2.6/")));
}
console.log("install_plan_test: OK");
