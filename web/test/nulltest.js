// Null test: every translated effect against the original JSFX in ysfx, sample by sample.
// Usage: node web/test/nulltest.js [plugin.jsfx ...]   (needs tests/run.sh to have built ysfx)
// For each effect: defaults, every slider at min and at max, and seeded random settings,
// at 44.1 and 96 kHz. Passes when the largest difference stays below 1e-6 (-120 dB).
// Instruments (and anything else that reads MIDI) also get the same MIDI notes in both, and
// what they send (midisend) must be the same messages at the same frames. MIDI effects
// (category MIDI) get a richer stream: overlapping notes on three channels, repeated note-ons,
// velocity-0 note-ons, pressure, pitch bend, sustain, program changes and all notes off.
"use strict";
var fs = require("fs"), path = require("path"), os = require("os");
var cp = require("child_process");
var T = require("../tools/eel2js.js");
var R = require("../runtime/eel-rt.js");
var HOST = require("../runtime/null-fx.js");

var root = path.resolve(__dirname, "../..");
var refBin = path.join(root, "tests/.build/render_ref");
var LIMIT = +(process.env.NULL_LIMIT || 1e-6);
var SECONDS = 1.5, BLOCK = 256;

// MT19937 with EEL2's fixed seed (0x4141f00d), so rand() matches a fresh ysfx process
function MT(seed) {
  var mt = new Uint32Array(624), mti = 625;
  function init(s) { mt[0] = s >>> 0; for (mti = 1; mti < 624; mti++) { var p = mt[mti - 1] ^ (mt[mti - 1] >>> 30); mt[mti] = (((((p & 0xffff0000) >>> 16) * 1812433253) << 16) + (p & 0x0000ffff) * 1812433253 + mti) >>> 0; } }
  function next() {
    var y, kk, mag = [0, 0x9908b0df];
    if (mti >= 624) {
      if (mti === 625) init(0x4141f00d);
      for (kk = 0; kk < 624 - 397; kk++) { y = (mt[kk] & 0x80000000) | (mt[kk + 1] & 0x7fffffff); mt[kk] = mt[kk + 397] ^ (y >>> 1) ^ mag[y & 1]; }
      for (; kk < 623; kk++) { y = (mt[kk] & 0x80000000) | (mt[kk + 1] & 0x7fffffff); mt[kk] = mt[kk + (397 - 624)] ^ (y >>> 1) ^ mag[y & 1]; }
      y = (mt[623] & 0x80000000) | (mt[0] & 0x7fffffff); mt[623] = mt[396] ^ (y >>> 1) ^ mag[y & 1];
      mti = 0;
    }
    y = mt[mti++];
    y ^= (y >>> 11); y ^= (y << 7) & 0x9d2c5680; y ^= (y << 15) & 0xefc60000; y ^= (y >>> 18);
    return y >>> 0;
  }
  if (seed != null) init(seed);
  return function () { return next() * (1.0 / 4294967295); };
}

// deterministic test signal (same idea as tests/jsfx_test.cpp): two tones, noise, dynamics
function signal(sr, frames, nin) {
  var seed = 12345;
  function rnd() { seed = (seed * 1103515245 + 12345) >>> 0; return seed / 4294967296 * 2 - 1; }
  var ch = [];
  for (var c = 0; c < nin; c++) ch.push(new Float64Array(frames));
  for (var i = 0; i < frames; i++) {
    var t = i / sr, env = (t % 0.5) < 0.25 ? 1 : 0.3;
    if (nin > 0) ch[0][i] = 0.25 * env * (Math.sin(2 * Math.PI * 220 * t) + 0.3 * rnd());
    if (nin > 1) ch[1][i] = 0.25 * env * (Math.sin(2 * Math.PI * 330 * t) + 0.3 * rnd());
    var tk = t % 0.25, sc = 0.8 * Math.exp(-tk * 20) * Math.sin(2 * Math.PI * 60 * tk);
    for (var c2 = 2; c2 < nin; c2++) ch[c2][i] = sc;
  }
  return ch;
}

function presets(FX) {
  var list = [{ name: "defaults", v: {} }];
  var vis = FX.sliders.filter(function (s) { return !s.path; });
  list.push({ name: "min", v: fromEach(vis, function (s) { return s.min; }) });
  list.push({ name: "max", v: fromEach(vis, function (s) { return s.max; }) });
  var seed = 42;
  function rnd() { seed = (seed * 69069 + 1) >>> 0; return seed / 4294967296; }
  for (var k = 0; k < 3; k++) {
    list.push({ name: "random" + (k + 1), v: fromEach(vis, function (s) {
      var v = s.min + rnd() * (s.max - s.min);
      if (s.inc > 0) v = s.min + Math.round((v - s.min) / s.inc) * s.inc;
      return v;
    }) });
  }
  return list;
}
function fromEach(list, f) { var o = {}; list.forEach(function (s) { o[s.i] = f(s); }); return o; }

// MIDI for the instruments: General MIDI drum notes (and some others) at uneven times, with
// changing velocities, a few note-offs and a note-on with velocity 0; [frame, status, d1, d2]
function midiEvents(sr, frames) {
  var notes = [36, 38, 39, 37, 42, 46, 41, 47, 56, 49, 35, 40, 44, 43, 50, 51, 60, 45, 48, 57];
  var out = [], k = 0;
  for (var at = Math.floor(0.013 * sr); at < frames; at += Math.floor((0.05 + 0.04 * (k % 5)) * sr), k++) {
    var n = notes[k % notes.length];
    out.push([at, k % 7 === 3 ? 0x80 : 0x90 | (k % 3), n, k % 11 === 5 ? 0 : 20 + (k * 37) % 108]);
  }
  return out;
}

// MIDI for the MIDI effects (as native/test/compare.cpp's testMidi); [frame, status, d1, d2]
function midiFxEvents(sr, frames) {
  var out = [], held = [], seed = 2024, allOff = false;
  function rnd(n) { seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0; return (seed >>> 8) % n; }
  var step = Math.floor(sr / 40);
  for (var at = Math.floor(sr * 0.003); at < frames; at += Math.floor(step / 2) + rnd(step)) {
    var ch = rnd(10) < 7 ? 0 : rnd(2) ? 1 : 9, kind = rnd(100), k;
    if (at > frames * 9 / 10 && !allOff) {
      allOff = true;
      out.push([at, 0xB0, 123, 0]);
      held = held.filter(function (h) { return h[0] !== 0; });
    }
    if (kind < 45 || !held.length) {
      var note = 24 + rnd(84);
      out.push([at, 0x90 | ch, note, 1 + rnd(127)]);
      held.push([ch, note]);
      if (rnd(4) === 0) { var n2 = 24 + rnd(84); out.push([at, 0x90 | ch, n2, 1 + rnd(127)]); held.push([ch, n2]); }
    } else if (kind < 80) {
      k = rnd(held.length);
      var zero = rnd(3) === 0;
      out.push([at, (zero ? 0x90 : 0x80) | held[k][0], held[k][1], zero ? 0 : rnd(128)]);
      held.splice(k, 1);
    } else if (kind < 84) { k = rnd(held.length); out.push([at, 0xA0 | held[k][0], held[k][1], rnd(128)]); }
    else if (kind < 88) out.push([at, 0xB0 | ch, 64, rnd(2) ? 127 : 0]);
    else if (kind < 92) out.push([at, 0xE0 | ch, rnd(128), rnd(128)]);
    else if (kind < 95) out.push([at, 0xD0 | ch, rnd(128), 0]);
    else if (kind < 97) out.push([at, 0xC0 | ch, rnd(128), 0]);
    else { k = rnd(held.length); out.push([at, 0x90 | held[k][0], held[k][1], 1 + rnd(127)]); }
  }
  return out;
}

// a sent message as text, the second data byte left out for two-byte messages
function midiText(e) {
  var two = (e[1] & 0xF0) === 0xC0 || (e[1] & 0xF0) === 0xD0;
  return e[0] + " " + e[1] + " " + e[2] + (two ? "" : " " + e[3]);
}

function runJs(FX, sr, ins, frames, sliders, midi) {
  R.random = MT();
  var fx = new HOST.Effect(FX, R, { srate: sr, blockSize: BLOCK, sliders: sliders, playing: true /* ysfx starts with play_state 1 */ });
  var nout = FX.outPins.length;
  var outs = [], sent = [];
  for (var c = 0; c < nout; c++) outs.push(new Float64Array(frames));
  for (var n = 0; n < frames; n += BLOCK) {
    var len = Math.min(BLOCK, frames - n);
    fx.setTime({ tempo: 120, playing: true, position: n / sr, beat: n / sr * 2, num: 4, denom: 4 });
    var ev = (midi || []).filter(function (e) { return e[0] >= n && e[0] < n + len; }).map(function (e) { return [e[0] - n, e[1], e[2], e[3]]; });
    fx.process(ins.map(function (a) { return a.subarray(n, n + len); }), outs.map(function (a) { return a.subarray(n, n + len); }), len, ins.length, ev);
    (fx.midiOut || []).forEach(function (e) { sent.push(midiText([n + e[0], e[1], e[2], e[3]])); });
  }
  return { outs: outs, pdc: fx.pdc(), midi: sent };
}

function runRef(file, sr, ins, frames, sliders, tmp, midi) {
  var inFile = path.join(tmp, "in.f64"), outFile = path.join(tmp, "out.f64");
  var buf = Buffer.alloc(ins.length * frames * 8);
  ins.forEach(function (a, c) { Buffer.from(a.buffer, a.byteOffset, a.byteLength).copy(buf, c * frames * 8); });
  fs.writeFileSync(inFile, buf);
  var args = [file, String(sr), String(BLOCK), String(frames), inFile, outFile];
  Object.keys(sliders).forEach(function (k) { args.push(k + "=" + sliders[k]); });
  var sentFile = path.join(tmp, "sent.txt");
  if (midi) {
    var midiFile = path.join(tmp, "midi.txt");
    fs.writeFileSync(midiFile, midi.map(function (e) { return e.join(" "); }).join("\n") + "\n");
    args.push("midi=" + midiFile, "midiout=" + sentFile);
  }
  var res = cp.execFileSync(refBin, args).toString().trim().split(" ");
  var nout = +res[1], raw = fs.readFileSync(outFile), outs = [];
  for (var c = 0; c < nout; c++) {
    var a = new Float64Array(frames);
    for (var i = 0; i < frames; i++) a[i] = raw.readDoubleLE((c * frames + i) * 8);
    outs.push(a);
  }
  var sent = midi ? fs.readFileSync(sentFile, "utf8").split("\n").filter(Boolean).map(function (l) {
    return midiText(l.split(" ").map(Number));
  }) : [];
  return { outs: outs, pdc: +res[2], midi: sent };
}

function main() {
  var files = process.argv.slice(2);
  if (!files.length) {
    var dir = path.join(root, "DATA/Effects/null_jsfx");
    files = fs.readdirSync(dir).filter(function (f) { return /\.jsfx$/.test(f); }).map(function (f) { return path.join(dir, f); })
      .concat(fs.readdirSync(path.join(dir, "gfx")).filter(function (f) { return /\.jsfx$/.test(f); }).map(function (f) { return path.join(dir, "gfx", f); }));
  }
  if (!fs.existsSync(refBin)) { console.error("build the reference renderer first: web/test/run.sh"); process.exit(2); }
  var tmp = fs.mkdtempSync(path.join(os.tmpdir(), "nulltest-"));
  var failed = 0, worst = [];
  files.forEach(function (file) {
    var name = path.basename(file), FX, err = null, maxDiff = 0, where = "", firstBad = "";
    try {
      var out = T.translate(file, { id: "__t" });
      (new Function(out.code))();
      FX = globalThis.NULLFX.__t;
      var nin = FX.inPins.length;
      [44100, 96000].forEach(function (sr) {
        var frames = Math.floor(sr * SECONDS);
        var ins = signal(sr, frames, nin);
        var midi = !FX.usesMidi ? null : FX.category === "MIDI" ? midiFxEvents(sr, frames) : midiEvents(sr, frames);
        presets(FX).forEach(function (p) {
          var js = runJs(FX, sr, ins, frames, p.v, midi), ref = runRef(file, sr, ins, frames, p.v, tmp, midi);
          if (Math.abs(js.pdc - ref.pdc) > 1e-9) { err = err || ("pdc " + js.pdc + " vs " + ref.pdc + " (" + p.name + " @" + sr + ")"); }
          // the MIDI sent: the same messages at the same frames
          for (var mi = 0; mi < Math.max(js.midi.length, ref.midi.length); mi++) {
            if (js.midi[mi] !== ref.midi[mi]) {
              err = err || ("MIDI sent differs at message " + mi + ": ref " + ref.midi[mi] + ", js " + js.midi[mi] + " (" + p.name + " @" + sr + ")");
              break;
            }
          }
          ref.outs.forEach(function (a, c) {
            var b = js.outs[c];
            for (var i = 0; i < a.length; i++) {
              var d = Math.abs(a[i] - b[i]);
              if (process.env.NULL_FIRST && d > 1e-9 && !firstBad) firstBad = p.name + " @" + sr + " ch" + c + " #" + i + " ref " + a[i] + " js " + b[i];
              if (!(d <= maxDiff)) { maxDiff = d !== d ? Infinity : d; where = p.name + " @" + sr + " ch" + c + " #" + i + " ref " + a[i] + " js " + b[i]; }
            }
          });
        });
      });
    } catch (e) { err = e.message.split("\n")[0]; }
    var ok = !err && maxDiff < LIMIT;
    if (!ok) failed++;
    console.log((ok ? "ok   " : "FAIL ") + name + "  max diff " + maxDiff.toExponential(2) + (err ? "  " + err : "") + (!ok && where ? "  at " + where : "") + (firstBad ? "  first " + firstBad : ""));
  });
  console.log("\n" + failed + " of " + files.length + " effects differ");
  process.exit(failed ? 1 : 0);
}
main();
