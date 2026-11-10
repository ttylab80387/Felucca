// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 ttylab80387
// a picture of the game at each background scene (LINES 0, 20, .. 140): <wasm> [out dir]; each scene its own
// (the empty cells' pixels differ between every two), LINES 19 still the first
import fs from "fs";
const { instance } = await WebAssembly.instantiate(fs.readFileSync(process.argv[2]), {});
const ex = instance.exports, mem = ex.memory;
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
const r = (ms) => { for (let k = 0; k < ms * 44.1; k += 128) ex.web_render(128); };
r(1500); ex.web_buttons(1 << 10); r(60); ex.web_buttons(0); r(300);
ex.web_buttons(1 << 10); r(60); ex.web_buttons(0); r(200);                  // paused: nothing falls meanwhile
const well = (fb) => Array.from({ length: 12 }, (_, i) => fb[(20 + i * 18) * 240 + 8 + 30]).join(",");   // (column 2)
const seen = [];
ex.web_test_game_lines(19); r(400);
const at19 = well(new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240));
for (let s = 0; s < 8; s++) {
  ex.web_test_game_lines(s * 20); r(400);
  const fb = new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240);
  seen.push(well(fb));
  const ppm = Buffer.alloc(15 + 240 * 240 * 3); ppm.write("P6\n240 240\n255\n");
  for (let i = 0; i < 240 * 240; i++) {
    const v = ((fb[i] & 255) << 8) | (fb[i] >> 8);                         // (the LCD's byte order)
    ppm[15 + i * 3] = (v >> 11) << 3; ppm[16 + i * 3] = ((v >> 5) & 63) << 2; ppm[17 + i * 3] = (v & 31) << 3;
  }
  if (process.argv[3]) fs.writeFileSync(`${process.argv[3]}/scene${s}.ppm`, ppm);
}
const distinct = new Set(seen).size;
console.log("scenes:", distinct, "different of 8; LINES 19 as LINES 0:", at19 === seen[0]);
console.log(distinct === 8 && at19 === seen[0] ? "scenes: ok" : "scenes: FAIL");
