// NULL Remote, Mixers: the X AIR family profile (XR12, XR16, XR18, X18; the MR12/MR18 share it).
// OSC over UDP port 10024. Paths follow the community X AIR OSC reference
// (behringer.world/wiki x-air_osc): /ch/01..16, /rtn/aux, /rtn/1..4 (FX returns), /bus/1..6,
// /fxsend/1..4, /lr, /dca/1..4; channel sends /ch/NN/mix/01..06/level to the buses and
// /ch/NN/mix/07..10/level to FX 1..4. Channel and bus counts per model follow xair-api (kinds.py).
// Marked UNVERIFIED below: the solo switch index map, the /meters/1 layout and the mute-group bit
// order, which no public source states; they must be checked on a real console (Bible VIII).
// NULL JSFX • https://nulljsfx.tech • MIT License
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else (root.NullMixerProfiles = root.NullMixerProfiles || {}).xair = api;
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  const MODELS = {
    XR12: { channels: 12, buses: 2 },
    XR16: { channels: 16, buses: 4 },
    XR18: { channels: 16, buses: 6 },
    X18: { channels: 16, buses: 6 },
    MR12: { channels: 12, buses: 2 },
    MR18: { channels: 16, buses: 6 },
  };
  const FX = 4;
  const DCAS = 4;
  const MUTE_GROUPS = 4;
  const pad2 = (n) => String(n).padStart(2, '0');

  // Colour index 0..15: OFF, RD, GN, YE, BL, MG, CY, WH, then the same eight inverted ("i").
  const COLORS = ['#3a3a3a', '#d4534a', '#5fae5a', '#d9c441', '#4a7fd4', '#b05cc8', '#4ab8c4', '#e0e0e0'];
  const colorOf = (i) => COLORS[(i | 0) & 7];
  const colorInverted = (i) => (i | 0) >= 8;

  // Solo switch indices (UNVERIFIED): ch 01..16, aux 17, FX returns 18..21, buses 22..27,
  // FX sends 28..31, main 32. Taken from the X32-style ordering of /-stat/solosw.
  const SOLO = { ch: 0, aux: 16, rtn: 17, bus: 21, fxsend: 27, lr: 32 };
  // /meters/1 (UNVERIFIED): 16 channels, aux L/R, FX returns 1..4 L/R, buses 1..6, FX sends 1..4,
  // main L/R, monitor L/R = 40 values.
  const METER = { ch: -1, aux: 16, rtn: 17, bus: 25, fxsend: 31, lr: 36 };

  function model(name) {
    const key = String(name || '').toUpperCase().replace(/[^A-Z0-9]/g, '');
    return MODELS[key] ? key : 'XR18';
  }
  // which answers to /xinfo this profile takes (an X32 or M32 answers X32.. or M32..)
  const matches = (raw) => !/^[XM]32/i.test(String(raw || '').trim());

  // One strip: a fader with its mute, pan, name and colour, and where it sits in solo and meters.
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
    if (kind in METER) s.meter = kind === 'lr' ? METER.lr : kind === 'aux' ? METER.aux : METER[kind] + (kind === 'rtn' ? 2 * (index - 1) + 1 : index);
    return s;
  }

  function layout(modelName) {
    const m = model(modelName);
    const { channels, buses } = MODELS[m];
    const inputs = [];
    for (let i = 1; i <= channels; i++) inputs.push(strip('ch', i, '/ch/' + pad2(i), 'Ch ' + i));
    inputs.push(strip('aux', 0, '/rtn/aux', 'Aux'));
    const returns = [];
    for (let i = 1; i <= FX; i++) returns.push(strip('rtn', i, '/rtn/' + i, 'FX ' + i + ' Rtn'));
    // mix targets: the buses and the FX sends, each with its master strip and its send slot
    const targets = [];
    for (let i = 1; i <= buses; i++) targets.push({ id: 'bus' + i, label: 'Bus ' + i, slot: pad2(i), master: strip('bus', i, '/bus/' + i, 'Bus ' + i) });
    for (let i = 1; i <= FX; i++) targets.push({ id: 'fx' + i, label: 'FX ' + i, slot: pad2(6 + i), master: strip('fxsend', i, '/fxsend/' + i, 'FX ' + i + ' Send', { pan: false }) });
    // every input and FX return can send to every target: /<base>/mix/<slot>/level
    for (const s of inputs.concat(returns)) {
      const base = s.fader.replace(/\/mix\/fader$/, '');
      s.sends = {};
      for (const t of targets) s.sends[t.id] = base + '/mix/' + t.slot + '/level';
    }
    const main = strip('lr', 0, '/lr', 'Main LR', { groups: false });
    const dcas = [];
    for (let i = 1; i <= DCAS; i++) {
      const d = strip('dca', i, '/dca/' + i, 'DCA ' + i, { noMix: true, pan: false, groups: false });
      d.name = '/dca/' + i + '/config/name';
      d.color = '/dca/' + i + '/config/color';
      dcas.push(d);
    }
    const muteGroups = [];
    for (let i = 1; i <= MUTE_GROUPS; i++) muteGroups.push({ id: 'mg' + i, label: 'Mute ' + i, path: '/config/mute/' + i, bit: 1 << (i - 1) });

    // typed parameter list, queried on connect
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
      meterAddress: '/meters/1',
    };
  }

  // /xinfo reply: ip, console name, model, firmware version (all strings).
  function parseInfo(args) {
    const s = (args || []).map((a) => String(a.v));
    if (s.length < 3 || !matches(s[2])) return null;
    return { ip: s[0], name: s[1], model: model(s[2]), modelRaw: s[2], version: s[3] || '' };
  }

  return {
    id: 'xair',
    family: 'X AIR',
    port: 10024,
    infoAddress: '/xinfo',
    keepAliveMs: 8000,
    // /xremote: the console pushes every change for the next 10 s; /meters starts the meter
    // stream for 10 s too, so both are renewed every 8 s.
    keepAlive: () => [['/xremote', []], ['/meters', [{ t: 's', v: '/meters/1' }]]],
    isMeter: (a) => a.startsWith('/meters/'),
    models: Object.keys(MODELS),
    model,
    matches,
    layout,
    parseInfo,
    colorOf,
    colorInverted,
    connectHelp: [
      'Connect this phone or tablet to the same network as the mixer: the mixer’s own Wi-Fi (access point mode, the mixer is then at 192.168.1.1), or the router the mixer is cabled or joined to.',
      'Tap Scan. Mixers on the network appear by name and model. If none appear (some routers block broadcasts), type the mixer’s IP address and tap Connect.',
      'Close other apps that control the same mixer only if you see lag: the mixer serves several remotes at once.',
    ],
  };
});
