#!/usr/bin/env node
// Renders the loops of the live player on the plugin pages (assets/js/live.js) from NULL JSFX's
// own instruments, through the web versions in web/fx (the same code orOS Studio runs):
//   assets/audio/loops/band.mp3    drums (Drum Machine), bass (Bass) and chords (Keys)
//   assets/audio/loops/drums.mp3   the drums alone
//   assets/audio/loops/keys.mp3    the chords and the bass, no drums
// 4 bars at 100 BPM (9.6 s, A minor: Am F C G), cut from the second to the sixth bar of a longer
// render so the tails of the bar before are in it and the loop joins without a gap. Each loop
// is set to the same RMS and kept under -1 dBFS. Needs ffmpeg.
// Usage: node tools/render_loops.js     (again after changing an instrument or this file)
"use strict";
const fs = require("fs");
const path = require("path");
const os = require("os");
const { execFileSync } = require("child_process");

const root = path.resolve(__dirname, "..");
global.NULLFX = {};
const R = require(path.join(root, "web/runtime/eel-rt.js"));
const HOST = require(path.join(root, "web/runtime/null-fx.js"));
for (const id of ["drum_machine", "bass", "keys"]) require(path.join(root, "web/fx", `${id}.js`));

const SR = 48000, BLOCK = 128, BPM = 100;
const BAR = 60 / BPM * 4, BARS = 6, FRAMES = Math.round(BAR * BARS * SR);
const OUT = path.join(root, "assets/audio/loops");

// a fixed seed, so every render is the same file
let seed = 12345;
R.random = () => { seed = (seed * 16807) % 2147483647; return seed / 2147483647; };

function slider(FX, name, value) {
  const s = FX.sliders.find((x) => x.name === name || x.name.startsWith(`${name} (`));
  if (!s) throw new Error(`${FX.id}: no slider ${name}`);
  const v = typeof value === "string" ? s.enums.indexOf(value) : value;
  if (v < 0) throw new Error(`${FX.id}: ${name} has no ${value}`);
  return [s.i, v];
}

/** renders one instrument: MIDI as [seconds, status, data1, data2], stereo out */
function render(id, settings, notes) {
  const FX = NULLFX[id];
  const sliders = Object.fromEntries(Object.entries(settings).map(([k, v]) => slider(FX, k, v)));
  const fx = new HOST.Effect(FX, R, { srate: SR, blockSize: BLOCK, sliders, playing: true });
  const outs = FX.outPins.map(() => new Float64Array(FRAMES));
  const ev = notes.map(([t, a, b, c]) => [Math.round(t * SR), a, b, c]).sort((x, y) => x[0] - y[0]);
  for (let n = 0; n < FRAMES; n += BLOCK) {
    const len = Math.min(BLOCK, FRAMES - n);
    fx.setTime({ tempo: BPM, playing: true, position: n / SR, beat: n / SR * BPM / 60, num: 4, denom: 4 });
    const block = ev.filter((e) => e[0] >= n && e[0] < n + len).map((e) => [e[0] - n, e[1], e[2], e[3]]);
    fx.process([], outs.map((a) => a.subarray(n, n + len)), len, 0, block);
  }
  return [outs[0], outs[1]];
}

const CHORDS = [[57, 60, 64], [53, 57, 60], [48, 55, 64], [55, 59, 62]]; // Am F C G
const ROOTS = [33, 29, 36, 31];
const beat = 60 / BPM;
function note(list, t, n, vel, len, ch = 0) { list.push([t, 0x90 | ch, n, vel], [t + len, 0x80 | ch, n, 0]); }

const bassNotes = [], keyNotes = [];
for (let bar = 0; bar < BARS; bar++) {
  const t0 = bar * BAR, k = (bar + 3) % 4; // the loop (bars 2 to 5) starts on Am
  // bass: root on the beat, an octave up on the offbeat of beat 2 and 4, a passing note at the end
  [[0, 0, 0.9], [1, 0, 0.4], [1.5, 12, 0.4], [2, 0, 0.9], [3, 0, 0.4], [3.5, k === 3 ? 2 : 7, 0.4]]
    .forEach(([b, d, len]) => note(bassNotes, t0 + b * beat, ROOTS[k] + d, b % 2 ? 84 : 104, len * beat));
  // keys: the chord held on beat 1, pushed again on the "and" of 2
  CHORDS[k].forEach((n, i) => {
    note(keyNotes, t0 + i * 0.012, n, 80, 1.4 * beat);
    note(keyNotes, t0 + 1.5 * beat + i * 0.01, n, 66, 2.3 * beat);
  });
}

const drums = render("drum_machine", { Pattern: "Boom Bap", "Swing": 56 }, []);
const bass = render("bass", { Oscillator: "Saw", Cutoff: 900, "Env Amount": 45 }, bassNotes);
const keys = render("keys", { Model: "Tine EP", Chorus: 30, "Tremolo Depth": 20 }, keyNotes);

function mix(parts) {
  const L = new Float64Array(FRAMES), Rt = new Float64Array(FRAMES);
  for (const [[a, b], g] of parts) for (let i = 0; i < FRAMES; i++) { L[i] += a[i] * g; Rt[i] += b[i] * g; }
  const from = Math.round(BAR * SR), to = Math.round(BAR * 5 * SR);
  return [L.subarray(from, to), Rt.subarray(from, to)];
}

function write([L, Rt], name) {
  let e = 0, pk = 0;
  for (let i = 0; i < L.length; i++) { e += L[i] * L[i] + Rt[i] * Rt[i]; pk = Math.max(pk, Math.abs(L[i]), Math.abs(Rt[i])); }
  const rms = Math.sqrt(e / (2 * L.length));
  if (!(rms > 1e-4) || !isFinite(pk)) throw new Error(`${name}: silent or broken (rms ${rms})`);
  const g = Math.min(Math.pow(10, -18 / 20) / rms, Math.pow(10, -1 / 20) / pk);
  const raw = Buffer.alloc(L.length * 8);
  for (let i = 0; i < L.length; i++) { raw.writeFloatLE(L[i] * g, i * 8); raw.writeFloatLE(Rt[i] * g, i * 8 + 4); }
  const tmp = path.join(fs.mkdtempSync(path.join(os.tmpdir(), "loops-")), "a.f32");
  fs.writeFileSync(tmp, raw);
  fs.mkdirSync(OUT, { recursive: true });
  const dest = path.join(OUT, `${name}.mp3`);
  execFileSync("ffmpeg", ["-v", "error", "-y", "-f", "f32le", "-ar", String(SR), "-ac", "2", "-i", tmp,
    "-c:a", "libmp3lame", "-b:a", "128k", "-map_metadata", "-1", "-fflags", "+bitexact", "-flags:a", "+bitexact", dest]);
  console.log(`render_loops: ${path.relative(root, dest)} (${(L.length / SR).toFixed(1)} s, gain ${(20 * Math.log10(g)).toFixed(1)} dB)`);
}

write(mix([[drums, 1], [bass, 0.55], [keys, 0.5]]), "band");
write(mix([[drums, 1]]), "drums");
write(mix([[bass, 0.55], [keys, 0.6]]), "keys");
