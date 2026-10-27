// Checks of the web runtime that the null test (ysfx vs JS) cannot cover: files handed over
// by the page, the @gfx interface driving a slider, and memory edge cases.
// Run: node --test web/test/*.test.js
"use strict";
var test = require("node:test");
var assert = require("node:assert");
var path = require("path");
var R = require("../runtime/eel-rt.js");
var HOST = require("../runtime/null-fx.js");
var GFX = require("../runtime/null-gfx.js");

var FXDIR = path.join(__dirname, "../fx");
function fx(id) { require(path.join(FXDIR, id + ".js")); return globalThis.NULLFX[id]; }

function noise(n, seed) {
  var out = new Float64Array(n), s = seed >>> 0;
  for (var i = 0; i < n; i++) { s = (Math.imul(s, 1664525) + 1013904223) >>> 0; out[i] = (s / 4294967296 - 0.5) * 0.5; }
  return out;
}

function run(e, ins, n) {
  var outs = [new Float64Array(n), new Float64Array(n)];
  for (var o = 0; o < n; o += 256) {
    var c = Math.min(256, n - o);
    e.process([ins[0].subarray(o, o + c), ins[1].subarray(o, o + c)], [outs[0].subarray(o, o + c), outs[1].subarray(o, o + c)], c, 2);
  }
  return outs;
}

test("Convolution reads an IR file handed over by the page", function () {
  var sr = 48000, n = sr, FX = fx("convolution");
  // IR: one impulse, so the wet signal is the input delayed by the effect's latency
  var ir = new Float64Array(4800); ir[0] = 1;
  var e = new HOST.Effect(FX, R, { srate: sr, blockSize: 256, playing: true,
    sliders: { 1: 5, 2: 1, 5: 20, 6: 20000, 7: 100, 8: 0 },
    files: { slider2: { channels: 1, srate: sr, data: ir } } });
  var read = 0, fileMem = e.H.fileMem;
  e.H.fileMem = function () { var r = fileMem.apply(this, arguments); read += r; return r; };
  var x = [noise(n, 1), noise(n, 2)];
  var y = run(e, x, n), d = e.pdc();
  assert.strictEqual(d, 1024);
  assert.strictEqual(read, 4800, "the whole IR was read");
  // wet = input delayed, apart from the 20 Hz low cut and 20 kHz high cut that stay in the
  // wet path (about 1% of white noise's energy)
  var peak = 0;
  for (var j = 0; j < n; j++) peak = Math.max(peak, Math.abs(y[0][j]));
  assert.ok(peak > 0, "the wet path is not silent");
  {
    var err = 0, ref = 0;
    for (var i = sr / 10; i < n; i++) { var t = x[0][i - d]; err += (y[0][i] - t) * (y[0][i] - t); ref += t * t; }
    assert.ok(err / ref < 0.03, "wet = input delayed (relative error " + (err / ref) + ")");
  }
});

// a canvas stand-in: records nothing, measures text as 7 px per character
function fakeCanvas() {
  var handlers = {};
  var ctx = {
    measureText: function (s) { return { width: s.length * 7, fontBoundingBoxAscent: 97, fontBoundingBoxDescent: 24 }; },
    setTransform: function () {}, fillRect: function () {}, strokeRect: function () {}, beginPath: function () {},
    moveTo: function () {}, lineTo: function () {}, arcTo: function () {}, arc: function () {}, closePath: function () {},
    stroke: function () {}, fill: function () {}, fillText: function () {}, createLinearGradient: function () { return { addColorStop: function () {} }; }
  };
  return {
    width: 0, height: 0, clientWidth: 360, clientHeight: 240, style: {},
    getContext: function () { return ctx; },
    addEventListener: function (k, f) { handlers[k] = f; }, removeEventListener: function (k) { delete handlers[k]; },
    getBoundingClientRect: function () { return { left: 0, top: 0 }; },
    setPointerCapture: function () {},
    fire: function (k, e) { e.preventDefault = function () {}; handlers[k](e); }
  };
}

test("dragging the slider in a GFX interface moves slider1 and tells the page", function () {
  globalThis.NULLRT = R; globalThis.NULLFXHost = HOST;
  var FX = fx("tilt"), cv = fakeCanvas(), got = [];
  var view = new GFX.View(FX, cv, { srate: 48000, sliders: { 1: 0 }, onSlider: function (i, v) { got.push([i, v]); } });
  view.draw();
  // the One Slider track runs from design x 74 to 286 at y 210 (360 x 240 design, scale 1)
  cv.fire("pointerdown", { clientX: 180, clientY: 210, button: 0 }); view.draw();
  cv.fire("pointermove", { clientX: 240, clientY: 210, button: 0 }); view.draw();
  cv.fire("pointerup", { clientX: 240, clientY: 210, button: 0 }); view.draw();
  assert.ok(got.length > 0, "onSlider called");
  var last = got[got.length - 1];
  assert.strictEqual(last[0], 1);
  assert.ok(last[1] > 40 && last[1] <= 100, "slider1 moved right: " + last[1]);
  assert.strictEqual(view.fx.inst.getSlider(1), last[1]);
  // the mirror re-ran @slider: tilt's high-shelf gain follows the new value
  assert.ok(Math.abs(view.fx.inst.getVar("g_high") - Math.pow(10, last[1] / 100 * 6 / 20)) < 1e-12);
  view.destroy();
});

test("the wheel steps the slider by one", function () {
  var FX = fx("tilt"), cv = fakeCanvas(), got = [];
  var view = new GFX.View(FX, cv, { srate: 48000, sliders: { 1: 10 }, onSlider: function (i, v) { got.push(v); } });
  view.draw();
  cv.fire("pointermove", { clientX: 180, clientY: 210 });
  cv.fire("wheel", { clientX: 180, clientY: 210, deltaY: -100 });
  view.draw();
  assert.deepStrictEqual(got, [11]);
});

test("the preset box in the GFX header steps through and lists the factory presets", function () {
  var FX = fx("tilt"), cv = fakeCanvas(), got = [];
  var view = new GFX.View(FX, cv, { srate: 48000, sliders: { 1: 0 }, onSlider: function (i, v) { got.push(v); } });
  function click(x, y) {
    cv.fire("pointermove", { clientX: x, clientY: y, button: 0 });
    cv.fire("pointerdown", { clientX: x, clientY: y, button: 0 }); view.draw();
    cv.fire("pointerup", { clientX: x, clientY: y, button: 0 }); view.draw(); view.draw();
  }
  view.draw(); view.draw();
  // the box spans design x 196..346, y 21..37 (360 x 240 design, scale 1); arrows 22 wide
  click(335, 29);  // next: Default -> "Vintage Dark" (-70)
  assert.strictEqual(view.fx.inst.getSlider(1), -70);
  click(205, 29);  // previous: back to Default (0)
  assert.strictEqual(view.fx.inst.getSlider(1), 0);
  click(270, 29);  // the name opens the list; rows of 15 from y 44
  click(250, 44 + 3 * 15 + 7);  // fourth preset, "Mastering Dark Nudge" (-15)
  assert.strictEqual(view.fx.inst.getSlider(1), -15);
  assert.deepStrictEqual(got, [-70, 0, -15]);
  view.destroy();
});

test("freembuf(0) clears block 0 without detaching it", function () {
  var m = R.memory(), b0 = m.b[0];
  m.w(5, 3); m.w(70000, 4);
  m.free(0);
  assert.strictEqual(m.b[0], b0);
  assert.strictEqual(m.r(5), 0);
  assert.strictEqual(m.r(70000), 0);
  m.w(5, 2);
  assert.strictEqual(b0[5], 2);
});

test("memory reads and writes outside the 8M slots share one scratch slot", function () {
  var m = R.memory();
  m.w(m.i(-5), 7);
  assert.strictEqual(m.r(-5), 7);
  assert.strictEqual(m.r(9e6), 7);
});

test("options:maxmem raises the memory to 32M slots, as in EEL2", function () {
  var m = R.memory(33554432);
  m.w(m.i(9e6), 5);
  assert.strictEqual(m.r(9e6), 5);
  assert.strictEqual(m.scratch, 0);
  m.w(m.i(33554432), 6); // past the end: the scratch slot
  assert.strictEqual(m.r(4e7), 6);
  m.set(33554430, 1, 10); // clipped at the end
  assert.strictEqual(m.r(33554431), 1);
  // the translated effects ask for it from their header
  assert.ok(/R\.memory\(33554432\)/.test(fx("slicer").create.toString()));
  assert.ok(/R\.memory\(\)/.test(fx("tilt").create.toString()));
});

// ---- instruments: MIDI, transport, @serialize, the interface's shared state ----------------

// runs an instrument like the worklet does: events from a Sched, block by block
function play(e, sched, sr, n, t0) {
  var out = [new Float64Array(n), new Float64Array(n)], BLK = 128, T = { playing: false, position: 0 };
  for (var o = 0; o < n; o += BLK) {
    var c = Math.min(BLK, n - o), due = sched.block(t0 + o / sr, c, sr);
    if (due.time) T = { playing: due.time.playing, position: due.time.position };
    e.setTime({ tempo: 120, playing: T.playing, position: T.position, beat: T.position * 2 });
    e.process([], [out[0].subarray(o, o + c), out[1].subarray(o, o + c)], c, 0, due.midi);
    if (T.playing) T.position += c / sr;
  }
  return out;
}
function firstSound(y) { for (var i = 0; i < y[0].length; i++) if (y[0][i] !== 0 || y[1][i] !== 0) return i; return -1; }

test("Sched hands MIDI and transport changes over at their samples", function () {
  var s = new HOST.Sched(), sr = 48000;
  s.addMidi([[1.0 + 300 / sr, 0x90, 36, 100], [0.5, 0x90, 38, 90], [1.0 + 10 / sr, 0x80, 36, 0]]);
  s.addTime({ at: 1.0 + 64 / sr, playing: true, position: 2 });
  var b = s.block(1.0, 128, sr);
  assert.deepStrictEqual(b.midi, [[0, 0x90, 38, 90], [10, 0x80, 36, 0]], "late events play at the first sample, in time order");
  assert.ok(Math.abs(b.time.position - (2 - 64 / sr)) < 1e-12, "the position is moved back to the block's start");
  assert.strictEqual(b.time.playing, true);
  b = s.block(1.0 + 128 / sr, 128, sr);
  assert.strictEqual(b.midi, null);
  b = s.block(1.0 + 256 / sr, 128, sr);
  assert.deepStrictEqual(b.midi, [[44, 0x90, 36, 100]]);
});

test("Drum Machine: a MIDI note sounds from its sample on, the pattern starts with the transport", function () {
  var FX = fx("drum_machine"), sr = 48000, s = new HOST.Sched();
  assert.ok(FX.usesMidi && FX.serialize && FX.inPins.length === 0);
  // Pattern Off: only MIDI
  var e = new HOST.Effect(FX, R, { srate: sr, blockSize: 128, sliders: { 1: 0 } });
  s.addMidi([[300 / sr, 0x99, 36, 127]]);
  var y = play(e, s, sr, 4800, 0);
  assert.strictEqual(firstSound(y), 300, "the kick starts at the note's sample");
  // House (the default) plays the kick on step 1 when the transport starts there
  e = new HOST.Effect(FX, R, { srate: sr, blockSize: 128 });
  s = new HOST.Sched();
  s.addTime({ at: 1000 / sr, playing: true, position: 0 });
  y = play(e, s, sr, 9600, 0);
  assert.strictEqual(firstSound(y), 1000, "step 1 at the transport start");
});

test("Drum Machine: the User pattern goes through @serialize", function () {
  var FX = fx("drum_machine"), sr = 48000;
  var a = new HOST.Effect(FX, R, { srate: sr });
  var st = a.getState();
  assert.strictEqual(st.length, 171, "version, then 170 values");
  assert.deepStrictEqual(st.slice(1, 17), [1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0], "the basic beat's kick");
  st[1 + 3] = 2;                 // an accent on kick step 4
  st[1 + 160] = 12;              // kick length 12
  var b = new HOST.Effect(FX, R, { srate: sr });
  b.setState(st);
  assert.strictEqual(b.inst.mem.r(3), 2);
  assert.strictEqual(b.inst.mem.r(160), 12);
  // @init again (transport start) keeps it, as in REAPER
  b.init();
  assert.deepStrictEqual(b.getState(), st);
});

test("Drum Machine interface: a step edit is state to save, a name click a voice to hear", function () {
  globalThis.NULLRT = R; globalThis.NULLFXHost = HOST;
  var FX = fx("drum_machine"), cv = fakeCanvas(), got = [];
  cv.clientWidth = 640; cv.clientHeight = 580;
  var view = new GFX.View(FX, cv, { srate: 48000, onSlider: function (i, v) { got.push([i, v]); } });
  function click(x, y) {
    cv.fire("pointermove", { clientX: x, clientY: y, button: 0 });
    cv.fire("pointerdown", { clientX: x, clientY: y, button: 0 }); view.draw();
    cv.fire("pointerup", { clientX: x, clientY: y, button: 0 }); view.draw();
  }
  view.draw();
  assert.strictEqual(view.takeState(), null, "nothing changed yet");
  click(171, 89);                // kick, step 3 (cell x 112 + 2 * 27, row y 82)
  assert.deepStrictEqual(got[got.length - 1], [1, 1], "an edit of a factory pattern switches to User");
  var st = view.takeState();
  assert.ok(st, "the edit is state to save");
  assert.strictEqual(st[1 + 2], 1, "a hit on kick step 3");
  assert.strictEqual(st[1 + 4], 1, "the rest is a copy of House");
  assert.strictEqual(view.takeState(), null, "taken once");
  click(50, 106);                // the SNARE name
  assert.deepStrictEqual(view.takePokes(), { dm_preview: 2 });
  assert.strictEqual(view.takePokes(), null, "taken once");
  // flash: what the audio hit since the last peek lights the name, zeros leave the page's value
  view.setLive({ live: new Float64Array(FX.live.length), flash: [{ base: view.fx.inst.getVar("hit"), data: Float64Array.from([0, 0, 0.7, 0, 0, 0, 0, 0, 0, 0]) }] });
  assert.strictEqual(view.fx.inst.mem.r(5052), 0.7);
  view.setState(st.map(function (v, i) { return i === 1 ? 0 : v; }));
  assert.strictEqual(view.fx.inst.mem.r(0), 0, "a saved state shows");
  assert.strictEqual(view.takeState(), null, "and is not state to save again");
  view.destroy();
});
