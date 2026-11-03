// NULL Remote, Mixers: the Yamaha CL, QL, TF and DM7 profile (beta). Yamaha's remote control
// protocol (RCP): text lines over TCP port 49280 (core/rcp.js). Yamaha's own parameter list is
// not public; the addresses, ranges and model sizes here are read from the parameter lists of the
// open Companion module for Yamaha RCP (bitfocus/companion-module-yamaha-rcp, MIT License:
// "CLQL Parameters-1", "TF Parameters-1", "DM7 Parameters-2"). Not checked on a console
// (Bible C25).
//   MIXER:Current/InCh/Fader/Level X 0     level in 0.01 dB: -32768 = -inf, 1000 = +10 dB
//   MIXER:Current/InCh/Fader/On X 0        1 = on (not muted)
//   MIXER:Current/InCh/ToSt/Pan X 0        -63 (left) .. 63 (right)
//   MIXER:Current/InCh/ToMix/Level X Y     the send of channel X to mix Y
//   MIXER:Current/InCh/Label/Name X 0      "text"; .../Label/Color X 0 "Blue" (colour names)
//   The same for StInCh (stereo inputs, L and R as X = 2k and 2k+1), FxRtnCh (TF: FX returns, L
//   and R), Mix (masters: Fader, Label), St (X 0: the stereo master), DCA (Fader, Label).
//   Mute groups: MIXER:Current/MuteMaster/On X 0 (CL, QL, TF; 1 = muting), on the DM7
//   MIXER:Current/MuteGrpCtrl/On X 0.
// A stereo strip shows its L channel and sets both L and R (the console may link them itself).
// Not covered yet: meters, solo (cue), the mono master, matrices, a mix's send on/off (the level
// alone is shown), scenes, Rivage and DM3.
// NULL JSFX • https://nulljsfx.tech • MIT License
(function (root, factory) {
  const levels = typeof module === 'object' && module.exports ? require('../core/levels.js') : root.NullLevels;
  const api = factory(levels);
  if (typeof module === 'object' && module.exports) module.exports = api;
  else (root.NullMixerProfiles = root.NullMixerProfiles || {}).yamaha = api;
})(typeof self !== 'undefined' ? self : this, function (Levels) {
  'use strict';

  const M = 'MIXER:Current/';
  // what each console has (mono inputs, stereo inputs, FX returns, mixes, DCAs, mute groups)
  const CLQL = { mix: 24, dca: 16, mg: 8, fxr: 0, mute: 'MuteMaster/On' };
  const TF = { in: 40, st: 2, fxr: 2, mix: 20, dca: 8, mg: 6, mute: 'MuteMaster/On' };
  const DM7 = { st: 0, fxr: 0, mix: 48, dca: 24, mg: 12, mute: 'MuteGrpCtrl/On' };
  const MODELS = {
    CL5: Object.assign({ in: 72, st: 8 }, CLQL),
    CL3: Object.assign({ in: 64, st: 8 }, CLQL),
    CL1: Object.assign({ in: 48, st: 8 }, CLQL),
    QL5: Object.assign({ in: 64, st: 8 }, CLQL),
    QL1: Object.assign({ in: 32, st: 8 }, CLQL),
    TF5: TF, TF3: TF, TF1: TF, 'TF-RACK': TF,
    DM7: Object.assign({ in: 120 }, DM7),
    'DM7 Compact': Object.assign({ in: 72 }, DM7),
  };
  // the console's colour names (the TF has SkyBlue and Pink where the others have Cyan and Magenta)
  const COLORS = ['Off', 'Blue', 'Orange', 'Yellow', 'Purple', 'Cyan', 'Magenta', 'Red', 'Green', 'SkyBlue', 'Pink'];
  const HEX = ['#3a3a3a', '#3c64c8', '#d2823c', '#c8b43c', '#8c50b4', '#50a0c8', '#c850a0', '#c83c3c', '#3c9c50', '#50a0c8', '#c850a0'];
  const LIGHT = new Set([2, 3, 5, 6, 8, 9, 10]); // dark text on these

  // the console's model from devinfo productname ("CL5", "QL1", "TF-RACK", "DM7 Compact" ...)
  function modelOf(product) {
    const p = String(product || '').trim();
    if (MODELS[p]) return p;
    const u = p.toUpperCase();
    if (/^DM7/.test(u)) return /COMPACT|CMP/.test(u) ? 'DM7 Compact' : 'DM7';
    if (/^TF.*RACK/.test(u)) return 'TF-RACK';
    for (const m of ['CL5', 'CL3', 'CL1', 'QL5', 'QL1', 'TF5', 'TF3', 'TF1']) if (u.startsWith(m)) return m;
    if (/^CL/.test(u)) return 'CL5';
    if (/^QL/.test(u)) return 'QL5';
    if (/^TF/.test(u)) return 'TF5';
    return 'CL5';
  }

  const map = new Map(); // path -> {kind, addr, x, y, pair}

  function layoutFor(model) {
    model = MODELS[model] ? model : 'CL5';
    const C = MODELS[model];
    map.clear();
    // kind: level | on | pan | name | color | group; pair: also X + 1 (a stereo strip's R)
    const add = (path, kind, addr, x, y = 0, pair = false) => { map.set(path, { kind, addr: M + addr, x, y, pair }); return path; };
    const mixes = [];
    for (let m = 1; m <= C.mix; m++) mixes.push(m);

    function strip(kind, index, label, cat, x, opts) {
      const base = '/' + kind + '/' + index;
      const pair = !!opts.pair;
      const s = {
        id: kind + index, kind, index, label,
        fader: add(base + '/lvl', 'level', cat + '/Fader/Level', x, 0, pair),
        on: add(base + '/on', 'on', cat + '/Fader/On', x, 0, pair),
        pan: opts.pan ? add(base + '/pan', 'pan', cat + '/ToSt/Pan', x) : null,
        name: add(base + '/name', 'name', cat + '/Label/Name', x),
        color: add(base + '/col', 'color', cat + '/Label/Color', x),
        muteGroups: null, solo: null, meter: null,
        sends: opts.sends ? {} : null,
      };
      if (opts.sends) for (const m of mixes) s.sends['mix' + m] = add(base + '/mix/' + m, 'level', cat + '/ToMix/Level', x, m - 1, pair);
      return s;
    }

    const inputs = [];
    for (let i = 1; i <= C.in; i++) inputs.push(strip('ch', i, 'Ch ' + i, 'InCh', i - 1, { pan: true, sends: true }));
    for (let i = 1; i <= C.st; i++) inputs.push(strip('st', i, 'ST In ' + i, 'StInCh', 2 * (i - 1), { pair: true, sends: true }));
    const returns = [];
    for (let r = 1; r <= C.fxr; r++) returns.push(strip('fxr', r, 'FX Rtn ' + r, 'FxRtnCh', 2 * (r - 1), { pair: true, sends: true }));
    const targets = mixes.map((m) => ({ id: 'mix' + m, label: 'Mix ' + m, slot: String(m), master: strip('mixm', m, 'Mix ' + m, 'Mix', m - 1, {}) }));
    const main = strip('stm', 1, 'Stereo', 'St', 0, {});
    const dcas = [];
    for (let d = 1; d <= C.dca; d++) dcas.push(strip('dca', d, 'DCA ' + d, 'DCA', d - 1, {}));
    const muteGroups = [];
    for (let g = 1; g <= C.mg; g++) muteGroups.push({ id: 'mg' + g, label: 'Mute ' + g, path: add('/mgrp/' + g, 'group', C.mute, g - 1), bit: 1 << (g - 1) });

    const params = [];
    const p = (path, type) => { if (path) params.push({ path, type }); };
    for (const s of inputs.concat(returns, targets.map((t) => t.master), [main], dcas)) {
      p(s.fader, 'f'); p(s.on, 'i'); p(s.pan, 'f'); p(s.name, 's'); p(s.color, 'i');
      if (s.sends) for (const x of Object.values(s.sends)) p(x, 'f');
    }
    for (const g of muteGroups) p(g.path, 'i');
    return { model, inputs, returns, targets, main, dcas, muteGroups, params, clearSolo: null, meters: [] };
  }
  layoutFor('CL5'); // fills the address map

  const key = (path) => { const a = map.get(path); return a.addr + ' ' + a.x + ' ' + a.y; };

  // fader position (0..1, the tab's law: 0.75 = 0 dB, 1 = +10 dB) <-> 0.01 dB
  function posToRaw(f) {
    if (!(f > 0)) return -32768;
    return Math.min(1000, Math.round(Levels.faderToDb(Math.min(1, f)) * 100));
  }
  // the tab's law turned round without its 1024 steps, so 0 dB is 0.75 exactly (the console
  // holds 0.01 dB steps)
  function rawToPos(raw) {
    const v = Number(raw);
    if (!(v > -9000)) return 0;
    const db = v / 100;
    let f;
    if (db >= -10) f = (db + 30) / 40;
    else if (db >= -30) f = (db + 50) / 80;
    else if (db >= -60) f = (db + 70) / 160;
    else f = (db + 90) / 480;
    return Math.min(1, Math.max(0, f));
  }

  return {
    id: 'yamaha',
    family: 'Yamaha CL, QL, TF and DM7',
    session: 'rcp',
    port: 49280,
    models: Object.keys(MODELS),
    beta: true,
    modelOf,
    layout: () => layoutFor('CL5'),
    layoutFor,
    isLate: (path) => /\/mix\/\d+$/.test(path),
    key,
    // the lines a set sends (without "set "): a stereo strip's level, on and sends go to L and R
    encode(path, raw) {
      const a = map.get(path);
      const v = typeof raw === 'string' ? '"' + raw.replace(/["\\]/g, '\\$&') + '"' : String(raw);
      const out = [a.addr + ' ' + a.x + ' ' + a.y + ' ' + v];
      if (a.pair) out.push(a.addr + ' ' + (a.x + 1) + ' ' + a.y + ' ' + v);
      return out;
    },
    toValue(path, v) {
      const k = (map.get(path) || {}).kind;
      if (k === 'level') return posToRaw(v);
      if (k === 'pan') return Math.max(-63, Math.min(63, Math.round(v * 126 - 63)));
      if (k === 'color') return COLORS[v | 0] || 'Off';
      if (k === 'name') return String(v);
      return v ? 1 : 0;
    },
    fromValue(path, raw) {
      const k = (map.get(path) || {}).kind;
      if (k === 'level') return rawToPos(raw);
      if (k === 'pan') return (Math.max(-63, Math.min(63, Number(raw) || 0)) + 63) / 126;
      if (k === 'name') return String(raw);
      if (k === 'color') { const i = COLORS.indexOf(String(raw)); return i < 0 ? 0 : i; }
      return Number(raw) ? 1 : 0;
    },
    colors: COLORS,
    colorOf: (c) => HEX[c | 0] || HEX[0],
    colorInverted: (c) => LIGHT.has(c | 0),
    connectHelp: [
      'Cable the console’s network port (NETWORK, not Dante) to a router or access point and put this phone or tablet on that network.',
      'Type the console’s IP address (SETUP > NETWORK on CL and QL, SETUP > NETWORK on TF, Setup > Network on DM7) and tap Connect. It is not found by Scan.',
      'Beta: names, colours, faders, on/off, pans, mix sends, DCAs and mute groups. No meters or cue yet. Not yet tried on a console.',
    ],
  };
});
