// NULL Remote, Mixers: the Allen & Heath dLive and Avantis profile (one MIDI scheme for both).
// MIDI over TCP port 51325 (core/nrpn.js with this profile's hooks). Sources: Allen & Heath's
// "dLive MIDI Over TCP/IP Protocol" (firmware V1.9) and "Avantis TCP/IP Protocol" (firmware
// V1.10). The encrypted port (51327, dLive) is not used.
//   Channels: each channel type has its own MIDI channel from the base N set on the console
//     (Utility > Control > MIDI, 1 to 12): inputs on N, groups N+1, aux N+2, matrices N+3, and FX
//     sends, FX returns, mains, DCAs and mute groups on N+4. CH (a note number) picks the channel:
//       dLive:   inputs 1-128 00..7F; mono aux 1-62 00..3D, stereo aux 1-31 40..5E; FX returns 1-16
//                20..2F; mains 1-6 30..35; DCA 1-24 36..4D; mute groups 1-8 4E..55.
//       Avantis: inputs 1-64 00..3F; mono aux 1-40 00..27, stereo aux 1-20 40..53; FX returns
//                1-12 20..2B; mains 1-3 30..32; DCA 1-16 36..45; mute groups 1-8 46..4D.
//   Fader: BN 63 CH, BN 62 17, BN 06 LV (7 bits, no CC 38): 00 -inf, 6B 0 dB, 7F +10 dB; the
//     tables between them go about 2 steps a dB, and this profile goes in straight lines from the
//     tab's 0, 0.75 and 1, so 0 dB and +10 dB are exact (Bible C24).
//   Mute: a note on, 9N CH 7F muted or 9N CH 3F not, then 9N CH 00. A mute group is muted the
//     same way on N+4.
//   Send: F0 00 00 1A 50 10 01 00 0N 0D CH SndN SndCH LV F7 (SndN, SndCH: the mix's MIDI channel
//     and note; LV as the fader).
//   Name: 0N 01 CH asks, 0N 02 CH <ASCII> answers; colour: 0N 04 CH asks, 0N 05 CH <0-7> answers
//     (off, red, green, yellow, blue, purple, light blue, white).
//   Get (dLive's document; the Avantis document gives none, so they are sent and may go
//     unanswered, Bible C24): fader 0N 05 0B 17 CH, mute 0N 05 09 CH, send 0N 05 0F 0D CH SndN
//     SndCH; the console is taken to answer with the message that sets the value.
//   Connect: no hello in the protocol, so the session asks Input 65's name, then Input 1's, on
//     every MIDI channel 1-12; the channel that answers is N, and an answer for Input 65 (which an
//     Avantis has not) means a dLive. Then names, colours, faders and mutes; the mixes whose
//     names answered are the ones the console has (stereo or mono, as set up on it), and only
//     their sends are asked.
// Not in the protocol: pan, meters, solo. Not covered yet: groups, matrices, FX sends as mixes,
// mains 2-6, scenes.
// NULL JSFX • https://nulljsfx.tech • MIT License
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else (root.NullMixerProfiles = root.NullMixerProfiles || {}).dlive = api;
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  const HEADER = [0xf0, 0x00, 0x00, 0x1a, 0x50, 0x10, 0x01, 0x00];
  const MODELS = {
    dLive: { inputs: 128, monoAux: 62, stAux: 31, returns: 16, dca: 24, dcaCh: 0x36, mgCh: 0x4e },
    Avantis: { inputs: 64, monoAux: 40, stAux: 20, returns: 12, dca: 16, dcaCh: 0x36, mgCh: 0x46 },
  };
  const OFF = { input: 0, group: 1, aux: 2, matrix: 3, fx: 4 }; // MIDI channel above N
  const KIND = { fader: 1, mute: 2, name: 3, color: 4, send: 5, group: 6 };
  const ZERO_DB = 0x6b;
  const TOP = 0x7f;
  const COLORS = ['#3a3a3a', '#c83c3c', '#3c9c50', '#c8b43c', '#3c64c8', '#8c50b4', '#50a0c8', '#d2d2d2'];

  // an address: what it is, the channel type's MIDI offset and note, and for a send the mix's
  const key = (kind, off, ch, dOff = 0, dCh = 0) => ((kind * 8 + off) * 128 + ch) * 1024 + dOff * 128 + dCh;
  const map = new Map(); // path -> {kind, off, ch, dOff, dCh, addr}
  const groupNotes = new Set(); // the mute groups' notes on N+4 (a note on there is a group's)

  function layoutFor(model) {
    const M = MODELS[model] || MODELS.dLive;
    map.clear();
    groupNotes.clear();
    for (let g = 0; g < 8; g++) groupNotes.add(M.mgCh + g);
    const add = (path, kind, off, ch, dOff = 0, dCh = 0) => {
      map.set(path, { kind, off, ch, dOff, dCh, addr: key(KIND[kind], off, ch, dOff, dCh) });
      return path;
    };
    // the mixes an input or FX return sends to: every mono and stereo aux (prune keeps the ones
    // the console answers for)
    const mixes = [];
    for (let a = 1; a <= M.monoAux; a++) mixes.push({ id: 'aux' + a, label: 'Aux ' + a, ch: a - 1 });
    for (let a = 1; a <= M.stAux; a++) mixes.push({ id: 'saux' + a, label: 'St Aux ' + a, ch: 0x40 + a - 1 });

    function strip(kind, index, label, off, ch, sends) {
      const base = '/' + kind + '/' + index;
      const s = {
        id: kind + index, kind, index, label,
        fader: add(base + '/lvl', 'fader', off, ch),
        on: add(base + '/mute', 'mute', off, ch),
        pan: null,
        name: add(base + '/name', 'name', off, ch),
        color: add(base + '/col', 'color', off, ch),
        muteGroups: null, solo: null, meter: null,
        sends: sends ? {} : null,
      };
      if (sends) for (const m of mixes) s.sends[m.id] = add(base + '/send/' + m.id, 'send', off, ch, OFF.aux, m.ch);
      return s;
    }

    const inputs = [];
    for (let i = 1; i <= M.inputs; i++) inputs.push(strip('ip', i, 'Ip ' + i, OFF.input, i - 1, true));
    const returns = [];
    for (let r = 1; r <= M.returns; r++) returns.push(strip('fxr', r, 'FX Rtn ' + r, OFF.fx, 0x20 + r - 1, true));
    const targets = mixes.map((m, i) => ({ id: m.id, label: m.label, slot: String(i + 1), master: strip(m.id.startsWith('s') ? 'saux' : 'aux', +m.id.replace(/\D/g, ''), m.label, OFF.aux, m.ch, false) }));
    const main = strip('main', 1, 'Main', OFF.fx, 0x30, false);
    const dcas = [];
    for (let d = 1; d <= M.dca; d++) dcas.push(strip('dca', d, 'DCA ' + d, OFF.fx, M.dcaCh + d - 1, false));
    const muteGroups = [];
    for (let g = 1; g <= 8; g++) muteGroups.push({ id: 'mg' + g, label: 'Mute ' + g, path: add('/mgrp/' + g, 'group', OFF.fx, M.mgCh + g - 1), bit: 1 << (g - 1) });
    return withParams({ model: model || 'dLive', inputs, returns, targets, main, dcas, muteGroups, clearSolo: null, meters: [] });
  }
  function withParams(L) {
    const params = [];
    const p = (path, type) => { if (path) params.push({ path, type }); };
    for (const s of L.inputs.concat(L.returns, L.targets.map((t) => t.master), [L.main], L.dcas)) {
      p(s.fader, 'f'); p(s.on, 'i'); p(s.name, 's'); p(s.color, 'i');
      if (s.sends) for (const x of Object.values(s.sends)) p(x, 'f');
    }
    for (const g of L.muteGroups) p(g.path, 'i');
    L.params = params;
    return L;
  }
  layoutFor('dLive'); // fills the address map

  // keep the mixes whose names answered (none answered: the first eight mono aux), and only
  // their sends
  function prune(L, has) {
    let targets = L.targets.filter((t) => has(t.master.name));
    if (!targets.length) targets = L.targets.slice(0, 8);
    const keep = new Set(targets.map((t) => t.id));
    targets.forEach((t, i) => { t.slot = String(i + 1); });
    for (const s of L.inputs.concat(L.returns)) {
      for (const id of Object.keys(s.sends)) if (!keep.has(id)) delete s.sends[id];
    }
    return withParams(Object.assign({}, L, { targets }));
  }

  // fader position (0..1, 0.75 = 0 dB) <-> the 7-bit level
  function posToRaw(f) {
    f = Math.min(1, Math.max(0, f));
    return Math.round(f <= 0.75 ? (f / 0.75) * ZERO_DB : ZERO_DB + ((f - 0.75) / 0.25) * (TOP - ZERO_DB));
  }
  const rawToPos = (v) => (v <= ZERO_DB ? (v / ZERO_DB) * 0.75 : 0.75 + ((v - ZERO_DB) / (TOP - ZERO_DB)) * 0.25);

  const isHeader = (sx) => sx.length > 10 && HEADER.every((b, i) => sx[i] === b);
  const ascii = (s) => Array.from(String(s), (c) => c.charCodeAt(0) & 127);
  const offOf = (ch, base) => (ch - base + 16) % 16;

  return {
    id: 'dlive',
    family: 'Allen & Heath dLive and Avantis',
    session: 'nrpn',
    port: 51325,
    midiChannel: 0, // until a name answers on the console's own
    models: ['dLive', 'Avantis'],
    channels: 5,
    sevenBit: true,
    layout: () => layoutFor('dLive'),
    layoutFor,
    prune,
    isLate: (path) => (map.get(path) || {}).kind === 'send',
    address: (path) => map.get(path).addr,
    key(msb, lsb, vc, vf, ch, base) {
      if (lsb !== 0x17) return null; // main assign, DCA and mute group assign: not shown
      return { addr: key(KIND.fader, offOf(ch, base), msb), raw: vc };
    },
    noteKey(note, velocity, ch, base) {
      const off = offOf(ch, base);
      const kind = off === OFF.fx && groupNotes.has(note) ? KIND.group : KIND.mute;
      return { addr: key(kind, off, note), raw: velocity >= 0x40 ? 1 : 0 };
    },
    encode(base, path, raw) {
      const a = map.get(path);
      const n = (base + a.off) & 15;
      if (a.kind === 'mute' || a.kind === 'group') return [0x90 | n, a.ch, raw ? 0x7f : 0x3f, 0x90 | n, a.ch, 0];
      if (a.kind === 'send') return HEADER.concat([n, 0x0d, a.ch, (base + a.dOff) & 15, a.dCh, raw & 127, 0xf7]);
      const b = 0xb0 | n;
      return [b, 0x63, a.ch, b, 0x62, 0x17, b, 0x06, raw & 127];
    },
    // Input 65's name, then Input 1's, on every MIDI channel 1-12
    hello() {
      const out = [];
      for (let n = 0; n < 12; n++) out.push(...HEADER, n, 0x01, 0x40, 0xf7, ...HEADER, n, 0x01, 0x00, 0xf7);
      return out;
    },
    helloReply(sx) {
      if (!isHeader(sx) || sx[9] !== 0x02 || (sx[10] !== 0x40 && sx[10] !== 0x00)) return null;
      return { channel: sx[8] & 15, model: sx[10] === 0x40 ? 'dLive' : 'Avantis' };
    },
    query(base, path) {
      const a = map.get(path);
      if (!a) return null;
      const n = (base + a.off) & 15;
      if (a.kind === 'name') return HEADER.concat([n, 0x01, a.ch, 0xf7]);
      if (a.kind === 'color') return HEADER.concat([n, 0x04, a.ch, 0xf7]);
      if (a.kind === 'fader') return HEADER.concat([n, 0x05, 0x0b, 0x17, a.ch, 0xf7]);
      if (a.kind === 'mute' || a.kind === 'group') return HEADER.concat([n, 0x05, 0x09, a.ch, 0xf7]);
      if (a.kind === 'send') return HEADER.concat([n, 0x05, 0x0f, 0x0d, a.ch, (base + a.dOff) & 15, a.dCh, 0xf7]);
      return null;
    },
    fromSysex(sx, base) {
      if (!isHeader(sx) || sx.length < 12) return [];
      const off = offOf(sx[8], base);
      if (off > 4) return [];
      if (sx[9] === 0x02) return [{ addr: key(KIND.name, off, sx[10]), raw: String.fromCharCode(...sx.slice(11, -1)).replace(/\0+$/, '').trim() }];
      if (sx[9] === 0x05 && sx.length === 13) return [{ addr: key(KIND.color, off, sx[10]), raw: sx[11] & 7 }];
      if (sx[9] === 0x0d && sx.length === 15) return [{ addr: key(KIND.send, off, sx[10], offOf(sx[11], base), sx[12]), raw: sx[13] }];
      return [];
    },
    toValue(path, v) {
      const k = (map.get(path) || {}).kind;
      if (k === 'fader' || k === 'send') return posToRaw(v);
      if (k === 'mute') return v ? 0 : 1; // the tab's "on" (1 = playing) is the console's mute off
      if (k === 'color') return v | 0;
      return v ? 1 : 0;
    },
    fromValue(path, raw) {
      const k = (map.get(path) || {}).kind;
      if (k === 'fader' || k === 'send') return rawToPos(raw);
      if (k === 'mute') return raw ? 0 : 1;
      if (k === 'name') return String(raw);
      if (k === 'color') return raw | 0;
      return raw ? 1 : 0;
    },
    ascii,
    header: HEADER,
    colorOf: (c) => COLORS[c | 0] || COLORS[0],
    colorInverted: (c) => c === 3 || c === 6 || c === 7, // yellow, light blue, white: dark text
    connectHelp: [
      'Cable a network port of the dLive Surface or MixRack, or the Avantis, to a router or access point and put this phone or tablet on that network.',
      'Type its IP address and tap Connect. It is not found by Scan. The MIDI channel is found by itself (Utility > Control > MIDI on the console).',
      'Names, colours, faders, mutes, sends to the aux mixes set up on the console, the first main, DCAs and mute groups. No pan, meters or solo: its MIDI protocol has none.',
    ],
  };
});
