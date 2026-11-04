// Unit test of data/www/logo.js (the web console's logo converter). Run: node test/js/logo_test.mjs
import assert from "node:assert/strict";
import { createRequire } from "node:module";
const require = createRequire(import.meta.url);
const { logoFitBox, logoEncode } = require("../../data/www/logo.js");

// Same rounding as scripts/png_to_bin_lvgl9.py fit_box (floor(x + 0.5), not banker's rounding).
assert.deepEqual(logoFitBox(100, 50), { w: 60, h: 30 });
assert.deepEqual(logoFitBox(30, 60), { w: 30, h: 60 });
assert.deepEqual(logoFitBox(10, 10), { w: 60, h: 60 });      // small logos are scaled up
assert.deepEqual(logoFitBox(1000, 1), { w: 60, h: 1 });      // never 0
assert.deepEqual(logoFitBox(120, 45), { w: 60, h: 23 });     // 22.5 rounds up
assert.deepEqual(logoFitBox(45, 120), { w: 23, h: 60 });

// 2 x 2: red, half-transparent green, transparent blue, white. Expected bytes: the Python
// encoder's output for the same pixels (scripts/test_png_to_bin_lvgl9.py pins the same hex).
const rgba = new Uint8ClampedArray([255, 0, 0, 255, 0, 255, 0, 128, 0, 0, 255, 0, 255, 255, 255, 255]);
const hex = Buffer.from(logoEncode(rgba, 2, 2)).toString("hex");
assert.equal(hex, "191400000200020004000000" + "00f8e0071f00ffff" + "ff8000ff");
console.log("logo_test: OK");
