#!/usr/bin/env node
// Measures the tuning of NULL Wind (DATA/Effects/null_jsfx/wind.jsfx, through its web version
// web/fx/wind.js, the same code): every model plays each note of its range for 1 s at velocity
// 100, and the pitch of the steady part is found by autocorrelation. Prints the error in cents.
//   node tools/tune_wind.js            the table, at 48 kHz
//   node tools/tune_wind.js --brass    also the brass tuning table (BT in wind.jsfx's @init) that
//                                      cancels the measured error, found in a few passes
//   node tools/tune_wind.js --check    exit 1 if a note is more than 12 cents off at 44.1, 48 or
//                                      96 kHz (run after node web/tools/build.js)
"use strict";
const path = require("path");
const root = path.resolve(__dirname, "..");
global.NULLFX = {};
const R = require(path.join(root, "web/runtime/eel-rt.js"));
const HOST = require(path.join(root, "web/runtime/null-fx.js"));
require(path.join(root, "web/fx/wind.js"));
const FX = NULLFX.wind;
const RANGE = [[60, 96], [50, 91], [49, 81], [40, 88]];
const NAMES = ["Flute", "Clarinet", "Saxophone", "Brass"];

function render(model, note, sr, bt) {
  const sliders = { 1: model };
  const fx = new HOST.Effect(FX, R, { srate: sr, blockSize: 128, sliders, playing: true });
  const N = sr, out = [new Float64Array(N), new Float64Array(N)];
  // one silent block runs @init, then the table under test goes in and the note starts
  fx.process([], [new Float64Array(128), new Float64Array(128)], 128, 0, []);
  if (bt) fx.inst.mem.write(fx.inst.getVar("BT"), Float64Array.from(bt));
  for (let n = 0; n < N; n += 128) {
    const len = Math.min(128, N - n);
    fx.process([], out.map((a) => a.subarray(n, n + len)), len, 0, n === 0 ? [[0, 0x90, note, 100]] : []);
  }
  return out[0];
}

function pitch(x, sr, guess) {
  const a = Math.round(sr * 0.6), n = Math.round(sr / 16);
  const lo = Math.floor(sr / (guess * 2.2)), hi = Math.ceil(sr / (guess / 2.2));
  const c = new Float64Array(hi + 2);
  for (let l = lo - 1; l <= hi + 1; l++) {
    let s = 0, e1 = 0, e2 = 0;
    for (let i = 0; i < n; i++) { s += x[a + i] * x[a + i + l]; e1 += x[a + i] ** 2; e2 += x[a + i + l] ** 2; }
    c[l] = s / Math.sqrt(e1 * e2 + 1e-30);
  }
  // the strongest peak, or the shortest lag nearly as strong (so not an octave below)
  let best = lo;
  for (let l = lo; l <= hi; l++) if (c[l] > c[best]) best = l;
  for (let l = lo; l <= hi; l++) if (c[l] > 0.9 * c[best] && c[l] >= c[l - 1] && c[l] >= c[l + 1]) { best = l; break; }
  const y0 = c[best - 1], y1 = c[best], y2 = c[best + 1];
  return sr / (best + 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2));
}

function cents(model, note, sr, bt) {
  const f = 440 * 2 ** ((note - 69) / 12);
  const x = render(model, note, sr, bt);
  let e = 0;
  for (let i = Math.round(sr * 0.6); i < sr * 0.9; i++) e += x[i] * x[i];
  if (!(e > 1e-6)) return NaN;
  return 1200 * Math.log2(pitch(x, sr, f) / f);
}

const args = process.argv.slice(2);
if (args.includes("--brass")) {
  // the error at the table's points (E2 every 4 semitones), cancelled a little more each pass
  const bt = new Array(13).fill(0);
  for (let pass = 0; pass < 5; pass++) {
    const err = bt.map((_, i) => cents(3, 40 + 4 * i, 48000, bt));
    err.forEach((e, i) => { if (isFinite(e)) bt[i] += e; });
    console.log(`pass ${pass + 1}: worst ${Math.max(...err.map(Math.abs)).toFixed(1)} ct`);
  }
  console.log("BT = " + bt.map((v) => v.toFixed(1)).join(", "));
  process.exit(0);
}
let bad = 0;
for (const sr of args.includes("--check") ? [44100, 48000, 96000] : [48000]) {
  for (let m = 0; m < 4; m++) {
    const row = [];
    for (let n = RANGE[m][0]; n <= RANGE[m][1]; n += 3) {
      const c = cents(m, n, sr);
      if (!(Math.abs(c) <= 12)) bad++;
      row.push(`${n}:${isFinite(c) ? c.toFixed(0) : "silent"}`);
    }
    console.log(`${sr} ${NAMES[m].padEnd(9)} ${row.join(" ")}`);
  }
}
if (args.includes("--check") && bad) { console.error(`tune_wind: ${bad} notes off by more than 12 cents`); process.exit(1); }
