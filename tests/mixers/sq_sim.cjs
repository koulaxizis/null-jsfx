// A software Allen & Heath SQ for the tests and for trying the Mixers tab without hardware. It
// listens on TCP (the SQ's port is 51325), keeps every parameter of the profile's layout as the
// console's 14-bit value, takes NRPN sets (CC 99, 98, 6, 38), answers a get (CC 99, 98, then
// CC 96 with 7F) with a set message of the current value, and sends every change to every other
// connected remote (not back to the one that made it).
// It follows our reading of the SQ MIDI Protocol (android/web/mixers/sq/profile.js, core/nrpn.js),
// so it proves the app is consistent with that reading, not that the reading matches a real SQ
// (Bible C18).
//   node tests/mixers/sq_sim.cjs [--port 51325]
// NULL JSFX • https://nulljsfx.tech • MIT License
'use strict';
const net = require('net');
const path = require('path');
const web = path.resolve(__dirname, '../../android/web/mixers');
const profile = require(path.join(web, 'sq/profile.js'));
const { setMessage } = require(path.join(web, 'core/nrpn.js'));

const ZERO_DB = 0x76 * 128 + 0x5c;

function start(opts = {}) {
  const ch = opts.channel == null ? profile.midiChannel : opts.channel;
  const layout = profile.layout();
  const state = new Map(); // path -> raw 14-bit value
  const byAddr = new Map();
  for (const p of layout.params) {
    byAddr.set(profile.address(p.path), p.path);
    let v = 0;
    if (/\/(lr|lvl)$/.test(p.path)) v = ZERO_DB; // faders at 0 dB, sends off
    if (/\/pan$/.test(p.path)) v = 8191;          // pans centred
    state.set(p.path, v);
  }
  const clients = new Set();
  const received = []; // {addr, value} of every set
  let gets = 0;

  const send = (sock, p) => sock.write(Buffer.from(setMessage(ch, profile.address(p), state.get(p))));
  function changed(p, from) { for (const c of clients) if (c !== from) send(c, p); }

  function reader(sock) {
    let status = 0;
    let data = [];
    const nrpn = { msb: -1, lsb: -1, vc: -1 };
    const cc = (num, v) => {
      if (num === 0x63) { nrpn.msb = v; nrpn.vc = -1; } else if (num === 0x62) { nrpn.lsb = v; nrpn.vc = -1; } else if (num === 0x06) nrpn.vc = v;
      else if (num === 0x26 && nrpn.msb >= 0 && nrpn.lsb >= 0 && nrpn.vc >= 0) {
        const addr = nrpn.msb * 128 + nrpn.lsb;
        const value = nrpn.vc * 128 + v;
        received.push({ addr, value });
        const p = byAddr.get(addr);
        if (p && state.get(p) !== value) { state.set(p, value); changed(p, sock); }
      } else if (num === 0x60 && v === 0x7f && nrpn.msb >= 0 && nrpn.lsb >= 0) {
        gets++;
        const p = byAddr.get(nrpn.msb * 128 + nrpn.lsb);
        if (p) send(sock, p);
      }
    };
    return (buf) => {
      for (const x of buf) {
        if (x >= 0xf8) continue;
        if (x >= 0x80) { status = x; data = []; continue; }
        data.push(x);
        if (data.length < 2) continue;
        if (status === (0xb0 | ch)) cc(data[0], data[1]);
        data = [];
      }
    };
  }

  const server = net.createServer((sock) => {
    clients.add(sock);
    const read = reader(sock);
    sock.on('data', read);
    sock.on('error', () => {});
    sock.on('close', () => clients.delete(sock));
  });
  return new Promise((resolve) => {
    server.listen(opts.port || 0, opts.host || '127.0.0.1', () => {
      resolve({
        port: server.address().port,
        state,
        received,
        get gets() { return gets; },
        clients: () => clients.size,
        // a change made on the console's own surface: every remote hears it
        change(p, raw) { state.set(p, raw); changed(p, null); },
        // the console goes away: every connection drops
        drop() { for (const c of clients) c.destroy(); },
        close() { for (const c of clients) c.destroy(); server.close(); },
      });
    });
  });
}

module.exports = { start, ZERO_DB };

if (require.main === module) {
  const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
  start({ port: +arg('--port', 51325), host: '0.0.0.0' }).then((s) => console.log('SQ simulator on TCP port ' + s.port));
}
