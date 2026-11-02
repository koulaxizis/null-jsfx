// NULL Remote, Mixers: the Allen & Heath Qu profile (Qu-16, Qu-24, Qu-32, Qu-Pac, Qu-SB). MIDI
// over TCP port 51325 (core/nrpn.js with this profile's hooks). Source: Allen & Heath's "Qu Mixer
// MIDI Protocol", firmware V1.9+ (Oct 2021). Not the Qu-5/6/7, which have their own document.
//   Connect: Get System State, F0 00 00 1A 50 11 01 00 7F 10 <flag> F7 (all call); the Qu answers
//     <header with its channel 0N> 11 <box: 1 Qu-16, 2 Qu-24, 3 Qu-32, 4 Qu-Pac, 5 Qu-SB>
//     <version> F7, pushes every value as NRPN, then ends with 14 F7. No value is asked one by one,
//     except names: 01 CH F7, answered 02 CH <ASCII> F7. Both sides send Active Sensing (FE); the
//     Qu closes a quiet connection. The Qu takes one TCP MIDI connection at a time.
//   NRPN: BN 63 CH, BN 62 ID, BN 06 VA, BN 26 VX: CH the channel, ID the parameter, VA the value
//     (7 bits), VX the destination where there is one. Fader 17 (VX 07), pan 16 (VX 07 = to LR;
//     00 left, 25 centre, 4A right), send level 20 (VX 00..06 = Mix 1, 2, 3, 4, 5-6, 7-8, 9-10).
//     Levels: 00 = -inf, 62 = 0 dB, 7F = +10 dB; between them the document gives a table, not a
//     law (+5 72, -5 4F, -10 3F, -20 2F, -30 1F, -40 10), so this profile goes in straight lines
//     from the tab's fader positions (0, 0.75, 1): 0 dB and +10 dB are exact (Bible C21).
//   Mutes: a note on with the channel as the note, velocity 7F (muted) or 3F (not), then the same
//     note with velocity 00, which the Qu ignores.
//   CH: FX return 1-4 08-0B, input 1-32 20-3F, ST 1-3 40-42, mute group 1-4 50-53, Mix 1-4 60-63,
//     Mix 5-6, 7-8, 9-10 64-66, LR 67.
//   Meters: 12 01 F7 asks for them (12 00 F7 stops them); the Qu answers 13 <data> F7, every
//     meter as 16 bits, signed dB in 7Q8 (dB·256) offset 8000, packed 7 to 8 bytes (a byte of the
//     seven top bits first). The document lists the meter blocks per model in columns that read
//     ambiguously, gives no count, and does not say whether 13 comes once or keeps coming, so
//     the session asks again after each answer (Bible C21). A strip shows its last meter before
//     the fader: Post Delay for an input, Post Fader for a mix and LR; the louder side for stereo.
// Not covered yet: PAFL, DCAs (the document does not give their master level), groups, matrices,
// FX sends, FX return meters (their block is not clear in the document), scenes.
// NULL JSFX • https://nulljsfx.tech • MIT License
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else (root.NullMixerProfiles = root.NullMixerProfiles || {}).qu = api;
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  const HEADER = [0xf0, 0x00, 0x00, 0x1a, 0x50, 0x11, 0x01, 0x00];
  const ALL_CALL = 0x7f;
  const BOXES = { 1: 'Qu-16', 2: 'Qu-24', 3: 'Qu-32', 4: 'Qu-Pac', 5: 'Qu-SB' };
  const MONO = { 'Qu-16': 16, 'Qu-24': 24 }; // the others: 32
  const RETURNS = 4;
  const MIXES = [['Mix 1', 0], ['Mix 2', 1], ['Mix 3', 2], ['Mix 4', 3], ['Mix 5-6', 4], ['Mix 7-8', 5], ['Mix 9-10', 6]];
  const MUTE_GROUPS = 4;
  const ID = { fader: 0x17, pan: 0x16, send: 0x20, mute: 0x7f, name: 0x7e }; // mute and name: ours, not NRPN
  const LR = 0x67;
  const ZERO_DB = 0x62;
  const CENTRE = 0x25;
  const RIGHT = 0x4a;

  // meters: where each model's blocks start, in meters (16 bits each). Mono input: 10 meters, Post
  // Delay the 4th; stereo input: the same 10 as L/R pairs; mono mix: 10, Post Fader the 6th; stereo
  // mix (5-6, 7-8, 9-10, LR): 10 L/R pairs. Our reading of the document's columns (Bible C21).
  const METER_BLOCKS = {
    'Qu-16': { mono: 16, stereo: 240, mono2: null, mixes: 320 },
    'Qu-24': { mono: 24, stereo: 240, mono2: null, mixes: 480 },
    Qu: { mono: 24, stereo: 240, mono2: 320, mixes: 400 }, // Qu-32, Qu-Pac, Qu-SB
  };
  const blocksOf = (model) => METER_BLOCKS[model] || METER_BLOCKS.Qu;
  // the tab's meters, in order: inputs (mono, then ST), Mix 1-4, Mix 5-6, 7-8, 9-10, LR. Each a
  // list of the meters in the Qu's data whose loudest it shows.
  function meterPoints(model) {
    const b = blocksOf(model);
    const pts = [];
    const mono = MONO[model] || 32;
    for (let i = 1; i <= mono; i++) pts.push([i <= b.mono ? (i - 1) * 10 + 3 : b.mono2 + (i - 25) * 10 + 3]);
    for (let k = 0; k < 3; k++) pts.push([b.stereo + k * 20 + 6, b.stereo + k * 20 + 7]);
    for (let m = 0; m < 4; m++) pts.push([b.mixes + m * 10 + 5]);
    for (let m = 0; m < 4; m++) pts.push([b.mixes + 40 + m * 20 + 10, b.mixes + 40 + m * 20 + 11]);
    return pts;
  }
  // 7-bit MIDI data -> the 8-bit bytes it carries: each group of up to 8 starts with the top bits
  function unpack7(d) {
    const out = [];
    for (let i = 0; i < d.length; i += 8) {
      const top = d[i];
      const n = Math.min(7, d.length - i - 1);
      for (let j = 0; j < n; j++) out.push(d[i + 1 + j] | (((top >> (6 - j)) & 1) << 7));
    }
    return out;
  }
  function pack7(bytes) {
    const out = [];
    for (let i = 0; i < bytes.length; i += 7) {
      const g = bytes.slice(i, i + 7);
      let top = 0;
      g.forEach((x, j) => { top |= ((x >> 7) & 1) << (6 - j); });
      out.push(top, ...g.map((x) => x & 127));
    }
    return out;
  }
  // 16-bit meter <-> dB: 7Q8 with 8000 as 0 dB
  const meterDb = (u16) => (u16 - 0x8000) / 256;
  const dbMeter = (db) => Math.max(0, Math.min(0xffff, Math.round(db * 256) + 0x8000));

  // an address: the channel, the parameter and the destination in one number
  const key = (ch, id, vx) => (ch << 14) | (id << 7) | vx;
  const map = new Map(); // path -> {ch, id, vx, kind}

  function layoutFor(model) {
    map.clear();
    const add = (path, ch, id, vx, kind) => { map.set(path, { ch, id, vx, kind, addr: key(ch, id, vx) }); return path; };
    const named = (base, ch) => add(base + '/name', ch, ID.name, 0, 'name');

    function strip(kind, index, label, ch) {
      const base = '/' + kind + '/' + index;
      const s = {
        id: kind + index, kind, index, label,
        fader: add(base + '/lr', ch, ID.fader, 7, 'level'),
        on: add(base + '/mute', ch, ID.mute, 0x7f, 'mute'),
        pan: add(base + '/pan', ch, ID.pan, 7, 'pan'),
        name: named(base, ch), color: null, muteGroups: null, solo: null, meter: null,
        sends: {},
      };
      for (const [, vx] of MIXES) s.sends['mix' + (vx + 1)] = add(base + '/mix/' + (vx + 1), ch, ID.send, vx, 'level');
      return s;
    }

    const inputs = [];
    const mono = MONO[model] || 32;
    for (let i = 1; i <= mono; i++) inputs.push(strip('ip', i, 'Ip ' + i, 0x20 + i - 1));
    for (let i = 1; i <= 3; i++) inputs.push(strip('st', i, 'ST ' + i, 0x40 + i - 1));
    const returns = [];
    for (let r = 1; r <= RETURNS; r++) returns.push(strip('fx', r, 'FX Rtn ' + r, 0x08 + r - 1));
    const targets = MIXES.map(([label, vx]) => {
      const ch = 0x60 + vx;
      const base = '/mix/' + (vx + 1);
      return { id: 'mix' + (vx + 1), label, slot: String(vx + 1), master: {
        id: 'mixm' + (vx + 1), kind: 'mix', index: vx + 1, label,
        fader: add(base + '/lvl', ch, ID.fader, 7, 'level'),
        on: add(base + '/mute', ch, ID.mute, 0x7f, 'mute'),
        pan: null, name: named(base, ch), color: null, sends: null, muteGroups: null, solo: null, meter: null,
      } };
    });
    const main = {
      id: 'lr', kind: 'lr', index: 0, label: 'LR',
      fader: add('/lr/lvl', LR, ID.fader, 7, 'level'),
      on: add('/lr/mute', LR, ID.mute, 0x7f, 'mute'),
      pan: null, name: named('/lr', LR), color: null, sends: null, muteGroups: null, solo: null, meter: null,
    };
    const muteGroups = [];
    // meters: inputs, the mix masters and LR (the order of meterPoints); FX returns have none yet
    inputs.forEach((st, i) => { st.meter = i; });
    targets.forEach((t, i) => { t.master.meter = inputs.length + i; });
    main.meter = inputs.length + targets.length;
    for (let g = 1; g <= MUTE_GROUPS; g++) muteGroups.push({ id: 'mg' + g, label: 'Mute ' + g, path: add('/mgrp/' + g, 0x50 + g - 1, ID.mute, 0x7f, 'group'), bit: 1 << (g - 1) });

    const params = [];
    const p = (path, type) => { if (path) params.push({ path, type }); };
    for (const s of inputs.concat(returns, targets.map((t) => t.master), [main])) {
      p(s.fader, 'f'); p(s.on, 'i'); p(s.pan, 'f'); p(s.name, 's');
      if (s.sends) for (const x of Object.values(s.sends)) p(x, 'f');
    }
    for (const g of muteGroups) p(g.path, 'i');
    return { model: model || 'Qu', inputs, returns, targets, main, dcas: [], muteGroups, params, clearSolo: null, meters: [{ address: 'qu', offset: 0 }] };
  }
  layoutFor('Qu-32'); // fills the address map

  // fader position (0..1, 0.75 = 0 dB) <-> the 7-bit level
  function posToRaw(f) {
    f = Math.min(1, Math.max(0, f));
    return Math.round(f <= 0.75 ? (f / 0.75) * ZERO_DB : ZERO_DB + ((f - 0.75) / 0.25) * (0x7f - ZERO_DB));
  }
  const rawToPos = (v) => (v <= ZERO_DB ? (v / ZERO_DB) * 0.75 : 0.75 + ((v - ZERO_DB) / (0x7f - ZERO_DB)) * 0.25);
  function panToRaw(x) {
    x = Math.min(1, Math.max(0, x));
    return x <= 0.5 ? Math.round(x * 2 * CENTRE) : CENTRE + Math.round((x - 0.5) * 2 * (RIGHT - CENTRE));
  }
  const rawToPan = (v) => (v <= CENTRE ? v / CENTRE / 2 : 0.5 + Math.min(1, (v - CENTRE) / (RIGHT - CENTRE)) / 2);

  const kindOf = (path) => (map.get(path) || {}).kind;
  const isHeader = (sx, ch) => sx.length > 10 && HEADER.every((b, i) => sx[i] === b) && (ch == null || sx[8] === ch);
  const ascii = (s) => Array.from(String(s), (c) => c.charCodeAt(0) & 127);

  return {
    id: 'qu',
    family: 'Allen & Heath Qu',
    session: 'nrpn',
    port: 51325,
    midiChannel: 0, // until the Qu says its own
    models: ['Qu-16', 'Qu-24', 'Qu-32', 'Qu-Pac', 'Qu-SB'],
    keepAliveMs: 1000,
    layout: () => layoutFor('Qu-32'),
    layoutFor,
    address: (path) => map.get(path).addr,
    key: (msb, lsb, vc, vf) => ({ addr: key(msb, lsb, vf), raw: vc }),
    noteKey: (note, velocity) => ({ addr: key(note, ID.mute, 0x7f), raw: velocity >= 0x40 ? 1 : 0 }),
    encode(ch, path, raw) {
      const a = map.get(path);
      if (a.kind === 'mute' || a.kind === 'group') {
        const n = 0x90 | (ch & 15);
        return [n, a.ch, raw ? 0x7f : 0x3f, n, a.ch, 0];
      }
      const b = 0xb0 | (ch & 15);
      return [b, 0x63, a.ch, b, 0x62, a.id, b, 0x06, raw & 127, b, 0x26, a.vx];
    },
    // Get System State, as the Qu-Pad app (flag 1): the Qu then also expects Active Sensing
    hello: () => HEADER.concat([ALL_CALL, 0x10, 0x01, 0xf7]),
    helloReply(sx) {
      if (!isHeader(sx) || sx[9] !== 0x11) return null;
      return { channel: sx[8] & 15, model: BOXES[sx[10]] || 'Qu' };
    },
    syncEnd: (sx) => isHeader(sx) && sx[9] === 0x14,
    query(ch, path) {
      const a = map.get(path);
      return a && a.kind === 'name' ? HEADER.concat([ch & 15, 0x01, a.ch, 0xf7]) : null;
    },
    // meters: ask (on) or stop; an answer -> {address, db: the tab's meters} or null
    meterRequest: (ch, on) => HEADER.concat([ch & 15, 0x12, on ? 1 : 0, 0xf7]),
    meterGapMs: 100,
    parseMeters(sx, model) {
      if (!isHeader(sx) || sx[9] !== 0x13) return null;
      const raw = unpack7(sx.slice(10, -1));
      const at = (i) => (2 * i + 1 < raw.length ? meterDb((raw[2 * i] << 8) | raw[2 * i + 1]) : null);
      return { address: 'qu', db: meterPoints(model).map((pts) => {
        let top = null;
        for (const i of pts) { const v = at(i); if (v != null && (top == null || v > top)) top = v; }
        return top == null ? -90 : top;
      }) };
    },
    meterPoints, unpack7, pack7, dbMeter, meterDb,
    fromSysex(sx) {
      if (!isHeader(sx) || sx[9] !== 0x02 || sx.length < 12) return [];
      return [{ addr: key(sx[10], ID.name, 0), raw: String.fromCharCode(...sx.slice(11, -1)).replace(/\0+$/, '').trim() }];
    },
    // the tab's value -> the Qu's value, and back
    toValue(path, v) {
      const k = kindOf(path);
      if (k === 'level') return posToRaw(v);
      if (k === 'pan') return panToRaw(v);
      if (k === 'mute') return v ? 0 : 1; // the tab's "on" (1 = playing) is the Qu's mute off
      return v ? 1 : 0;
    },
    fromValue(path, raw) {
      const k = kindOf(path);
      if (k === 'level') return rawToPos(raw);
      if (k === 'pan') return rawToPan(raw);
      if (k === 'mute') return raw ? 0 : 1;
      if (k === 'name') return String(raw);
      return raw ? 1 : 0;
    },
    ascii,
    header: HEADER,
    colorOf: () => '#3a3a3a',
    colorInverted: () => false,
    connectHelp: [
      'Cable the Qu’s network port to a router or access point and put this phone or tablet on that network.',
      'Type the Qu’s IP address (Setup > Network on the Qu) and tap Connect. The Qu is not found by Scan, and it takes one MIDI connection at a time: close the Qu-Pad app or other MIDI remotes first.',
      'Names, faders, mutes, pans, mix sends, mute groups and meters. No colours, PAFL or FX return meters yet.',
    ],
  };
});
