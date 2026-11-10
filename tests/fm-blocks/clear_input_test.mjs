// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 ttylab80387
// input during a clear's effect: (a) PLAY + OP2 together, OP2 held: no pause, the next piece repeats left;
// (b) the soft-drop key held over the clear: the next piece falls at the soft rate at once (no normal-gravity wait)
import fs from "fs";
const bytes = fs.readFileSync(process.argv[2]);
async function clearing() {
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const ex = instance.exports;
  ex._initialize(); ex.web_nor_erase(); ex.web_boot();
  const r = (ms) => { for (let k = 0; k < ms * 44.1; k += 128) ex.web_render(128); };
  r(1500); ex.web_buttons(1 << 10); r(60); ex.web_buttons(0); r(300);   // start
  ex.web_test_game_rows(); r(100);
  ex.web_keys(1 << 19); r(100); ex.web_keys(0); r(100);                 // hard drop: a clear (400 ms)
  return { ex, r };
}
let ok = true;
{
  const { ex, r } = await clearing();
  ex.web_buttons(1 << 10); ex.web_keys(1 << 3); r(60); ex.web_buttons(0);   // PLAY + OP2 in the clear
  r(700);
  const st = ex.web_test_game_st(), x = ex.web_test_game_x();
  ex.web_keys(0); r(60);
  console.log("PLAY + OP2 in a clear:", { st, x });
  ok &&= st === 2 && x === 0;
}
{
  const { ex, r } = await clearing();
  ex.web_keys(1 << 6); r(200);                                              // soft drop pressed in the clear, held
  const y0 = ex.web_test_game_y();                                          // (the clear over at ~200 ms from here)
  r(400);
  const y1 = ex.web_test_game_y();
  ex.web_keys(0); r(60);
  console.log("soft drop held over the clear:", { y0, y1 });
  ok &&= y1 - y0 >= 4;
}
{                                                                           // (c) hard drop + OP2 together, OP2 held
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const ex = instance.exports;
  ex._initialize(); ex.web_nor_erase(); ex.web_boot();
  const r = (ms) => { for (let k = 0; k < ms * 44.1; k += 128) ex.web_render(128); };
  r(1500); ex.web_buttons(1 << 10); r(60); ex.web_buttons(0); r(300);
  ex.web_test_game_rows(); r(100);
  ex.web_keys((1 << 19) | (1 << 3)); r(100); ex.web_keys(1 << 3);         // drop and left in one pass; left held
  r(800);
  const x = ex.web_test_game_x();
  ex.web_keys(0); r(60);
  console.log("hard drop + OP2 together:", { x });
  ok &&= x === 0;
}
console.log(ok ? "clear input: ok" : "clear input: FAIL");
