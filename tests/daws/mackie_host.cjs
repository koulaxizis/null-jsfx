// A software DAW that drives a Mackie Control surface, for the tests of the Mackie tab of Remote
// for DAWs (android/web/daws/mackie/mackie.js): a network MIDI session (rtpmidi_peer.cjs) in
// front of a mixer of tracks that does with the surface what a DAW's Mackie Control support does
// (Logic Control / Mackie Control protocol): after the surface's hello it confirms and sends the
// whole bank (names in the display's top row, faders, mute/solo/arm lights, pan rings, time),
// moves a track's volume with the fader, toggles mute, solo and arm with their buttons, turns the
// pan with the V-pot (one tick 1/64 of the range here) and centres it on a push, banks by eight,
// plays, stops (a second Stop goes to the start), records and cycles, and sends meters.
// It is written from the protocol on its own, not from mackie.js. Not a real DAW (Bible C26).
//   start({tracks, port}) -> {port, tracks, transport, bank, refreshes, touches, peer, meter(i, lv),
//     rename(i, name), setVolume(i, v), tick(), close()}
// NULL JSFX • https://nulljsfx.tech • MIT License
'use strict';
const peerSession = require('./rtpmidi_peer.cjs');

const HEAD = [0xf0, 0x00, 0x00, 0x66, 0x14];

async function start(opts = {}) {
  const peer = await peerSession.start({ port: opts.port, name: 'Studio Mac' });
  const count = opts.tracks || 11;
  const tracks = [];
  for (let i = 0; i < count; i++) {
    tracks.push({ name: ['Kick', 'Snare', 'Hats', 'Bass', 'Keys', 'Guitar', 'Vox', 'Pad', 'Choir', 'FX', 'Organ', 'Perc'][i % 12] + (i >= 12 ? ' ' + i : ''),
      vol: 0.75, pan: 0.5, mute: false, solo: false, arm: false });
  }
  const master = { vol: 0.8 };
  const transport = { play: false, rec: false, cycle: false, position: 0 };
  const host = { port: peer.port, tracks, master, transport, bank: 0, refreshes: 0, touches: [], peer, versionAsked: false };
  const send = (m) => peer.send(m);

  const pad = (s) => (s + '       ').slice(0, 7);
  function lcd() {
    let top = '';
    let bottom = '';
    for (let n = 0; n < 8; n++) {
      const t = tracks[host.bank + n];
      top += pad(t ? t.name.slice(0, 6) : '');
      bottom += pad(t ? (t.pan === 0.5 ? '  <C>' : t.pan < 0.5 ? ' L' + Math.round((0.5 - t.pan) * 200) : ' R' + Math.round((t.pan - 0.5) * 200)) : '');
    }
    const chars = (s) => Array.from(s, (c) => c.charCodeAt(0) & 0x7f);
    send([HEAD.concat([0x12, 0], chars(top), [0xf7]), HEAD.concat([0x12, 56], chars(bottom), [0xf7])]);
  }
  const bend = (ch, v) => { const r = Math.round(v * 16383); return [0xe0 | ch, r & 0x7f, r >> 7]; };
  const led = (note, on) => [0x90, note, on ? 0x7f : 0];
  const ring = (n, t) => [0xb0, 0x30 + n, t ? 0x40 | (1 + Math.round(t.pan * 10)) : 0];
  function strip(n) {
    const t = tracks[host.bank + n];
    send([bend(n, t ? t.vol : 0), led(0x00 + n, t && t.arm), led(0x08 + n, t && t.solo), led(0x10 + n, t && t.mute), ring(n, t)]);
  }
  function lights() {
    send([led(0x5e, transport.play), led(0x5d, !transport.play), led(0x5f, transport.rec), led(0x56, transport.cycle)]);
  }
  function time() { // bars.beats.subdivision.ticks from the position in beats
    const bar = 1 + Math.floor(transport.position / 4);
    const beat = 1 + (transport.position % 4);
    const digits = String(bar).padStart(3, '0') + String(beat).padStart(2, '0') + '01' + '000';
    const out = [];
    const dots = new Set([2, 4, 6]); // after the bar, the beat and the subdivision
    for (let i = 0; i < 10; i++) out.push([0xb0, 0x49 - i, (digits.charCodeAt(i) & 0x3f) | (dots.has(i) ? 0x40 : 0)]);
    send(out);
  }
  function refresh() {
    host.refreshes++;
    lcd();
    for (let n = 0; n < 8; n++) strip(n);
    send([bend(8, master.vol)]);
    lights();
    time();
  }

  peer.onMidi((m) => {
    if (m[0] === 0xf0) {
      if (m.length < 7 || HEAD.some((b, i) => m[i] !== b)) return;
      if (m[5] === 0x01) { // the surface's hello: our answer, then everything
        send([HEAD.concat([0x02], m.slice(6, 13), [0x11, 0x22, 0x33, 0x44, 0xf7])]);
        send([HEAD.concat([0x13, 0x00, 0xf7])]);
      } else if (m[5] === 0x03) {
        refresh();
      } else if (m[5] === 0x14) {
        host.versionAsked = String.fromCharCode(...m.slice(6, -1));
      }
      return;
    }
    const st = m[0] & 0xf0;
    const ch = m[0] & 0x0f;
    if (st === 0xe0) {
      const v = ((m[2] << 7) | m[1]) / 16383;
      if (ch === 8) master.vol = v;
      else { const t = tracks[host.bank + ch]; if (t) t.vol = v; }
      return;
    }
    if (st === 0x90 && m[1] >= 0x68 && m[1] <= 0x70) { host.touches.push([m[1] - 0x68, m[2] > 0]); return; }
    if (st === 0x90 && m[2] > 0) { // a press (releases do nothing here)
      const note = m[1];
      const n = note & 7;
      const t = tracks[host.bank + n];
      if (note < 0x08) { if (t) { t.arm = !t.arm; send([led(note, t.arm)]); } } else if (note < 0x10) { if (t) { t.solo = !t.solo; send([led(note, t.solo)]); } } else if (note < 0x18) { if (t) { t.mute = !t.mute; send([led(note, t.mute)]); } } else if (note >= 0x20 && note < 0x28) { if (t) { t.pan = 0.5; send([ring(n, t)]); lcd(); } } else if (note === 0x2a) { host.panMode = true; } else if (note === 0x2e || note === 0x2f) {
        const b = note === 0x2f ? (host.bank + 8 < tracks.length ? host.bank + 8 : host.bank) : Math.max(0, host.bank - 8); // a bank past the last track shows blank strips
        if (b !== host.bank) { host.bank = b; lcd(); for (let k = 0; k < 8; k++) strip(k); }
      } else if (note === 0x5e) { transport.play = true; lights(); } else if (note === 0x5d) {
        if (!transport.play) { transport.position = 0; time(); }
        transport.play = false; transport.rec = false; lights();
      } else if (note === 0x5f) { transport.rec = !transport.rec; lights(); } else if (note === 0x56) { transport.cycle = !transport.cycle; lights(); }
      return;
    }
    if (st === 0xb0 && m[1] >= 0x10 && m[1] < 0x18) { // a V-pot
      const n = m[1] - 0x10;
      const t = tracks[host.bank + n];
      if (!t) return;
      const ticks = m[2] & 0x40 ? -(m[2] & 0x0f) : m[2] & 0x0f;
      t.pan = Math.max(0, Math.min(1, t.pan + ticks / 64));
      send([ring(n, t)]);
    }
  });

  return Object.assign(host, {
    meter(n, lv) { send([[0xd0, (n << 4) | lv]]); },
    rename(i, name) { tracks[i].name = name; lcd(); },
    setVolume(i, v) { tracks[i].vol = v; const n = i - host.bank; if (n >= 0 && n < 8) send([bend(n, v)]); },
    tick() { transport.position++; time(); },
    close() { peer.close(); },
  });
}

module.exports = { start };

if (require.main === module) {
  start({ port: +(process.argv[2] || 5004) }).then((h) => console.log('software DAW (Mackie Control) on network MIDI UDP ' + h.port + ' and ' + (h.port + 1)));
}
