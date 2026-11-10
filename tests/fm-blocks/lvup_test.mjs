// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 ttylab80387
// clears until LINES crosses 10; compares the sound after the thump of the level-up clear with a plain one
import fs from "fs";
const { instance } = await WebAssembly.instantiate(fs.readFileSync(process.argv[2]), {});
const ex = instance.exports, mem = ex.memory;
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
let L = [];
const r = (ms, keep) => { for (let k = 0; k < ms * 44.1; k += 128) { ex.web_render(128); if (keep) L.push(...new Float32Array(mem.buffer, ex.web_out_l(), 128)); } };
const rms = (a, t0, t1) => { let s = 0, n = 0; for (let i = Math.floor(t0 * 44100); i < Math.min(a.length, t1 * 44100); i++, n++) s += a[i] * a[i]; return 20 * Math.log10(Math.sqrt(s / Math.max(n, 1)) + 1e-9); };
r(1500); ex.web_buttons(1 << 10); r(60); ex.web_buttons(0); r(16000);   // start; the music played out
let lines = 0, plain = null, lvup = null;
const w = [0.75, 1.3];
while (lines < 10) {
  const n = ex.web_test_game_rows(); r(100);
  const cross = lines < 10 && lines + n >= 10;
  L = []; ex.web_keys(1 << 19); r(1600, true); ex.web_keys(0);
  lines += n;
  if (cross) lvup = L; else plain = L;
}
// the level-up clear again, the next piece hard-dropped right after the clear (before the fanfare is due): still there
let quick = null;
for (let crossed = false; !crossed; ) {
  const n = ex.web_test_game_rows(); r(100);
  crossed = Math.floor(lines / 10) < Math.floor((lines + n) / 10);
  L = []; ex.web_keys(1 << 19); r(450, true); ex.web_keys(0); r(30, true);
  if (crossed) { ex.web_keys(1 << 19); r(60, true); ex.web_keys(0); }
  r(1060, true);
  lines += n;
  if (crossed) quick = L;
}
console.log("level, quick drop after the clear", rms(quick, ...w).toFixed(1), "dBFS");
// the drop at 0, the clear 400 ms, the thump, the fanfare ~0.7 s on
console.log("plain  after thump", rms(plain, ...w).toFixed(1), "dBFS");
console.log("level  after thump", rms(lvup, ...w).toFixed(1), "dBFS");
console.log("level  0-0.4 s (line sound kept)", rms(lvup, 0, 0.4).toFixed(1), "plain", rms(plain, 0, 0.4).toFixed(1));
const ok = rms(lvup, ...w) - rms(plain, ...w) > 10 && rms(quick, ...w) > rms(lvup, ...w) - 4 && Math.abs(rms(lvup, 0, 0.4) - rms(plain, 0, 0.4)) < 3;
const pcm = Buffer.alloc(lvup.length * 2); lvup.forEach((v, i) => pcm.writeInt16LE(Math.max(-32767, Math.min(32767, Math.round(v * 32767))), i * 2));
const h = Buffer.alloc(44); h.write("RIFF", 0); h.writeUInt32LE(36 + pcm.length, 4); h.write("WAVEfmt ", 8); h.writeUInt32LE(16, 16);
h.writeUInt16LE(1, 20); h.writeUInt16LE(1, 22); h.writeUInt32LE(44100, 24); h.writeUInt32LE(88200, 28); h.writeUInt16LE(2, 32); h.writeUInt16LE(16, 34); h.write("data", 36); h.writeUInt32LE(pcm.length, 40);
if (process.argv[3]) fs.writeFileSync(process.argv[3], Buffer.concat([h, pcm]));
console.log(ok ? "lvup: ok" : "lvup: FAIL");
