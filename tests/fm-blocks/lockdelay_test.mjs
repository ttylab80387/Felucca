// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 ttylab80387
// lock delay: a piece on the floor can still move / turn before it locks; each move there starts the delay again,
// at most 15 times a piece (then it locks however it is played)
import fs from "fs";
const bytes = fs.readFileSync(process.argv[2]);
async function landed() {                                               // a new game, the first piece soft-dropped down
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const ex = instance.exports;
  ex._initialize(); ex.web_nor_erase(); ex.web_boot();
  const r = (ms) => { for (let k = 0; k < ms * 44.1; k += 128) ex.web_render(128); };
  r(1500); ex.web_buttons(1 << 10); r(60); ex.web_buttons(0); r(100);
  ex.web_keys(1 << 6);                                                  // soft drop most of the way (held to the floor
  for (let t = 0; t < 3000 && ex.web_test_game_y() < 14; t += 10) r(10);  // .. it would lock at once), then gravity
  ex.web_keys(0);
  for (let t = 0; t < 8000 && !ex.web_test_game_ground(); t += 5) r(5);
  return { ex, r, floorY: ex.web_test_game_y() };
}
const tap = (ex, r, k) => { ex.web_keys(1 << k); r(40); ex.web_keys(0); r(20); };
let ok = true;
{ // (a) on the floor: OP2 moves it there, it is still the same piece; untouched it then locks
  const { ex, r, floorY } = await landed();
  const x0 = ex.web_test_game_x();
  r(200); tap(ex, r, 3);
  const x1 = ex.web_test_game_x(), y1 = ex.web_test_game_y();
  r(1200);
  const y2 = ex.web_test_game_y();
  console.log("(a) moved on the floor, then locked:", { floorY, x0, x1, y1, y2 });
  ok &&= floorY >= 17 && x1 === x0 - 1 && y1 === floorY && y2 <= 0;
}
{ // (b) moving every 300 ms on the floor keeps it there past the plain delay (2 s)
  const { ex, r, floorY } = await landed();
  for (let i = 0; i < 6; i++) { r(260); tap(ex, r, i & 1 ? 5 : 3); }
  const y = ex.web_test_game_y();
  console.log("(b) still on the floor after 2 s of moves:", { floorY, y });
  ok &&= y === floorY;
}
{ // (c) 15 restarts at most: moving on and on, it still locks
  const { ex, r, floorY } = await landed();
  let lockedAt = -1;
  for (let i = 0; i < 40 && lockedAt < 0; i++) { r(260); tap(ex, r, i & 1 ? 5 : 3); if (ex.web_test_game_y() <= 0) lockedAt = i; }
  console.log("(c) locked after this many moves:", { floorY, lockedAt });
  ok &&= lockedAt >= 14 && lockedAt <= 20;
}
{ // (d) turning back and forth on the floor (a kick lifts it, it lands again): still locks, for every first piece
  for (let v = 0; v < 12; v++) {
    const { instance } = await WebAssembly.instantiate(bytes, {});
    const ex = instance.exports;
    ex._initialize(); ex.web_nor_erase(); ex.web_boot();
    const r = (ms) => { for (let k = 0; k < ms * 44.1; k += 128) ex.web_render(128); };
    r(1500 + v * 37); ex.web_buttons(1 << 10); r(60); ex.web_buttons(0); r(100);   // (another first piece each)
    ex.web_keys(1 << 6); for (let t = 0; t < 3000 && ex.web_test_game_y() < 14; t += 10) r(10);
    ex.web_keys(0); for (let t = 0; t < 8000 && !ex.web_test_game_ground(); t += 5) r(5);   // on the floor by gravity
    let locked = false;
    for (let i = 0; i < 80 && !locked; i++) {                                      // CCW, CW, .. every 200 ms
      ex.web_keys(1 << (i & 1 ? 22 : 20)); r(40); ex.web_keys(0); r(160);
      if (ex.web_test_game_y() <= 1) locked = true;
    }
    ex.web_keys(0); r(20);
    if (!locked) console.log("(d) never locked, start", v);
    ok &&= locked;
  }
  console.log("(d) turning on the floor, 12 first pieces: all locked?");
}
{ // (e) soft drop held, both turn keys together over and over (the kick lifts it as it lands): still locks
  let stuck = 0;
  for (let v = 0; v < 12; v++) {
    const { instance } = await WebAssembly.instantiate(bytes, {});
    const ex = instance.exports;
    ex._initialize(); ex.web_nor_erase(); ex.web_boot();
    const r = (ms) => { for (let k = 0; k < ms * 44.1; k += 128) ex.web_render(128); };
    r(1500 + v * 37); ex.web_buttons(1 << 10); r(60); ex.web_buttons(0); r(100);
    let spawns = 0, last = ex.web_test_game_y(), chord = 0;
    for (let b = 0; b < 2800; b++) {                                       // ~8 s, an audio block at a time
      ex.web_keys((1 << 6) | (chord === 1 ? (1 << 20) | (1 << 22) : 0));  // the chord in the block after a step down
      ex.web_render(128);
      const y = ex.web_test_game_y();
      chord = y > last ? 1 : chord === 1 ? 2 : 0;
      if (y < last - 2) spawns++;
      last = y;
    }
    ex.web_keys(0); r(20);
    if (spawns < 3) { stuck++; console.log("(e) start", v, "pieces locked in 8 s:", spawns); }
  }
  console.log("(e) chords of both turns with soft drop held, 12 first pieces stuck:", stuck);
  ok &&= stuck === 0;
}
{ // (f) soft drop held down to the floor: it locks as it lands (no delay)
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const ex = instance.exports;
  ex._initialize(); ex.web_nor_erase(); ex.web_boot();
  const r = (ms) => { for (let k = 0; k < ms * 44.1; k += 128) ex.web_render(128); };
  r(1500); ex.web_buttons(1 << 10); r(60); ex.web_buttons(0); r(100);
  ex.web_keys(1 << 6);
  let maxY = -9, lastDown = 0, gap = -1;                                 // gap: its last step down .. the next piece
  for (let t = 0; t < 3000 && gap < 0; t += 5) {
    r(5);
    const y = ex.web_test_game_y();
    if (y < maxY - 2) gap = t - lastDown;
    else if (y > maxY) { maxY = y; lastDown = t; }
  }
  ex.web_keys(0); r(20);
  console.log("(f) soft drop held to the floor, its last step down to the next piece (ms):", { maxY, gap });
  ok &&= maxY >= 17 && gap >= 0 && gap <= 60;                           // (a soft step is 40 ms; the delay 800)
}
{ // (g) soft drop held from the last piece onto a new piece that spawns on the floor: it gets its delay (no
  //     lock at once, no GAME OVER forced before the player can move it)
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const ex = instance.exports;
  ex._initialize(); ex.web_nor_erase(); ex.web_boot();
  const r = (ms) => { for (let k = 0; k < ms * 44.1; k += 128) ex.web_render(128); };
  r(1500); ex.web_buttons(1 << 10); r(60); ex.web_buttons(0); r(100);
  ex.web_keys(1 << 6);                                                   // soft drop held: the first piece down
  let maxY = -9, spawned = false;
  for (let t = 0; t < 3000 && !spawned; t += 1) {
    ex.web_render(44);                                                   // (~1 ms at a time)
    const y = ex.web_test_game_y();
    if (y < maxY - 2) spawned = true; else maxY = Math.max(maxY, y);
  }
  for (let c = 0; c < 9; c++) ex.web_test_game_cell(2, c, 1);           // the next piece now sits on row 2 (not full:
                                                                         // .. no clear)
  r(300);                                                                // soft still held
  const st = ex.web_test_game_st(), y = ex.web_test_game_y(), ground = ex.web_test_game_ground();
  ex.web_keys(0); r(20);
  console.log("(g) soft held from the last piece, the new one spawned on the floor, 300 ms on:", { spawned, st, y, ground });
  ok &&= spawned && st === 2 && ground === 1 && y <= 1;
}
console.log(ok ? "lock delay: ok" : "lock delay: FAIL");
