// NULL Remote, Mixers: the X32 and M32 profile (Behringer X32, X32 Compact, Producer, Rack and Core;
// Midas M32, M32R and M32C: one OSC tree for all). OSC over UDP port 10023.
// Sources: Patrick-Gilles Maillot's "Unofficial X32/M32 OSC Remote Protocol" and his X32 emulator
// (github.com/pmaillot/X32-Behringer: X32.c and the X32*.h tables), the Bitfocus Companion X32
// module (src/paths.ts: the solo switch map) and the x32_osc_state crate (the meter blob).
//   /ch/01..32, /auxin/01..08, /fxrtn/01..08 (inputs, each with sends /mix/01..16/level to the
//   buses), /bus/01..16, /main/st (Main LR), /dca/1..8 (not zero padded), mute groups
//   /config/mute/1..6, /-stat/solosw/01..80, /-action/clearsolo.
// Meters: /meters/0 (70 values: 32 channels, 8 aux, 8 FX returns, 16 buses, 6 matrices) and
// /meters/2 (49 values: 16 buses, 6 matrices, main L, main R, mono, then gain reductions); each
// blob is an int32 count then float32 values, little endian, linear (1.0 = 0 dBFS).
// Marked UNVERIFIED below: the mute-group bit order and the linear meter scale, which no source
// states outright, and the model strings in /xinfo, which come from the PDF only (Bible VIII).
// NULL JSFX • https://nulljsfx.tech • MIT License
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else (root.NullMixerProfiles = root.NullMixerProfiles || {}).x32 = api;
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  // every model has the same engine: 32 channels, 8 aux ins, 8 FX returns, 16 buses, 8 DCAs
  const MODELS = ['X32', 'X32C', 'X32P', 'X32RACK', 'X32CORE', 'M32', 'M32R', 'M32C'];
  const CHANNELS = 32;
  const AUX = 8;
  const RETURNS = 8;
  const BUSES = 16;
  const DCAS = 8;
  const MUTE_GROUPS = 6;
  const pad2 = (n) => String(n).padStart(2, '0');

  // Colour index 0..15, the same palette as X AIR: OFF, RD, GN, YE, BL, MG, CY, WH, then inverted.
  const COLORS = ['#3a3a3a', '#d4534a', '#5fae5a', '#d9c441', '#4a7fd4', '#b05cc8', '#4ab8c4', '#e0e0e0'];
  const colorOf = (i) => COLORS[(i | 0) & 7];
  const colorInverted = (i) => (i | 0) >= 8;

  // /-stat/solosw indices (Companion): ch 01..32, aux 33..40, FX returns 41..48, buses 49..64,
  // matrices 65..70, main LR 71, mono 72, DCA 73..80.
  const SOLO = { ch: 0, aux: 32, rtn: 40, bus: 48, lr: 71, dca: 72 };
  // meter indices: /meters/0 at 0..69, then /meters/2 at 70..118 (main L = 92, R = 93)
  const METERS = [{ address: '/meters/0', offset: 0, count: 70 }, { address: '/meters/2', offset: 70, count: 49 }];
  const METER = { ch: -1, aux: 31, rtn: 39, bus: 47, lr: 92 };

  function model(name) {
    const key = String(name || '').toUpperCase().replace(/[^A-Z0-9]/g, '');
    if (MODELS.includes(key)) return key;
    if (key.startsWith('M32')) return 'M32';
    return 'X32';
  }
  // which answers to /xinfo this profile takes (an X AIR answers XR.., X18 or MR..)
  const matches = (raw) => /^[XM]32/i.test(String(raw || '').trim());

  function strip(kind, index, base, label, opts = {}) {
    const mix = opts.noMix ? base : base + '/mix';
    const s = {
      id: kind + (index || ''),
      kind,
      index,
      label,
      fader: mix + '/fader',
      on: mix + '/on',
      pan: opts.pan === false ? null : mix + '/pan',
      name: base + '/config/name',
      color: base + '/config/color',
      sends: null,
      muteGroups: opts.groups === false ? null : base + '/grp/mute',
      solo: null,
      meter: null,
    };
    if (kind in SOLO) s.solo = '/-stat/solosw/' + pad2(kind === 'lr' ? SOLO.lr : SOLO[kind] + (index || 1));
    if (kind in METER) s.meter = kind === 'lr' ? METER.lr : METER[kind] + index;
    return s;
  }

  function layout(modelName) {
    const m = model(modelName);
    const inputs = [];
    for (let i = 1; i <= CHANNELS; i++) inputs.push(strip('ch', i, '/ch/' + pad2(i), 'Ch ' + i));
    for (let i = 1; i <= AUX; i++) inputs.push(strip('aux', i, '/auxin/' + pad2(i), 'Aux ' + i));
    const returns = [];
    for (let i = 1; i <= RETURNS; i++) returns.push(strip('rtn', i, '/fxrtn/' + pad2(i), 'FX Rtn ' + i));
    // mix targets: the 16 buses (on an X32 the FX sends are buses too)
    const targets = [];
    for (let i = 1; i <= BUSES; i++) targets.push({ id: 'bus' + i, label: 'Bus ' + i, slot: pad2(i), master: strip('bus', i, '/bus/' + pad2(i), 'Bus ' + i) });
    for (const s of inputs.concat(returns)) {
      const base = s.fader.replace(/\/mix\/fader$/, '');
      s.sends = {};
      for (const t of targets) s.sends[t.id] = base + '/mix/' + t.slot + '/level';
    }
    const main = strip('lr', 0, '/main/st', 'Main LR', { groups: false });
    const dcas = [];
    for (let i = 1; i <= DCAS; i++) {
      const d = strip('dca', i, '/dca/' + i, 'DCA ' + i, { noMix: true, pan: false, groups: false });
      dcas.push(d);
    }
    // mute groups on/off; a strip's /grp/mute is a bitmap, bit n-1 = group n (UNVERIFIED)
    const muteGroups = [];
    for (let i = 1; i <= MUTE_GROUPS; i++) muteGroups.push({ id: 'mg' + i, label: 'Mute ' + i, path: '/config/mute/' + i, bit: 1 << (i - 1) });

    const params = [];
    const add = (path, type) => { if (path) params.push({ path, type }); };
    const addStrip = (s) => {
      add(s.fader, 'f'); add(s.on, 'i'); add(s.pan, 'f'); add(s.name, 's'); add(s.color, 'i');
      add(s.muteGroups, 'i'); add(s.solo, 'i');
      if (s.sends) for (const p of Object.values(s.sends)) add(p, 'f');
    };
    inputs.concat(returns, targets.map((t) => t.master), [main], dcas).forEach(addStrip);
    for (const g of muteGroups) add(g.path, 'i');

    return {
      model: m,
      inputs,
      returns,
      targets,
      main,
      dcas,
      muteGroups,
      params,
      clearSolo: ['/-action/clearsolo', [{ t: 'i', v: 1 }]],
      meters: METERS,
    };
  }

  // /xinfo reply: ip, console name, model, firmware version (all strings).
  function parseInfo(args) {
    const s = (args || []).map((a) => String(a.v));
    if (s.length < 3 || !matches(s[2])) return null;
    return { ip: s[0], name: s[1], model: model(s[2]), modelRaw: s[2], version: s[3] || '' };
  }

  // A meter blob: int32 count, then float32 linear levels, little endian. Returns dB values.
  function parseMeter(blob) {
    if (!blob || blob.length < 4) return null;
    const dv = new DataView(blob.buffer, blob.byteOffset, blob.byteLength);
    const count = dv.getInt32(0, true);
    if (count < 0 || 4 + 4 * count > blob.length) return null;
    const out = new Float32Array(count);
    for (let i = 0; i < count; i++) {
      const v = dv.getFloat32(4 + 4 * i, true);
      out[i] = v > 0 ? Math.max(-90, 20 * Math.log10(v)) : -90;
    }
    return out;
  }
  // the console's side of parseMeter, for the simulator and the tests
  function buildMeter(db) {
    const out = new Uint8Array(4 + 4 * db.length);
    const dv = new DataView(out.buffer);
    dv.setInt32(0, db.length, true);
    db.forEach((d, i) => dv.setFloat32(4 + 4 * i, d <= -90 ? 0 : Math.pow(10, d / 20), true));
    return out;
  }

  return {
    id: 'x32',
    family: 'X32/M32',
    port: 10023,
    infoAddress: '/xinfo',
    keepAliveMs: 8000,
    // /xremote and each /meters request last 10 s on the console, so all are renewed every 8 s
    keepAlive: () => [['/xremote', []]].concat(METERS.map((m) => ['/meters', [{ t: 's', v: m.address }]])),
    isMeter: (a) => a.startsWith('/meters/'),
    models: ['X32', 'X32 Compact', 'X32 Producer', 'X32 Rack', 'X32 Core', 'M32', 'M32R', 'M32C'],
    model,
    matches,
    layout,
    parseInfo,
    parseMeter,
    buildMeter,
    colorOf,
    colorInverted,
    connectHelp: [
      'An X32 or M32 has no Wi-Fi of its own: cable its Ethernet port to a router or access point and put this phone or tablet on that network.',
      'Tap Scan. Mixers on the network appear by name and model. If none appear (some routers block broadcasts), type the mixer’s IP address (Setup > Network on the console) and tap Connect.',
      'The console serves four remotes at once (X32-Edit and other apps count).',
    ],
  };
});
