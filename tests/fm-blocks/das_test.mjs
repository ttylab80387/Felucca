// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 ttylab80387
// OP2 (left) pressed during a clear's effect and held: the next piece repeats it once the clear is over
import fs from "fs";
const bytes = fs.readFileSync(process.argv[2]);
async function run(hold) {
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const ex = instance.exports;
  ex._initialize(); ex.web_nor_erase(); ex.web_boot();
  const r = (ms) => { for (let k = 0; k < ms * 44.1; k += 128) ex.web_render(128); };
  r(1500); ex.web_buttons(1 << 10); r(60); ex.web_buttons(0); r(300);   // start
  ex.web_test_game_rows(); r(100);
  ex.web_keys(1 << 19); r(100); ex.web_keys(0); r(100);                 // hard drop: a clear (400 ms)
  if (hold) ex.web_keys(1 << 3);                                        // left pressed in the clear's effect, held
  r(750);
  const x = ex.web_test_game_x();
  ex.web_keys(0); r(100);
  return x;
}
const still = await run(false), held = await run(true);
console.log({ still, held });
console.log(held === 0 && still >= 3 ? "das: ok" : "das: FAIL");
