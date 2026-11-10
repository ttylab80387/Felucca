// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 ttylab80387
// the synth's song playing, then the menu's GAME: the song stops (PLAY is the game's; BEST is saved only when stopped)
import fs from "fs";
const { instance } = await WebAssembly.instantiate(fs.readFileSync(process.argv[2]), {});
const ex = instance.exports;
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
const r = (ms) => { for (let k = 0; k < ms * 44.1; k += 128) ex.web_render(128); };
const tap = (b, ms = 60) => { ex.web_buttons(1 << b); r(ms); ex.web_buttons(0); r(100); };
r(1500);
ex.web_buttons(1 << 6); r(1200); ex.web_buttons(0); r(200);   // HOME held: out of the game, the menu
const out = !ex.web_game_on();
tap(6); r(300);                                               // HOME: the menu closed
tap(10); r(300);                                              // PLAY: the song
const playing = ex.web_playing();
ex.web_buttons(1 << 6); r(1200); ex.web_buttons(0); r(200);   // HOME held: the menu
ex.web_enc(1, -1); r(60);                                     // SYSTEM
for (let j = 0; j < 4; j++) { ex.web_enc(2, 1); r(60); }      // GAME
tap(13); r(300);                                              // OCT+: open it
const inGame = ex.web_game_on(), after = ex.web_playing();
console.log({ out, playing, inGame, after });
console.log(out && playing && inGame && !after ? "transport: ok" : "transport: FAIL");
