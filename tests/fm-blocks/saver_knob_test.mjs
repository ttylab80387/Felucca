// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 ttylab80387
// the saver (a minute idle on the title) wakes on any knob, not only SELECT
import fs from "fs";
const bytes = fs.readFileSync(process.argv[2]);
const lit = (ex, mem) => { const s = new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240), bg = s[120 * 240 + 2]; let n = 0; for (const v of s) if (v !== bg) n++; return n; };   // (not the background)
let ok = true;
for (const enc of [0, 1, 2, 3, 4, 5]) {
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const ex = instance.exports, mem = ex.memory;
  ex._initialize(); ex.web_nor_erase(); ex.web_boot();
  const r = (ms) => { for (let k = 0; k < ms * 44.1; k += 128) ex.web_render(128); };
  r(1500); const title = lit(ex, mem);
  r(61000); const saver = lit(ex, mem);
  ex.web_enc(enc, 1); r(400); const woke = lit(ex, mem);
  console.log({ enc, title, saver, woke });
  ok &&= saver < title / 4 && woke > title / 2;
}
console.log(ok ? "saver knob: ok" : "saver knob: FAIL");
