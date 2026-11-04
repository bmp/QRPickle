// Custom splash logos: scale an image into the 60 x 60 box and encode it as the LVGL 9 RGB565A8
// .bin the device stores (same bytes as scripts/png_to_bin_lvgl9.py --fit 60x60 for the same pixels).
// Pure functions: unit-tested with Node (test/js/logo_test.mjs).
function logoFitBox(srcW, srcH, boxW = 60, boxH = 60) {
    const scale = Math.min(boxW / srcW, boxH / srcH);
    return { w: Math.max(1, Math.floor(srcW * scale + 0.5)), h: Math.max(1, Math.floor(srcH * scale + 0.5)) };
}

function logoEncode(rgba, w, h) {
    const out = new Uint8Array(12 + w * h * 3);
    const stride = w * 2;
    out.set([0x19, 0x14, 0, 0, w & 0xff, w >> 8, h & 0xff, h >> 8, stride & 0xff, stride >> 8, 0, 0]);
    const alpha = 12 + w * h * 2;
    for (let i = 0; i < w * h; i++) {
        const r = rgba[i * 4], g = rgba[i * 4 + 1], b = rgba[i * 4 + 2];
        const c = ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3);
        out[12 + i * 2] = c & 0xff;
        out[13 + i * 2] = c >> 8;
        out[alpha + i] = rgba[i * 4 + 3];
    }
    return out;
}

if (typeof module !== "undefined") module.exports = { logoFitBox, logoEncode };
