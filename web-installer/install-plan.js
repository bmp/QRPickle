// The installer page's plan (pure functions, no DOM): which images each mode writes, where, and what
// it would touch. Used by web-installer/pages/install.html; unit-tested with Node
// (test/js/install_plan_test.mjs).
const MODES = [
  {id: "full", label: "Full install", dialog: "QRPickle (full install)",
   desc: "Everything: bootloader, partition table, firmware and web console files. Writing the web console " +
         "files deletes saved profiles (download a backup first); settings stay. You will be asked " +
         "\"Erase device\": tick it for a clean start, which also clears settings. For a new board.",
   files: ["bootloader.bin", "partitions.bin", "boot_app0.bin", "firmware.bin", "littlefs.bin"]},
  {id: "firmware", label: "Firmware only", dialog: "QRPickle (firmware only; settings and profiles kept)",
   desc: "Only the firmware (and the record of which slot to start from). Settings, profiles and the web " +
         "console files stay: leave \"Erase device\" unticked when asked. The board must already run " +
         "QRPickle v0.2.0 or later; the web console files stay at their old version until a full install " +
         "or a filesystem upload.",
   files: ["boot_app0.bin", "firmware.bin"]},
  {id: "custom", label: "Custom", dialog: "QRPickle (custom: chosen images only)",
   desc: "Pick the images and their addresses (from the partition table). Writing the web console files " +
         "erases saved profiles. Leave \"Erase device\" unticked when asked, unless you want the whole " +
         "chip cleared. Only change an address if you know why.",
   files: null},
];

// The flash areas (partitions.csv; the bootloader and partition table at the ESP32's fixed places).
const AREAS = [
  {name: "bootloader", start: 0x1000, end: 0x8000},
  {name: "partition table", start: 0x8000, end: 0x9000},
  {name: "settings (nvs)", start: 0x9000, end: 0xE000, keep: "your settings (WiFi, callsign, keys)", guard: true},
  {name: "start-slot record (otadata)", start: 0xE000, end: 0x10000, guard: true, owner: "boot_app0.bin"},
  {name: "firmware slot app0", start: 0x10000, end: 0x1D0000},
  {name: "firmware slot app1", start: 0x1D0000, end: 0x390000, keep: "the second firmware slot (app1)"},
  {name: "web console files (LittleFS)", start: 0x390000, end: 0x400000,
   keep: "the web console files and your saved profiles"},
];
const hex = n => "0x" + n.toString(16).toUpperCase();
const range = (a, b) => `${hex(a)}–${hex(b - 1)}`;
const areasOf = (a, b) => AREAS.filter(r => a < r.end && b > r.start);

// What a mode would write for version v: {parts: [{path, offset, end}], errors: [], warnings: []}.
// parts: the images of versions.json ({path, offset}); custom (custom mode only): [{path, text,
// ticked}] from the page's rows. Each image is written to whole 4 KB sectors from its address;
// nothing else on the chip is erased unless "Erase device" is ticked in the installer's dialog.
function planInstall(modeId, v, parts, custom) {
  const m = MODES.find(x => x.id === modeId);
  const out = {parts: [], errors: [], warnings: []};
  const sizes = v.sizes || {};
  const picks = m.files
    ? parts.filter(p => m.files.includes(p.path)).map(p => ({path: p.path, text: p.offset}))
    : (custom || []).filter(x => x.ticked);
  if (!picks.length) out.errors.push("Tick at least one image.");
  for (const x of picks) {
    const off = /^0x[0-9a-f]+$/i.test(x.text) ? parseInt(x.text, 16) : NaN;
    if (isNaN(off) || off % 0x1000) {
      out.errors.push(`${x.path}: "${x.text}" is not a valid address. Use hex such as 0x10000, in steps of 0x1000.`);
      continue;
    }
    const size = sizes[x.path];
    if (!size) {
      out.errors.push(`${x.path}: its size isn't known for ${v.version}. Use Full install or Firmware only.`);
      continue;
    }
    const end = off + Math.ceil(size / 0x1000) * 0x1000;
    if (end > 0x400000) {
      out.errors.push(`${x.path} needs ${range(off, end)}, past the end of the 4 MB flash (0x3FFFFF). Use a lower address.`);
      continue;
    }
    const clash = out.parts.find(q => off < q.end && end > q.offset);
    if (clash) {
      out.errors.push(`${x.path} (${range(off, end)}) overlaps ${clash.path} (${range(clash.offset, clash.end)}). Move one of them.`);
      continue;
    }
    for (const r of areasOf(off, end))
      if (r.guard && r.owner !== x.path)
        out.warnings.push(`${x.path} will overwrite the ${r.name} area (${range(r.start, r.end)}).`);
    out.parts.push({path: x.path, offset: off, end});
  }
  return out;
}

// The ESP Web Tools manifest for the chosen images. Always ask about erasing: without the question,
// ESP Web Tools ERASES THE WHOLE CHIP on boards that don't speak Improv Serial (QRPickle doesn't):
// "Firmware only" would then wipe settings and leave no bootloader. With the question, an unticked
// "Erase device" writes only the chosen images. The dialogs show the manifest name: it names the mode.
// manifestHref: the version's manifest URL (absolute); image URLs are resolved against it.
function manifestFor(modeId, v, chosen, manifestHref) {
  const m = MODES.find(x => x.id === modeId);
  return {name: m.dialog, version: v.version, new_install_prompt_erase: true,
          builds: [{chipFamily: "ESP32", parts: chosen.map(p => ({path: new URL(p.path, manifestHref).href, offset: p.offset}))}]};
}

if (typeof module !== "undefined") module.exports = {MODES, AREAS, hex, range, areasOf, planInstall, manifestFor};
