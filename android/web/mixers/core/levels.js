// NULL Remote, Mixers: value laws and meter blobs shared by the X AIR and X32/M32 families.
// - Fader and send levels: OSC float 0..1 <-> dB with the four-segment law of these consoles
//   (1.0 = +10 dB, 0.75 = 0 dB, 0.5 = -10 dB, 0.25 = -30 dB, 0.0625 = -60 dB, 0 = -inf), quantised
//   to the 1024 fader steps the console stores.
// - Pan: float 0..1 <-> -100..+100.
// - Meter blobs: an int32 value count, then int16 values in 1/256 dB. The byte order of the blob
//   payload is detected from the count (the X32 sends little-endian; unverified on X AIR).
// NULL JSFX • https://nulljsfx.tech • MIT License
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.NullLevels = api;
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  const STEPS = 1023;

  function faderToDb(f) {
    if (!(f > 0)) return -Infinity;
    if (f >= 0.5) return f * 40 - 30;
    if (f >= 0.25) return f * 80 - 50;
    if (f >= 0.0625) return f * 160 - 70;
    return f * 480 - 90;
  }

  function dbToFader(db) {
    let f;
    if (!(db > -90)) f = 0;
    else if (db >= -10) f = (db + 30) / 40;
    else if (db >= -30) f = (db + 50) / 80;
    else if (db >= -60) f = (db + 70) / 160;
    else f = (db + 90) / 480;
    return quantise(f);
  }

  function quantise(f) {
    f = Math.min(1, Math.max(0, f));
    return Math.round(f * STEPS) / STEPS;
  }

  function formatDb(db) {
    if (db === -Infinity || db <= -89.95) return '-∞';
    const r = Math.round(db * 10) / 10;
    return (r > 0 ? '+' : '') + r.toFixed(1);
  }

  const panToPercent = (p) => Math.round((p * 2 - 1) * 100);
  const percentToPan = (pc) => Math.min(1, Math.max(0, (pc / 100 + 1) / 2));
  function formatPan(p) {
    const pc = panToPercent(p);
    return pc === 0 ? 'C' : pc < 0 ? 'L' + -pc : 'R' + pc;
  }

  // Parse a meter blob into dB values (Float32Array). Returns null when it does not parse.
  function parseMeterBlob(blob) {
    if (!blob || blob.length < 4) return null;
    const dv = new DataView(blob.buffer, blob.byteOffset, blob.byteLength);
    const room = (blob.length - 4) >> 1;
    let little = true;
    let count = dv.getInt32(0, true);
    if (count < 0 || count > room) {
      count = dv.getInt32(0, false);
      little = false;
      if (count < 0 || count > room) return null;
    }
    const out = new Float32Array(count);
    for (let i = 0; i < count; i++) out[i] = dv.getInt16(4 + 2 * i, little) / 256;
    return out;
  }

  // Build a meter blob (the simulator and tests use this).
  function buildMeterBlob(dbValues, little = true) {
    const b = new Uint8Array(4 + 2 * dbValues.length);
    const dv = new DataView(b.buffer);
    dv.setInt32(0, dbValues.length, little);
    dbValues.forEach((db, i) => dv.setInt16(4 + 2 * i, Math.max(-32768, Math.min(32767, Math.round(db * 256))), little));
    return b;
  }

  // Meter level 0..1 for drawing: -60 dB .. 0 dB mapped linearly in dB.
  const meterPos = (db) => Math.min(1, Math.max(0, (db + 60) / 60));

  return { faderToDb, dbToFader, quantise, formatDb, panToPercent, percentToPan, formatPan, parseMeterBlob, buildMeterBlob, meterPos };
});
