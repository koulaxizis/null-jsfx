// NULL Remote, Mixers: the Allen & Heath SQ profile (SQ-5, SQ-6, SQ-7: one parameter map).
// MIDI NRPN over TCP port 51325 (core/nrpn.js), MIDI channel 1 (the console's default is set
// under Utility > General > MIDI; this version uses channel 1). Source: Allen & Heath's
// "SQ MIDI Protocol", Issue 5 (firmware 1.5.0 and later).
//   Mutes (MSB 00): inputs 00 00..2F, groups 30..3B, FX returns 3C..43, LR 44, Aux 1-12 45..50;
//     DCA 02 00..07; mute groups 04 00..07 (1 = muted, 0 = not).
//   Levels: input or FX return to LR at 40 00 + source (inputs 0..47, groups 48..59, FX
//     returns 60..67); source to Aux a at 40 44 + source·12 + (a - 1) (the document's tables:
//     Ip40 > Aux5 = 44 1C, Grp1 > Aux1 = 45 04, FX1Rtn > Aux1 = 46 14 all follow it); LR master
//     4F 00, Aux masters 4F 01..0C.
//   Pan: to LR at 50 00 + source, to Aux at 50 44 + source·12 + (a - 1) (Ip24 > Aux5 = 52 5C,
//     Ip37 > Aux8 = 53 7B); 00 00 left, 3F 7F centre, 7F 7F right.
// Levels are 14 bits. The document gives points, not a law: in its linear taper 0 = -inf,
// 76 5C = 0 dB, 7F 7F = +10 dB; between those this profile goes in straight lines from the
// tab's fader positions (0, 0.75, 1), so 0 dB and +10 dB are exact and the dB shown in between
// is near, not exact (UNVERIFIED, Bible C18). The console must be set to the linear taper
// (Utility > General > MIDI > NRPN Fader Law).
// Not in the protocol: names, colours, meters, solo, scenes. Not covered yet: groups as strips,
// DCAs (their level address is not certain), FX sends, matrices.
// NULL JSFX • https://nulljsfx.tech • MIT License
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else (root.NullMixerProfiles = root.NullMixerProfiles || {}).sq = api;
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  const INPUTS = 48;
  const RETURNS = 8;
  const AUX = 12;
  const MUTE_GROUPS = 8;
  const ZERO_DB = 0x76 * 128 + 0x5c; // 15196
  const FULL = 16383;
  const at = (msb, lsb) => msb * 128 + lsb;

  // a path of the tab -> {addr, kind: 'level' | 'mute' | 'group' | 'pan'}
  const map = new Map();
  const add = (path, addr, kind) => { map.set(path, { addr, kind }); return path; };

  function strip(kind, index, label, src, muteAddr) {
    const base = '/' + kind + '/' + index;
    const s = {
      id: kind + index, kind, index, label,
      fader: add(base + '/lr', at(0x40, 0) + src, 'level'),
      on: add(base + '/mute', muteAddr, 'mute'),
      pan: add(base + '/pan', at(0x50, 0) + src, 'pan'),
      name: null, color: null, muteGroups: null, solo: null, meter: null,
      sends: {},
    };
    for (let a = 1; a <= AUX; a++) s.sends['aux' + a] = add(base + '/aux/' + a, at(0x40, 0x44) + src * 12 + (a - 1), 'level');
    return s;
  }

  function layout() {
    const inputs = [];
    for (let i = 1; i <= INPUTS; i++) inputs.push(strip('ip', i, 'Ip ' + i, i - 1, at(0, i - 1)));
    const returns = [];
    for (let r = 1; r <= RETURNS; r++) returns.push(strip('fx', r, 'FX Rtn ' + r, 60 + r - 1, at(0, 0x3c + r - 1)));
    const targets = [];
    for (let a = 1; a <= AUX; a++) {
      targets.push({ id: 'aux' + a, label: 'Aux ' + a, slot: String(a), master: {
        id: 'auxm' + a, kind: 'aux', index: a, label: 'Aux ' + a,
        fader: add('/aux/' + a + '/lvl', at(0x4f, a), 'level'),
        on: add('/aux/' + a + '/mute', at(0, 0x45 + a - 1), 'mute'),
        pan: null, name: null, color: null, sends: null, muteGroups: null, solo: null, meter: null,
      } });
    }
    const main = {
      id: 'lr', kind: 'lr', index: 0, label: 'LR',
      fader: add('/lr/lvl', at(0x4f, 0), 'level'),
      on: add('/lr/mute', at(0, 0x44), 'mute'),
      pan: null, name: null, color: null, sends: null, muteGroups: null, solo: null, meter: null,
    };
    const muteGroups = [];
    for (let g = 1; g <= MUTE_GROUPS; g++) muteGroups.push({ id: 'mg' + g, label: 'Mute ' + g, path: add('/mgrp/' + g, at(4, g - 1), 'group'), bit: 1 << (g - 1) });

    const params = [];
    const p = (path, type) => { if (path) params.push({ path, type }); };
    for (const s of inputs.concat(returns, targets.map((t) => t.master), [main])) {
      p(s.fader, 'f'); p(s.on, 'i'); p(s.pan, 'f');
      if (s.sends) for (const x of Object.values(s.sends)) p(x, 'f');
    }
    for (const g of muteGroups) p(g.path, 'i');
    return { model: 'SQ', inputs, returns, targets, main, dcas: [], muteGroups, params, clearSolo: null, meters: [] };
  }
  layout(); // fills the address map

  // fader position (0..1, 0.75 = 0 dB) <-> the 14-bit linear-taper value
  function posToRaw(f) {
    f = Math.min(1, Math.max(0, f));
    return Math.round(f <= 0.75 ? (f / 0.75) * ZERO_DB : ZERO_DB + ((f - 0.75) / 0.25) * (FULL - ZERO_DB));
  }
  function rawToPos(v) {
    return v <= ZERO_DB ? (v / ZERO_DB) * 0.75 : 0.75 + ((v - ZERO_DB) / (FULL - ZERO_DB)) * 0.25;
  }
  // pan 0..1 <-> 0..16383, in two straight halves that meet at 3F 7F (8191), the centre
  const CENTRE = 8191;
  function panToRaw(x) {
    x = Math.min(1, Math.max(0, x));
    return x <= 0.5 ? Math.round(x * 2 * CENTRE) : CENTRE + Math.round((x - 0.5) * 2 * (FULL - CENTRE));
  }
  const rawToPan = (v) => (v <= CENTRE ? v / CENTRE / 2 : 0.5 + (v - CENTRE) / (FULL - CENTRE) / 2);

  const kindOf = (path) => (map.get(path) || {}).kind;
  return {
    id: 'sq',
    family: 'Allen & Heath SQ',
    session: 'nrpn',
    port: 51325,
    midiChannel: 0,
    models: ['SQ-5', 'SQ-6', 'SQ-7'],
    layout,
    address: (path) => map.get(path).addr,
    // the tab's value -> the console's 14-bit value, and back
    toValue(path, v) {
      const k = kindOf(path);
      if (k === 'level') return posToRaw(v);
      if (k === 'pan') return panToRaw(v);
      if (k === 'mute') return v ? 0 : 1; // the tab's "on" (1 = playing) is the console's mute 0
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
      'Cable the SQ’s network port to a router or access point and put this phone or tablet on that network.',
      'On the SQ: Utility > General > MIDI: MIDI channel 1 and NRPN Fader Law Linear Taper.',
      'Type the SQ’s IP address (Utility > General > Network) and tap Connect. The SQ is not found by Scan. No names, colours, meters or solo: its MIDI protocol has none.',
    ],
  };
});
