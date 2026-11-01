// A software Behringer WING for the tests and for trying the Mixers tab without hardware. It keeps
// every parameter of the profile's layout in the WING's own units (fdr and lvl in dB, pan
// -100..100, mute 1 = muted), answers "/?" and queries the way the WING does (,sff text, raw,
// value for numbers; ,sfi for integers; ,s for text), takes sets without echoing them, and sends
// every change made elsewhere to its one subscriber ("/*S", for 10 s; a newer subscriber
// replaces the older one), as one native argument.
// It follows our reading of the protocol (android/web/mixers/wing/profile.js), so it proves the
// app is consistent with that reading, not that the reading matches a real WING (Bible C17).
//   node tests/mixers/wing_sim.cjs [--model ngc-full|ngc-compact|ngc-rack] [--port 2223] [--name WING-SIM]
// NULL JSFX • https://nulljsfx.tech • MIT License
'use strict';
const dgram = require('dgram');
const path = require('path');
const web = path.resolve(__dirname, '../../android/web/mixers');
const Osc = require(path.join(web, 'core/osc.js'));
const profile = require(path.join(web, 'wing/profile.js'));

function start(opts = {}) {
  const model = opts.model || 'ngc-full';
  const name = opts.name || 'WING-SIM';
  const subscribeMs = opts.subscribeMs || 10000;
  const layout = profile.layout(model);
  const state = new Map();
  const types = new Map();
  for (const p of layout.params) {
    types.set(p.path, p.type);
    let v = p.type === 's' ? '' : 0;
    if (/\/fdr$/.test(p.path)) v = 0;        // faders at 0 dB
    if (/\/lvl$/.test(p.path)) v = -144;     // sends off
    state.set(p.path, v);
  }
  state.set('/ch/1/name', 'Kick');
  state.set('/ch/1/col', 9);
  state.set('/ch/2/name', 'Vocal');
  state.set('/ch/2/col', 7);
  state.set('/bus/1/name', 'Drummer');

  const isNum = (p) => /\/(fdr|lvl|pan)$/.test(p);
  const text = (p, v) => (/\/(fdr|lvl)$/.test(p) ? (v <= -144 ? '-oo' : v.toFixed(1)) : String(v));
  const raw = (p, v) => (/\/(fdr|lvl)$/.test(p) ? Math.max(0, (v + 144) / 154) : /\/pan$/.test(p) ? (v + 100) / 200 : v ? 1 : 0);
  const answer = (p) => {
    const v = state.get(p);
    if (types.get(p) === 's') return [{ t: 's', v }];
    if (isNum(p)) return [{ t: 's', v: text(p, v) }, { t: 'f', v: raw(p, v) }, { t: 'f', v }];
    return [{ t: 's', v: text(p, v) }, { t: 'f', v: raw(p, v) }, { t: 'i', v }];
  };
  const native = (p) => {
    const v = state.get(p);
    return [{ t: types.get(p) === 's' ? 's' : isNum(p) ? 'f' : 'i', v }];
  };

  const sock = dgram.createSocket({ type: 'udp4', reuseAddr: true });
  let sub = null; // { host, port, until }: the WING has one subscriber, the newest
  const received = [];
  const send = (address, args, host, port) => sock.send(Osc.encode(address, args), port, host);

  function push(p, from) {
    if (!sub || sub.until < Date.now()) { sub = null; return; }
    if (from && from.address === sub.host && from.port === sub.port) return; // no echo of a set
    send(p, native(p), sub.host, sub.port);
  }

  sock.on('message', (buf, rinfo) => {
    let msgs;
    try { msgs = Osc.decode(new Uint8Array(buf)); } catch (_) { return; }
    for (const m of msgs) {
      received.push(m);
      const a = m.address;
      if (a === '/?') {
        send('/?', [{ t: 's', v: ['WING', '127.0.0.1', name, model, 'S-SIM', '3.0.5'].join(',') }], rinfo.address, rinfo.port);
      } else if (a === '/*S') {
        sub = { host: rinfo.address, port: rinfo.port, until: Date.now() + subscribeMs };
      } else if (types.has(a)) {
        if (!m.args.length) send(a, answer(a), rinfo.address, rinfo.port);
        else {
          let v = m.args[0].v;
          if (types.get(a) === 's') v = String(v);
          else if (isNum(a)) v = Number(v);
          else v = Math.round(Number(v));
          if (/\/(fdr|lvl)$/.test(a)) v = Math.min(10, Math.max(-144, v));
          if (/\/pan$/.test(a)) v = Math.min(100, Math.max(-100, v));
          state.set(a, v);
          push(a, rinfo);
        }
      }
    }
  });

  return new Promise((resolve) => {
    sock.bind(opts.port || 0, opts.host || '127.0.0.1', () => {
      resolve({
        port: sock.address().port,
        state,
        received,
        model,
        // a change made on the console itself (or by another app): sent to the subscriber
        change(p, v) { state.set(p, v); push(p); },
        subscriber: () => (sub && sub.until >= Date.now() ? sub.host + ':' + sub.port : null),
        profile: 'wing',
        close() { sock.close(); },
      });
    });
  });
}

module.exports = { start };

if (require.main === module) {
  const arg = (n, d) => { const i = process.argv.indexOf('--' + n); return i > 0 ? process.argv[i + 1] : d; };
  start({ model: arg('model'), port: +arg('port', 2223), name: arg('name'), host: arg('host', '0.0.0.0') }).then((s) => {
    console.log(`WING simulator (${s.model}) on UDP ${s.port}`);
  });
}
