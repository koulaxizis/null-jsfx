// A software X AIR console for the tests and for trying the Mixers tab without hardware.
// It answers /xinfo, parameter queries and sets for every parameter of the profile's layout,
// pushes changes to /xremote subscribers (for 10 s per renewal, like the console), streams
// /meters/1 blobs every 50 ms to /meters subscribers, and handles /-action/clearsolo.
// It follows our reading of the protocol (android/web/mixers/xair/profile.js), so it proves the
// app is consistent with that reading, not that the reading matches a real console.
//   node tests/mixers/xair_sim.cjs [--model XR18] [--port 10024] [--name XR18-SIM]
'use strict';
const dgram = require('dgram');
const path = require('path');
const web = path.resolve(__dirname, '../../android/web/mixers');
const Osc = require(path.join(web, 'core/osc.js'));
const Levels = require(path.join(web, 'core/levels.js'));
const profile = require(path.join(web, 'xair/profile.js'));

function start(opts = {}) {
  const model = opts.model || 'XR18';
  const name = opts.name || model + '-SIM';
  const subscribeMs = opts.subscribeMs || 10000;
  const layout = profile.layout(model);
  const state = new Map();
  const types = new Map();
  for (const p of layout.params) {
    types.set(p.path, p.type);
    let v = p.type === 's' ? '' : p.type === 'i' ? 0 : 0;
    if (/\/(mix\/)?on$/.test(p.path)) v = 1;                    // strips start unmuted
    if (/\/pan$/.test(p.path)) v = 0.5;
    if (/\/fader$/.test(p.path)) v = Levels.dbToFader(0);
    if (/\/level$/.test(p.path)) v = 0;
    if (/\/config\/name$/.test(p.path)) v = '';
    state.set(p.path, v);
  }
  state.set('/ch/01/config/name', 'Kick');
  state.set('/ch/01/config/color', 1);
  state.set('/ch/02/config/name', 'Vocal');
  state.set('/ch/02/config/color', 3);
  state.set('/bus/1/config/name', 'Drummer');

  const sock = dgram.createSocket({ type: 'udp4', reuseAddr: true });
  const remotes = new Map();  // "host:port" -> expiry
  const meterSubs = new Map();
  const received = [];
  const key = (r) => r.address + ':' + r.port;
  const send = (address, args, host, port) => sock.send(Osc.encode(address, args), port, host);
  const argOf = (p, v) => ({ t: types.get(p), v });

  function push(p, except) {
    const now = Date.now();
    for (const [k, exp] of remotes) {
      if (exp < now) { remotes.delete(k); continue; }
      if (k === except) continue;
      const [h, port] = k.split(':');
      send(p, [argOf(p, state.get(p))], h, +port);
    }
  }

  sock.on('message', (buf, rinfo) => {
    let msgs;
    try { msgs = Osc.decode(new Uint8Array(buf)); } catch (_) { return; }
    for (const m of msgs) {
      received.push(m);
      const a = m.address;
      if (a === '/xinfo') {
        send('/xinfo', ['127.0.0.1', name, model, '1.22'].map((v) => ({ t: 's', v })), rinfo.address, rinfo.port);
      } else if (a === '/xremote') {
        remotes.set(key(rinfo), Date.now() + subscribeMs);
      } else if (a === '/meters') {
        if (m.args[0] && m.args[0].v === '/meters/1') meterSubs.set(key(rinfo), Date.now() + subscribeMs);
      } else if (a === '/-action/clearsolo') {
        for (const p of state.keys()) if (p.startsWith('/-stat/solosw/') && state.get(p)) { state.set(p, 0); push(p); }
      } else if (types.has(a)) {
        if (!m.args.length) send(a, [argOf(a, state.get(a))], rinfo.address, rinfo.port);
        else {
          const v = m.args[0].v;
          state.set(a, types.get(a) === 'i' ? v | 0 : v);
          push(a, key(rinfo));
        }
      }
    }
  });

  // 40 meter values: inputs follow their fader so a test can see movement
  let phase = 0;
  const meterTimer = setInterval(() => {
    const now = Date.now();
    if (!meterSubs.size) return;
    phase += 1;
    const vals = new Array(40).fill(-90);
    const all = layout.inputs.concat(layout.returns, layout.targets.map((t) => t.master), [layout.main]);
    for (const s of all) {
      if (s.meter == null) continue;
      const db = state.get(s.on) ? Math.max(-90, Levels.faderToDb(state.get(s.fader)) - 12 + 6 * Math.sin(phase / 7 + s.meter)) : -90;
      vals[s.meter] = db;
      if (s.kind === 'rtn' || s.kind === 'aux' || s.kind === 'lr') vals[s.meter + 1] = db;
    }
    const blob = Levels.buildMeterBlob(vals, true);
    for (const [k, exp] of meterSubs) {
      if (exp < now) { meterSubs.delete(k); continue; }
      const [h, port] = k.split(':');
      send('/meters/1', [{ t: 'b', v: blob }], h, +port);
    }
  }, 50);

  return new Promise((resolve) => {
    sock.bind(opts.port || 0, opts.host || '127.0.0.1', () => {
      resolve({
        port: sock.address().port,
        state,
        received,
        model,
        // a change made on the console itself (or by another app): pushed to subscribers
        change(p, v) { state.set(p, v); push(p); },
        meterSubscribers: () => meterSubs.size,
        close() { clearInterval(meterTimer); sock.close(); },
      });
    });
  });
}

module.exports = { start };

if (require.main === module) {
  const arg = (n, d) => { const i = process.argv.indexOf('--' + n); return i > 0 ? process.argv[i + 1] : d; };
  start({ model: arg('model', 'XR18'), port: +arg('port', 10024), name: arg('name'), host: arg('host', '0.0.0.0') }).then((s) => {
    console.log(`X AIR simulator (${s.model}) on UDP ${s.port}`);
  });
}
