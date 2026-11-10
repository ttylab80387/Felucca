// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 ttylab80387
// BEST survives a power-off: play to GAME OVER (hard drops score), keep the flash, boot a fresh instance with it
import fs from "fs";
const bytes = fs.readFileSync(process.argv[2]);
async function dev(nor) {
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const ex = instance.exports;
  ex._initialize(); ex.web_nor_erase();
  if (nor) new Uint8Array(ex.memory.buffer, ex.web_nor(), ex.web_nor_size()).set(nor);
  ex.web_boot();
  const r = (ms) => { for (let k = 0; k < ms * 44.1; k += 128) ex.web_render(128); };
  const line = () => { const f = new Uint16Array(ex.memory.buffer, ex.web_screen(), 240 * 240); return Array.from(f.slice(223 * 240, 238 * 240)); };
  return { ex, r, line };
}
const a = await dev(null);
a.r(1500);
const best0 = a.line();
a.ex.web_buttons(1 << 10); a.r(60); a.ex.web_buttons(0); a.r(200);           // play
for (let d = 0; d < 60; d++) { a.ex.web_keys(1 << 19); a.r(60); a.ex.web_keys(0); a.r(100); }   // hard drops to GAME OVER
a.r(3000);
const writes = a.ex.web_flash_writes_count();
const nor = new Uint8Array(a.ex.memory.buffer, a.ex.web_nor(), a.ex.web_nor_size()).slice();
const b = await dev(nor);                                                      // power off, on
b.r(1500);
const kept = b.line();
const c = await dev(null);                                                     // a blank flash, for comparison
c.r(1500);
const differs = kept.some((v, i) => v !== best0[i]), blankSame = c.line().every((v, i) => v === best0[i]);
console.log(`best: flash writes ${writes}; after a power-off the title's BEST line ${differs ? "shows the score" : "is still 0"}; a blank flash: ${blankSame ? "BEST 0" : "?"}`);
console.log(differs && blankSame && writes > 0 ? "best: ok" : "best: FAIL");
