// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 ttylab80387
// BEST's bytes holding no BEST (a foreign backup): the game reads 0, not a 10-digit number its panel cannot hold
import fs from "fs";
const { instance } = await WebAssembly.instantiate(fs.readFileSync(process.argv[2]), {});
const ex = instance.exports;
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
const r = (ms) => { for (let k = 0; k < ms * 44.1; k += 128) ex.web_render(128); };
const tap = (b, ms = 60) => { ex.web_buttons(1 << b); r(ms); ex.web_buttons(0); r(100); };
r(1500);
ex.web_buttons(1 << 6); r(1200); ex.web_buttons(0); r(200);   // HOME held: the menu
let ok = true;
let first = true;
for (const v of [0x90000000, 0xC4653600, 100000000, 12345]) {
  ex.web_test_game_best_bytes(v);
  if (first) { ex.web_enc(1, -1); r(60); for (let j = 0; j < 4; j++) { ex.web_enc(2, 1); r(60); } }   // SYSTEM > GAME (kept after)
  first = false;
  tap(13); r(300);                                                                       // open it: BEST read
  const best = ex.web_test_game_best();
  console.log({ stored: v >>> 0, best, on: ex.web_game_on() });
  ok &&= best === (v <= 99999999 ? v : 0) && ex.web_game_on() === 1;
  ex.web_buttons(1 << 6); r(1200); ex.web_buttons(0); r(200);                            // back to the menu
}
console.log(ok ? "best bytes: ok" : "best bytes: FAIL");
