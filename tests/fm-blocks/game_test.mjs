// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 ttylab80387
// Headless FM BLOCKS check against build/emu/felucca.wasm (as web/emu/emu_test.mjs drives it).
//   node game_test.mjs path/to/felucca.wasm [out.ppm]
import fs from "fs";

const bytes = fs.readFileSync(process.argv[2]);
const B = { HOME: 6, PLAY: 10, OCTUP: 13 };
const EN = { ALGO: 1, PRESETS: 2, SELECT: 0 };
const KEY = { LEFT: 3, SOFT: 6, RIGHT: 5, CCW: 20, CW: 22, DROP: 19, C4: 7 };
let fails = 0;
const check = (what, ok) => { console.log(`game: ${what.padEnd(70)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };

const { instance } = await WebAssembly.instantiate(bytes, {});
const ex = instance.exports, mem = ex.memory;
ex._initialize();
ex.web_nor_erase();
ex.web_boot();
let peak = 0;
const render = (ms) => {
  const n = Math.round(ms * 44.1);
  for (let k = 0; k < n; k += 128) {
    const m = Math.min(128, n - k);
    ex.web_render(m);
    const l = new Float32Array(mem.buffer, ex.web_out_l(), m), r = new Float32Array(mem.buffer, ex.web_out_r(), m);
    for (let i = 0; i < m; i++) peak = Math.max(peak, Math.abs(l[i]), Math.abs(r[i]));
  }
};
const hold = (b, ms) => { ex.web_buttons(1 << b); render(ms); ex.web_buttons(0); render(100); };
const tap = (b) => hold(b, 60);
const key = (k, ms = 60) => { ex.web_keys(1 << k); render(ms); ex.web_keys(0); render(60); };
const turn = (role, n) => { for (let i = 0; i < Math.abs(n); i++) { ex.web_enc(role, Math.sign(n)); render(40); } };
const screen = () => new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240).slice();
const px = (fb, x, y) => fb[y * 240 + x];
const cell = (fb, c, r) => px(fb, 8 + c * 12 + 5, r * 12 + 5);
// a cell holds a piece (or the clear's white, GAME OVER's gray) by its colour: an empty one shows the level's scene
const c565 = (r, g, b) => { const v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3); return ((v >> 8) | (v << 8)) & 0xFFFF; };
const SOLID = new Set([[0, 200, 230], [240, 210, 0], [170, 70, 220], [70, 200, 70], [230, 60, 60], [50, 100, 240],
                       [245, 140, 30], [255, 255, 255], [110, 110, 110]].map(([r, g, b]) => c565(r, g, b)));
const filled = (v) => SOLID.has(v);

render(1500);                                               // boot: the splash, then the title (FELUCCA_GAME_BOOT)
let fb = screen();
const bg = px(fb, 2, 120);
const lit = (f) => { let n = 0; for (let y = 124; y < 143; y++) for (let x = 0; x < 240; x++) if (px(f, x, y) !== bg) n++; return n; };
let blinkOn = 0, blinkOff = 0;
for (let i = 0; i < 8; i++) { render(250); const n = lit(screen()); if (n > 50) blinkOn++; else if (n === 0) blinkOff++; }
let pieces = 0;
for (let x = 0; x < 240; x++) if (px(fb, x, 36) !== bg) pieces++;
check(`power-on: the title, seven pieces (${pieces} px), PRESS PLAY blinks (${blinkOn} lit / ${blinkOff} dark)`,
      pieces > 100 && blinkOn >= 2 && blinkOff >= 2 && px(fb, 6, 120) === bg);

const BTN = () => [ex.web_lit_buttons(), ex.web_dim_buttons(), ex.web_breath_buttons()];
const HOME = 1 << B.HOME, PLAYB = 1 << B.PLAY, GREEN = 1 << 14;
const bTitle = BTN();
check(`buttons on the title: PLAY breathing, HOME dim, the rest dark (${bTitle.map((v) => v.toString(2)).join(" / ")})`,
      bTitle[0] === 0 && bTitle[1] === HOME && bTitle[2] === PLAYB);
peak = 0;
tap(B.PLAY);
render(100);
check(`PLAY on the title: the start jingle sounds (peak ${peak.toFixed(3)})`, peak > 0.01);
fb = screen();
const wall = px(fb, 6, 120);
check("PLAY on the title: the well, its walls drawn", wall !== bg && px(fb, 8 + 120, 120) === wall);
const t0 = screen();
render(2500);
const t1 = screen();
let moved = 0;
for (let r = 0; r < 20; r++) for (let c = 0; c < 10; c++) if (cell(t0, c, r) !== cell(t1, c, r)) moved++;
check(`PLAY: the piece falls by gravity (${moved} cells changed)`, moved > 0);

tap(B.PLAY);                                                // pause: the music stops (its notes ring out)
render(2000);
peak = 0;
key(KEY.C4, 300);
check(`paused: silent, and a key the game does not use plays nothing (peak ${peak.toFixed(4)})`, peak === 0);
tap(B.PLAY);                                                // go on
render(300);
const leftmost = () => { const f = screen(); for (let c = 0; c < 10; c++) for (let r = 0; r < 20; r++) if (filled(cell(f, c, r)) && r < 6) return c; return -1; };
const x0 = leftmost(); key(KEY.RIGHT); const x1 = leftmost(); key(KEY.LEFT); key(KEY.LEFT); const x2 = leftmost(); key(KEY.RIGHT);
check(`OP3 moves right, OP2 left (leftmost column ${x0} -> ${x1} -> ${x2})`, x1 === x0 + 1 && x2 === x1 - 2);
const bPlay = BTN();
check(`buttons in a game: PLAY's green lit, HOME dim, the rest dark (REC too)`,
      bPlay[0] === GREEN && bPlay[1] === HOME && bPlay[2] === 0);
{ // a clear: the rows flash white (the keys' LEDs too), go out from the middle, then drop; the panel's note
  const WHITE = 0xFFFF, nRows = ex.web_test_game_rows();
  render(100);
  ex.web_keys(1 << KEY.DROP); render(40);
  const f1 = screen(), ledAll = ex.web_lit_keys();
  ex.web_keys(0); render(50);
  const f2 = screen();
  render(220);
  const f3 = screen();
  render(300);
  const f4 = screen();
  const row = (f, r) => Array.from({ length: 10 }, (_, c) => cell(f, c, r));
  let empty = true;
  for (let r = 16; r < 20; r++) if (row(f4, r).some(filled)) empty = false;
  let note = 0;
  for (let y = 178; y < 198; y++) for (let x = 140; x < 240; x++) if (px(f4, x, y) !== bg) note++;
  check(`a clear of ${nRows}: rows flash white with every key's LED, then out from the middle, then gone; the note shown`,
        row(f1, 19).every((v) => v === WHITE) && ledAll === (1 << 27) - 1 && !row(f2, 19).every((v) => v === WHITE) &&
        !filled(cell(f3, 4, 19)) && !filled(cell(f3, 5, 19)) && cell(f3, 0, 19) === WHITE && cell(f3, 9, 19) === WHITE &&
        empty && note > 50);
  if (process.env.DBG) {
    const dump = (f, name) => { const img = Buffer.alloc(240 * 240 * 3); for (let i = 0; i < f.length; i++) { const v = ((f[i] >> 8) | (f[i] << 8)) & 0xFFFF; img[i*3] = ((v >> 11) & 31) * 255 / 31; img[i*3+1] = ((v >> 5) & 63) * 255 / 63; img[i*3+2] = (v & 31) * 255 / 31; } fs.writeFileSync(name, Buffer.concat([Buffer.from("P6\n240 240\n255\n"), img])); };
    dump(f1, process.env.DBG + "/c1.ppm"); dump(f2, process.env.DBG + "/c2.ppm"); dump(f3, process.env.DBG + "/c3.ppm"); dump(f4, process.env.DBG + "/c4.ppm");
  }
}
const SIX = (1 << KEY.LEFT) | (1 << KEY.SOFT) | (1 << KEY.RIGHT) | (1 << KEY.CCW) | (1 << KEY.CW) | (1 << KEY.DROP);
const dimIdle = ex.web_dim_keys(), litIdle = ex.web_lit_keys();
ex.web_keys(1 << KEY.CW); render(80);
const dimHeld = ex.web_dim_keys(), litHeld = ex.web_lit_keys();
ex.web_keys(0); render(80);
check(`LEDs: the six keys dim, none lit; POLY held lit, the other five dim`,
      dimIdle === SIX && litIdle === 0 && litHeld === (1 << KEY.CW) && dimHeld === (SIX & ~(1 << KEY.CW)));
const sfx = {};
for (const [n, k] of [["move", KEY.LEFT], ["turn", KEY.CW], ["move back", KEY.RIGHT], ["turn back", KEY.CCW]]) {
  peak = 0; key(k); render(200); sfx[n] = peak;
}
check(`OP2 OP3 MONO POLY: their effects sound (${Object.values(sfx).map((v) => v.toFixed(3)).join(" ")})`,
      Object.values(sfx).every((v) => v > 0.005));

const before = screen();
key(KEY.DROP);
render(100);
fb = screen();
const floorBefore = Array.from({ length: 10 }, (_, c) => cell(before, c, 19)), floorAfter = Array.from({ length: 10 }, (_, c) => cell(fb, c, 19));
check("5th white from the right: a hard drop locks it on the floor; no ghost piece there before",
      floorBefore.every((v) => !filled(v)) && floorAfter.some(filled));

const G565 = ((110 >> 3) << 11) | ((110 >> 2) << 5) | (110 >> 3), GRAY = ((G565 >> 8) | (G565 << 8)) & 0xFFFF;
const grayIn = (f) => { for (let r = 0; r < 20; r++) for (let c = 0; c < 10; c++) if (cell(f, c, r) === GRAY) return true; return false; };
let sweep = null;
for (let d = 0; d < 60; d++) {                              // stack to the top (a column of 4 wide: ~10 pieces)
  key(KEY.DROP); render(50);
  if (!sweep) { const f = screen(); if (grayIn(f)) sweep = f; }   // (the first frame of GAME OVER's gray)
}
render(2000);
const a = screen(); render(1500); const b = screen();
let full = 0;
for (let r = 0; r < 20; r++) if (filled(cell(a, 4, r))) full++;
check(`GAME OVER: column 4 stacked (${full}/20), the board stands still`, full >= 14 && a.every((v, i) => v === b[i]));
{ // the gray rises from the floor: at first the bottom row gray, pieces higher up still in colour; then every piece gray
  let bottomGray = sweep !== null, colourAbove = false, allGray = true;
  if (sweep) for (let c = 0; c < 10; c++) {
    const v = cell(sweep, c, 19);
    if (filled(v) && v !== GRAY) bottomGray = false;
    for (let r = 0; r < 12; r++) { const u = cell(sweep, c, r); if (filled(u) && u !== GRAY) colourAbove = true; }
  }
  for (let r = 0; r < 20; r++) for (let c = 0; c < 10; c++) { const v = cell(a, c, r); if (filled(v) && v !== GRAY) allGray = false; }
  check("GAME OVER: the pieces turn gray from the floor up, then all gray", bottomGray && colourAbove && allGray);
}

tap(B.PLAY);                                                // GAME OVER -> PLAY: the title
render(300);
check("PLAY after GAME OVER: the title", px(screen(), 6, 120) === bg && px(screen(), 128, 120) === bg);
hold(B.HOME, 1200);                                         // HOME held: the menu
render(200);
check("HOME held: back to the menu (the title gone)", lit(screen()) !== 0 || px(screen(), 30, 36) === bg);
turn(EN.ALGO, -1);                                          // SYSTEM (the last tab)
turn(EN.PRESETS, 4);                                        // USB SERIAL RESTORE LAST HELP 2ND CORE -> GAME
tap(B.OCTUP);                                               // GAME again
render(300);
fb = screen();
let deco = 0;
for (let x = 0; x < 240; x++) if (px(fb, x, 36) !== bg) deco++;
check(`GAME again: the title (${deco} px of pieces)`, deco > 100 && px(fb, 6, 120) === bg);
tap(B.PLAY);                                                // PLAY: a new game
render(300);
fb = screen();
let empty = 0;
for (let c = 0; c < 10; c++) if (!filled(cell(fb, c, 19))) empty++;
check(`PLAY on the title: a new, empty board (${empty}/10 floor cells empty)`, empty >= 6);

render(500);                                                // (the new game plays: pause it)
tap(B.PLAY);
render(500);
const paused = screen();
const bPause = BTN();
check("buttons paused: PLAY breathing, HOME dim, the rest dark", bPause[0] === 0 && bPause[1] === HOME && bPause[2] === PLAYB);
render(61000);                                              // a minute untouched: the saver
const s1 = screen();
let litPx = 0;
for (const v of s1) if (v !== bg) litPx++;
render(4500);
const s2 = screen();
if (process.env.DBG) {
  const dump = (f, name) => { const img = Buffer.alloc(240 * 240 * 3); for (let i = 0; i < f.length; i++) { const v = ((f[i] >> 8) | (f[i] << 8)) & 0xFFFF; img[i*3] = ((v >> 11) & 31) * 255 / 31; img[i*3+1] = ((v >> 5) & 63) * 255 / 63; img[i*3+2] = (v & 31) * 255 / 31; } fs.writeFileSync(name, Buffer.concat([Buffer.from("P6\n240 240\n255\n"), img])); };
  dump(paused, process.env.DBG + "/paused.ppm"); dump(s1, process.env.DBG + "/s1.ppm"); dump(s2, process.env.DBG + "/s2.ppm");
}
check(`a minute idle (paused): the saver, the screen dark but a small PRESS PLAY (${litPx} px), moving`,
      px(s1, 6, 120) === bg && litPx > 50 && litPx < 3000 && s2.some((v, i) => v !== s1[i]));
key(KEY.LEFT);                                              // any input wakes it, and does nothing more
render(300);
const woke = screen();
render(2000);
check("a key wakes it: the paused board back as it was, still paused",
      woke.every((v, i) => v === paused[i]) && screen().every((v, i) => v === woke[i]));
if (process.argv[3]) {                                      // a picture to look at (the GAME OVER screen)
  const img = Buffer.alloc(240 * 240 * 3);
  fb = a;
  for (let i = 0; i < fb.length; i++) {
    const v = ((fb[i] >> 8) | (fb[i] << 8)) & 0xFFFF;       // (the panel's byte order -> RGB565)
    img[i * 3] = ((v >> 11) & 31) * 255 / 31; img[i * 3 + 1] = ((v >> 5) & 63) * 255 / 63; img[i * 3 + 2] = (v & 31) * 255 / 31;
  }
  fs.writeFileSync(process.argv[3], Buffer.concat([Buffer.from("P6\n240 240\n255\n"), img]));
}
console.log(fails ? `game: ${fails} failed` : "game: all ok");
process.exit(fails ? 1 : 0);
