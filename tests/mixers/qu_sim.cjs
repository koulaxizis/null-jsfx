// A software Allen & Heath Qu for the tests and for trying the Mixers tab without hardware. It
// listens on TCP (the Qu's port is 51325) and takes one connection at a time (a second one is
// closed), sends Active Sensing (FE) every 300 ms and closes a connection that stays quiet, answers
// Get System State with its box id and channel, pushes every value of the profile's layout and
// ends the push, takes NRPN sets (BN 63 CH, 62 ID, 06 VA, 26 VX) and mutes (note on, velocity 40
// or more = muted), answers a name request, and sends a change made on its own surface. Meters:
// 12 01 F7 is answered with one 13 meter message (or, with meterStreamMs, one that often until
// 12 00 F7); a meter the test has not set reads -90 dB.
// It follows our reading of the Qu MIDI Protocol (android/web/mixers/qu/profile.js), so it proves
// the app is consistent with that reading, not that the reading matches a real Qu (Bible C21).
//   node tests/mixers/qu_sim.cjs [--port 51325] [--model Qu-24]
// NULL JSFX • https://nulljsfx.tech • MIT License
'use strict';
const net = require('net');
const path = require('path');
const profile = require(path.resolve(__dirname, '../../android/web/mixers/qu/profile.js'));

const BOX = { 'Qu-16': 1, 'Qu-24': 2, 'Qu-32': 3, 'Qu-Pac': 4, 'Qu-SB': 5 };

function start(opts = {}) {
  const ch = opts.channel == null ? 0 : opts.channel;
  const model = opts.model || 'Qu-24';
  const quietMs = opts.quietMs || 12000;
  const layout = profile.layoutFor(model);
  const state = new Map(); // path -> the Qu's raw value (names: strings)
  const byAddr = new Map();
  const nameOf = new Map(); // channel -> its name's path
  const names = { '/ip/1/name': 'Kick', '/ip/2/name': 'Vox', '/mix/1/name': 'Wedge 1' };
  for (const p of layout.params) {
    byAddr.set(profile.address(p.path), p.path);
    if (/\/name$/.test(p.path)) nameOf.set(profile.address(p.path) >> 14, p.path);
    let v = 0;
    if (/\/(lr|lvl)$/.test(p.path)) v = 0x62; // faders at 0 dB, sends off
    if (/\/pan$/.test(p.path)) v = 0x25;      // pans centred
    if (/\/name$/.test(p.path)) v = names[p.path] || '';
    state.set(p.path, v);
  }
  let client = null;
  let refused = 0;
  const received = []; // every NRPN set and mute: {path, raw}
  let hellos = 0;
  let iPadFlag = -1;
  const meterDb = new Map(); // the Qu's meter index -> dB
  const meterCount = profile.meterPoints(model).flat().reduce((a, b) => Math.max(a, b), 0) + 1 + 200;
  let meterAsks = 0;
  let meterOn = false;
  let meterTimer = null;
  const meterMessage = () => {
    const bytes = [];
    for (let i = 0; i < meterCount; i++) {
      const u = profile.dbMeter(meterDb.has(i) ? meterDb.get(i) : -90);
      bytes.push(u >> 8, u & 255);
    }
    return sysex([0x13].concat(profile.pack7(bytes)));
  };

  const bytesOf = (p) => (/\/name$/.test(p) ? [] : profile.encode(ch, p, state.get(p)));
  const sysex = (body) => Buffer.from(profile.header.concat([ch], body, [0xf7]));
  const send = (sock, bytes) => { if (sock && !sock.destroyed) sock.write(Buffer.from(bytes)); };

  function reader(sock) {
    let status = 0;
    let data = [];
    let sx = null;
    const nrpn = { msb: -1, lsb: -1, va: -1 };
    const set = (p, raw) => { received.push({ path: p, raw }); state.set(p, raw); };
    const onSysex = (m) => {
      const head = profile.header.every((b, i) => m[i] === b);
      if (!head) return;
      if (m[8] === 0x7f && m[9] === 0x10) { // Get System State
        hellos++;
        iPadFlag = m[10];
        send(sock, sysex([0x11, BOX[model], 1, 9]));
        const all = [];
        for (const p of layout.params) all.push(...bytesOf(p.path));
        send(sock, all);
        send(sock, sysex([0x14]));
      } else if (m[8] === ch && m[9] === 0x12) { // meters on (01) or off (00)
        meterAsks++;
        meterOn = m[10] === 1;
        clearInterval(meterTimer);
        if (meterOn) {
          send(sock, meterMessage());
          if (opts.meterStreamMs) meterTimer = setInterval(() => send(sock, meterMessage()), opts.meterStreamMs);
        }
      } else if (m[8] === ch && m[9] === 0x01) { // get name: 02 CH <ASCII>
        const p = nameOf.get(m[10]);
        if (p) send(sock, sysex([0x02, m[10]].concat(profile.ascii(state.get(p)))));
      }
    };
    return (buf) => {
      for (const x of buf) {
        if (x === 0xf0) { sx = [x]; continue; }
        if (x === 0xf7) {
          if (sx) { sx.push(x); onSysex(sx); }
          sx = null;
          continue;
        }
        if (sx) { sx.push(x); continue; }
        if (x >= 0xf8) continue;
        if (x >= 0x80) { status = x; data = []; continue; }
        data.push(x);
        if (data.length < 2) continue;
        const [a, b] = data;
        data = [];
        if (status === (0xb0 | ch)) {
          if (a === 0x63) { nrpn.msb = b; nrpn.va = -1; } else if (a === 0x62) { nrpn.lsb = b; nrpn.va = -1; } else if (a === 0x06) nrpn.va = b;
          else if (a === 0x26 && nrpn.msb >= 0 && nrpn.lsb >= 0 && nrpn.va >= 0) {
            const p = byAddr.get((nrpn.msb << 14) | (nrpn.lsb << 7) | b);
            if (p) set(p, nrpn.va);
          }
        } else if (status === (0x90 | ch) && b > 0) {
          const p = byAddr.get((a << 14) | (0x7f << 7) | 0x7f);
          if (p) set(p, b >= 0x40 ? 1 : 0);
        }
      }
    };
  }

  const server = net.createServer((sock) => {
    sock.on('error', () => {});
    if (client) { refused++; sock.destroy(); return; } // one TCP MIDI connection at a time
    client = sock;
    const read = reader(sock);
    let last = Date.now();
    send(sock, [0xfe]);
    const alive = setInterval(() => {
      if (Date.now() - last > quietMs) { sock.destroy(); return; }
      send(sock, [0xfe]);
    }, 300);
    sock.on('data', (buf) => { last = Date.now(); read(buf); });
    sock.on('close', () => { clearInterval(alive); clearInterval(meterTimer); meterOn = false; if (client === sock) client = null; });
  });
  return new Promise((resolve) => {
    server.listen(opts.port || 0, opts.host || '127.0.0.1', () => {
      resolve({
        port: server.address().port,
        state,
        received,
        get hellos() { return hellos; },
        get iPadFlag() { return iPadFlag; },
        get refused() { return refused; },
        connected: () => !!client,
        get meterAsks() { return meterAsks; },
        get meterOn() { return meterOn; },
        // a meter in the Qu's own numbering (profile.meterPoints gives a strip's)
        meter(i, db) { meterDb.set(i, db); },
        // a change made on the console's own surface
        change(p, raw) { state.set(p, raw); send(client, bytesOf(p)); },
        // the console goes away: the connection drops
        drop() { if (client) client.destroy(); },
        close() { if (client) client.destroy(); server.close(); },
      });
    });
  });
}

module.exports = { start };

if (require.main === module) {
  const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
  start({ port: +arg('--port', 51325), host: '0.0.0.0', model: arg('--model', 'Qu-24') }).then((s) => console.log('Qu simulator on TCP port ' + s.port));
}
