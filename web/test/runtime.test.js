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
