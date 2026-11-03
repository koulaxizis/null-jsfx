// NULL Remote, Mixers: the Allen & Heath CQ profile (CQ-12T, CQ-18T, CQ-20B: one parameter map).
// MIDI NRPN over TCP port 51325 (core/nrpn.js, no hooks: the SQ's way), MIDI channel 1 (the CQ
// uses channel 1 for all control). Source: Allen & Heath's "CQ MIDI Protocol", firmware V1.2.0,
// issue 4. Its parameter numbers follow the SQ's scheme on fewer channels:
//   Sources: Ip 1-16 = 0..15, ST 1 = 24, ST 2 = 26, USB = 28, BT = 30, FX returns 1-4 = 60..63.
//   Mutes (MSB 00): a source at 00 + source, LR 44, Out 1-6 45..4A; DCA 02 00..03; mute groups
//     04 00..03 (VC 00, VF 01 muted, 00 not). The document lists no mute for the FX returns.
//   Levels: to LR at 40 00 + source, to Out a at 40 44 + source·12 + (a - 1) (Ip1 > Out1 40 44,
//     ST1 > Out1 42 64, FX4 > Out6 46 3D); LR master 4F 00, Out masters 4F 01..06, DCA 4F 20..23.
//   Pan: to LR at 50 00 + source, to Out at 50 44 + source·12 + (a - 1); 00 00 left, 40 00
//     centre, 7F 7F right.
//   Get: the SQ's (CC 99, 98, then CC 96 with 7F); the document does not give the answer's form.
// Levels are 14 bits: the document's table has 00 00 = -inf, 62 00 = 0 dB, 7F 40 = +10 dB; this
// profile goes in straight lines between those from the tab's 0, 0.75 and 1, so 0 dB and +10 dB
// are exact and the dB shown in between is near (Bible C23). Linked outputs (Out 1/2, 3/4, 5/6)
// share the first one's parameters.
// Not in the protocol: names, colours, meters, solo. Not covered yet: FX sends, soft keys, scenes.
// NULL JSFX • https://nulljsfx.tech • MIT License
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else (root.NullMixerProfiles = root.NullMixerProfiles || {}).cq = api;
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  const OUTS = 6;
  const RETURNS = 4;
  const DCAS = 4;
  const MUTE_GROUPS = 4;
  const ZERO_DB = 0x62 * 128; // 12544
  const TOP = 0x7f * 128 + 0x40; // +10 dB, 16320
  const FULL = 16383;
  const CENTRE = 0x40 * 128; // 8192
  const at = (msb, lsb) => msb * 128 + lsb;
  // the input strips: [kind, index, label, source]
  const SOURCES = [];
  for (let i = 1; i <= 16; i++) SOURCES.push(['ip', i, 'Ip ' + i, i - 1]);
  SOURCES.push(['st', 1, 'ST 1', 24], ['st', 2, 'ST 2', 26], ['usb', 1, 'USB', 28], ['bt', 1, 'BT', 30]);

  // a path of the tab -> {addr, kind: 'level' | 'mute' | 'group' | 'pan'}
  const map = new Map();
  const add = (path, addr, kind) => { map.set(path, { addr, kind }); return path; };

  function strip(kind, index, label, src, muteAddr) {
    const base = '/' + kind + '/' + index;
    const s = {
      id: kind + index, kind, index, label,
      fader: add(base + '/lr', at(0x40, 0) + src, 'level'),
      on: muteAddr == null ? null : add(base + '/mute', muteAddr, 'mute'),
      pan: add(base + '/pan', at(0x50, 0) + src, 'pan'),
      name: null, color: null, muteGroups: null, solo: null, meter: null,
      sends: {},
    };
    for (let a = 1; a <= OUTS; a++) s.sends['out' + a] = add(base + '/out/' + a, at(0x40, 0x44) + src * 12 + (a - 1), 'level');
    return s;
  }
  const master = (id, kind, index, label, fader, on) => ({
    id, kind, index, label, fader, on, pan: null, name: null, color: null, sends: null, muteGroups: null, solo: null, meter: null,
  });

  function layout() {
    const inputs = SOURCES.map(([kind, index, label, src]) => strip(kind, index, label, src, at(0, src)));
    const returns = [];
    for (let r = 1; r <= RETURNS; r++) returns.push(strip('fx', r, 'FX Rtn ' + r, 60 + r - 1, null));
    const targets = [];
    for (let a = 1; a <= OUTS; a++) {
      targets.push({ id: 'out' + a, label: 'Out ' + a, slot: String(a), master: master('outm' + a, 'out', a, 'Out ' + a,
        add('/out/' + a + '/lvl', at(0x4f, a), 'level'), add('/out/' + a + '/mute', at(0, 0x45 + a - 1), 'mute')) });
    }
    const main = master('lr', 'lr', 0, 'LR', add('/lr/lvl', at(0x4f, 0), 'level'), add('/lr/mute', at(0, 0x44), 'mute'));
    const dcas = [];
    for (let d = 1; d <= DCAS; d++) {
      dcas.push(master('dca' + d, 'dca', d, 'DCA ' + d, add('/dca/' + d + '/lvl', at(0x4f, 0x20 + d - 1), 'level'), add('/dca/' + d + '/mute', at(2, d - 1), 'mute')));
    }
    const muteGroups = [];
    for (let g = 1; g <= MUTE_GROUPS; g++) muteGroups.push({ id: 'mg' + g, label: 'Mute ' + g, path: add('/mgrp/' + g, at(4, g - 1), 'group'), bit: 1 << (g - 1) });

    const params = [];
    const p = (path, type) => { if (path) params.push({ path, type }); };
    for (const s of inputs.concat(returns, targets.map((t) => t.master), [main], dcas)) {
      p(s.fader, 'f'); p(s.on, 'i'); p(s.pan, 'f');
      if (s.sends) for (const x of Object.values(s.sends)) p(x, 'f');
    }
    for (const g of muteGroups) p(g.path, 'i');
    return { model: 'CQ', inputs, returns, targets, main, dcas, muteGroups, params, clearSolo: null, meters: [] };
  }
  layout(); // fills the address map

  // fader position (0..1, 0.75 = 0 dB) <-> the 14-bit value; above +10 dB (7F 41..7F 7F) reads 1
  function posToRaw(f) {
    f = Math.min(1, Math.max(0, f));
    return Math.round(f <= 0.75 ? (f / 0.75) * ZERO_DB : ZERO_DB + ((f - 0.75) / 0.25) * (TOP - ZERO_DB));
  }
  function rawToPos(v) {
    return v <= ZERO_DB ? (v / ZERO_DB) * 0.75 : Math.min(1, 0.75 + ((v - ZERO_DB) / (TOP - ZERO_DB)) * 0.25);
  }
  function panToRaw(x) {
    x = Math.min(1, Math.max(0, x));
    return x <= 0.5 ? Math.round(x * 2 * CENTRE) : CENTRE + Math.round((x - 0.5) * 2 * (FULL - CENTRE));
  }
  const rawToPan = (v) => (v <= CENTRE ? v / CENTRE / 2 : 0.5 + (v - CENTRE) / (FULL - CENTRE) / 2);

  const kindOf = (path) => (map.get(path) || {}).kind;
  return {
    id: 'cq',
    family: 'Allen & Heath CQ',
    session: 'nrpn',
    port: 51325,
    midiChannel: 0,
    models: ['CQ-12T', 'CQ-18T', 'CQ-20B'],
    layout,
    address: (path) => map.get(path).addr,
    // an address only an SQ has (Aux 12's mute): the CQ, with Out 1-6, does not answer it
    sqOnlyAddress: at(0, 0x50),
    toValue(path, v) {
      const k = kindOf(path);
      if (k === 'level') return posToRaw(v);
      if (k === 'pan') return panToRaw(v);
      if (k === 'mute') return v ? 0 : 1; // the tab's "on" (1 = playing) is the CQ's mute 0
      return v ? 1 : 0;
    },
    fromValue(path, raw) {
      const k = kindOf(path);
      if (k === 'level') return rawToPos(raw);
      if (k === 'pan') return rawToPan(raw);
      if (k === 'mute') return raw ? 0 : 1;
      return raw ? 1 : 0;
    },
    colorOf: () => '#3a3a3a',
    colorInverted: () => false,
    connectHelp: [
      'Put the CQ on your network (its network port, or its Wi-Fi in client mode) and put this phone or tablet on the same network.',
      'Type the CQ’s IP address (Settings > Network on the CQ) and tap Connect. The CQ is not found by Scan, and it takes one MIDI connection at a time.',
      'Faders, mutes, pans, sends to Out 1-6, DCAs and mute groups. No names, colours, meters or solo: its MIDI protocol has none.',
    ],
  };
});
