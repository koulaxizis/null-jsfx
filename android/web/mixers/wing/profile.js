// NULL Remote, Mixers: the Behringer WING profile (WING, WING Compact, WING Rack: one OSC tree).
// OSC over UDP port 2223. Source: Behringer's "WING remote protocols" by P.-G. Maillot (V 3.0.5-1,
// for firmware 3.0.5 and later), with the Bitfocus Companion WING module and libwing as checks.
//   Discovery: "/?" answers ,s "WING,<ip>,<name>,<model>,<serial>,<firmware>" (model ngc-full,
//   ngc-compact, ngc-rack). Strips /ch/1..40, /aux/1..8, /bus/1..16, /main/1..4, /dca/1..16
//   (not zero padded), leaves fdr (dB, -144..10), mute (1 = muted), pan (-100..100), name, col
//   (1..12), $solo; sends /<strip>/send/1..16/lvl (dB); mute groups /mgrp/1..8/mute.
//   A query (the address alone) answers ,sff (text, raw, value) or ,sfi or ,s; a set takes the
//   value in its own units and is not echoed. "/*S" subscribes to every change as one native
//   argument, for 10 s, and the console keeps only one subscriber: the newest one.
// Unlike X AIR and X32, the WING speaks dB and its own units: this profile converts to and from
// the tab's fader positions (toWire, fromWire). Not covered in this version: meters (the WING
// sends them only through its native protocol on TCP 2222 and a UDP port of the client's),
// mute-group membership per strip (strip tags, format not documented), mains 2..4, matrices.
// NULL JSFX • https://nulljsfx.tech • MIT License
(function (root, factory) {
  const api = factory(root);
  if (typeof module === 'object' && module.exports) module.exports = api;
  else (root.NullMixerProfiles = root.NullMixerProfiles || {}).wing = api;
})(typeof self !== 'undefined' ? self : this, function (root) {
  'use strict';

  const Levels = typeof module === 'object' && module.exports ? require('../core/levels.js') : root.NullLevels;

  const MODELS = { 'ngc-full': 'WING', 'ngc-compact': 'WING Compact', 'ngc-rack': 'WING Rack' };
  const CHANNELS = 40;
  const AUX = 8;
  const BUSES = 16;
  const DCAS = 16;
  const MUTE_GROUPS = 8;
  const FLOOR_DB = -144; // the WING's -oo

  // Colour 1..12 (protocol p.190): gray blue, medium blue, dark blue, turquoise, green, olive
  // green, yellow, orange, red, coral, pink, mauve. 0 or anything else: no colour.
  const COLORS = ['#3a3a3a', '#6f7f99', '#4a7fd4', '#2f4f9e', '#3fb3b0', '#5fae5a', '#8a9a3c', '#d9c441', '#e08a3a', '#d4534a', '#e07a6a', '#d77fb8', '#a07ad0'];
  const colorOf = (i) => COLORS[(i | 0) >= 1 && (i | 0) <= 12 ? i | 0 : 0];
  const colorInverted = () => false;

  // from the raw model in the "/?" answer (ngc-compact) or a name already made (WING Compact)
  const model = (name) => {
    const k = String(name || '').trim();
    return MODELS[k.toLowerCase()] || Object.values(MODELS).find((m) => m.toLowerCase() === k.toLowerCase()) || 'WING';
  };

  function strip(kind, index, base, label, opts = {}) {
    return {
      id: kind + index,
      kind,
      index,
      label,
      fader: base + '/fdr',
      on: base + '/mute', // shown as "on" (1 = playing) like the other families; converted below
      pan: opts.pan === false ? null : base + '/pan',
      name: base + '/name',
      color: base + '/col',
      sends: null,
      muteGroups: null, // membership lives in the strip's tags; not documented yet
      solo: base + '/$solo',
      meter: null,
    };
  }

  function layout(modelName) {
    const inputs = [];
    for (let i = 1; i <= CHANNELS; i++) inputs.push(strip('ch', i, '/ch/' + i, 'Ch ' + i));
    for (let i = 1; i <= AUX; i++) inputs.push(strip('aux', i, '/aux/' + i, 'Aux ' + i));
    const targets = [];
    for (let i = 1; i <= BUSES; i++) targets.push({ id: 'bus' + i, label: 'Bus ' + i, slot: String(i), master: strip('bus', i, '/bus/' + i, 'Bus ' + i) });
    for (const s of inputs) {
      const base = s.fader.replace(/\/fdr$/, '');
      s.sends = {};
      for (const t of targets) s.sends[t.id] = base + '/send/' + t.slot + '/lvl';
    }
    const main = strip('main', 1, '/main/1', 'Main 1');
    const dcas = [];
    for (let i = 1; i <= DCAS; i++) dcas.push(strip('dca', i, '/dca/' + i, 'DCA ' + i, { pan: false }));
    const muteGroups = [];
    for (let i = 1; i <= MUTE_GROUPS; i++) muteGroups.push({ id: 'mg' + i, label: 'Mute ' + i, path: '/mgrp/' + i + '/mute', bit: 1 << (i - 1) });

    const params = [];
    const add = (path, type) => { if (path) params.push({ path, type }); };
    const addStrip = (s) => {
      add(s.fader, 'f'); add(s.on, 'i'); add(s.pan, 'f'); add(s.name, 's'); add(s.color, 'i'); add(s.solo, 'i');
      if (s.sends) for (const p of Object.values(s.sends)) add(p, 'f');
    };
    inputs.concat(targets.map((t) => t.master), [main], dcas).forEach(addStrip);
    for (const g of muteGroups) add(g.path, 'i');

    return {
      model: model(modelName),
      inputs,
      returns: [], // FX returns come back on channels or aux strips on a WING
      targets,
      main,
      dcas,
      muteGroups,
      params,
      clearSolo: null, // no clear-solo command: the tab sets every soloed strip's $solo to 0
      meters: [],
    };
  }

  // the "/?" answer: one string "WING,<ip>,<name>,<model>,<serial>,<firmware>"
  function parseInfo(args) {
    const f = String((args && args[0] && args[0].v) || '').split(',');
    if (f[0] !== 'WING' || f.length < 4) return null;
    return { ip: f[1], name: f[2], model: model(f[3]), modelRaw: f[3], version: f[5] || '' };
  }

  // --- the tab's values <-> the WING's units ----------------------------------------------------
  const isLevel = (p) => /\/(fdr|lvl)$/.test(p);
  const isPan = (p) => /\/pan$/.test(p);
  const isStripMute = (p) => /\/mute$/.test(p) && !p.startsWith('/mgrp/');
  // the tab's positions have 1024 steps (the X AIR law); the WING shows 0.1 dB, so a position
  // goes out rounded to that: 0 dB at unity stays 0.0, not -0.01
  const posToDb = (f) => (f <= 0 ? FLOOR_DB : Math.round(Levels.faderToDb(f) * 10) / 10 + 0);
  const dbToPos = (db) => (db <= -90 ? 0 : Levels.dbToFader(Math.min(10, db)));

  // A value from the console: a query answer (,sff / ,sfi: text, raw, value; ,s: text) or a
  // subscription event (one native argument). Returns the tab's value.
  function fromWire(path, args) {
    if (!args || !args.length) return undefined;
    const a = args.length >= 3 ? args[2] : args[0];
    let v = a.v;
    if (isLevel(path)) return dbToPos(Number(v));
    if (isPan(path)) return Math.min(1, Math.max(0, (Number(v) + 100) / 200));
    if (isStripMute(path)) return Number(v) ? 0 : 1;
    if (a.t === 'f' && /\/(col|\$solo|mute)$/.test(path)) v = Math.round(v);
    return v;
  }
  // The tab's value as the WING takes it.
  function toWire(path, value, type) {
    if (isLevel(path)) return [{ t: 'f', v: posToDb(value) }];
    if (isPan(path)) return [{ t: 'f', v: Math.round(value * 200 - 100) }];
    if (isStripMute(path)) return [{ t: 'i', v: value ? 0 : 1 }];
    return [{ t: type, v: value }];
  }

  return {
    id: 'wing',
    family: 'WING',
    port: 2223,
    infoAddress: '/?',
    keepAliveMs: 8000,
    // the subscription lasts 10 s, and only the newest subscriber gets the events
    keepAlive: () => [['/*S', []]],
    isMeter: () => false,
    models: ['WING', 'WING Compact', 'WING Rack'],
    model,
    matches: (raw) => /^ngc-/i.test(String(raw || '')),
    layout,
    parseInfo,
    fromWire,
    toWire,
    colorOf,
    colorInverted,
    connectHelp: [
      'Cable the WING’s Ethernet port to a router or access point and put this phone or tablet on that network.',
      'Tap Scan, or type the WING’s IP address (Setup > Network on the console) and tap Connect.',
      'The WING sends its changes to one remote app at a time (the last one to connect). No meters yet on the WING.',
    ],
  };
});
