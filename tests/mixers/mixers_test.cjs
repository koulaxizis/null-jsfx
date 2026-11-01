// Tests the Mixers tab of NULL Remote: the OSC codec, the level laws and meter blobs, the X AIR
// profile, and a live session against the X AIR simulator (xair_sim.cjs) over real UDP.
// The UI test (ui_test.cjs) drives the page in a browser. Usage: node tests/mixers/mixers_test.cjs
'use strict';
const assert = require('assert');
const dgram = require('dgram');
const path = require('path');
const web = path.resolve(__dirname, '../../android/web/mixers');
const Osc = require(path.join(web, 'core/osc.js'));
const Levels = require(path.join(web, 'core/levels.js'));
const Mixer = require(path.join(web, 'core/mixer.js'));
const profile = require(path.join(web, 'xair/profile.js'));
const sim = require('./xair_sim.cjs');

let failures = 0;
const tests = [];
const test = (name, fn) => tests.push([name, fn]);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
async function until(cond, ms = 2000, what = 'condition') {
  const t0 = Date.now();
  while (!cond()) {
    if (Date.now() - t0 > ms) throw new Error('timed out waiting for ' + what);
    await sleep(10);
  }
}

// a UDP transport with the shape the app's NullShell transport has; "broadcast" goes to localhost
function nodeTransport() {
  const sock = dgram.createSocket('udp4');
  const listeners = new Set();
  sock.on('message', (buf, r) => { for (const cb of listeners) cb(new Uint8Array(buf), r.address, r.port); });
  const ready = new Promise((r) => sock.bind(0, '127.0.0.1', r));
  return {
    ready,
    sent: 0,
    send(host, port, bytes) { this.sent++; sock.send(bytes, port, host); },
    broadcast(port, bytes) { sock.send(bytes, port, '127.0.0.1'); },
    onPacket(cb) { listeners.add(cb); return () => listeners.delete(cb); },
    close() { sock.close(); },
  };
}

// --- OSC ------------------------------------------------------------------------------------

test('OSC round trip of every type, with string padding edge cases', () => {
  for (const s of ['', 'abc', 'abcd', 'abcde', 'Βιολί']) {
    const bytes = Osc.encode('/x', [{ t: 's', v: s }, { t: 'i', v: -7 }, { t: 'f', v: 0.75 }, { t: 'b', v: new Uint8Array([1, 2, 3]) }]);
    assert.strictEqual(bytes.length % 4, 0);
    const [m] = Osc.decode(bytes);
    assert.strictEqual(m.address, '/x');
    assert.deepStrictEqual(m.args.map((a) => a.t), ['s', 'i', 'f', 'b']);
    assert.strictEqual(m.args[0].v, s);
    assert.strictEqual(m.args[1].v, -7);
    assert.strictEqual(m.args[2].v, 0.75);
    assert.deepStrictEqual([...m.args[3].v], [1, 2, 3]);
  }
});

test('OSC wire format matches the spec byte for byte', () => {
  const bytes = Osc.encode('/ch/01/mix/on', [{ t: 'i', v: 1 }]);
  const hex = Buffer.from(bytes).toString('hex');
  // "/ch/01/mix/on" + 3 NUL, ",i" + 2 NUL, int32 BE 1
  assert.strictEqual(hex, Buffer.from('/ch/01/mix/on\0\0\0,i\0\0').toString('hex') + '00000001');
  const q = Osc.encode('/xinfo');
  assert.strictEqual(Buffer.from(q).toString(), '/xinfo\0\0,\0\0\0');
});

test('OSC bundles are flattened; bad packets throw', () => {
  const a = Osc.encode('/a', [1.5]);
  const b = Osc.encode('/b', ['x']);
  const head = Buffer.concat([Buffer.from('#bundle\0'), Buffer.alloc(8)]);
  const el = (m) => { const n = Buffer.alloc(4); n.writeInt32BE(m.length); return Buffer.concat([n, Buffer.from(m)]); };
  const msgs = Osc.decode(new Uint8Array(Buffer.concat([head, el(a), el(b)])));
  assert.deepStrictEqual(msgs.map((m) => m.address), ['/a', '/b']);
  assert.throws(() => Osc.decode(new Uint8Array(Buffer.from('/abc'))));
});

test('base64 helpers round trip', () => {
  const bytes = new Uint8Array(300).map((_, i) => (i * 37) & 255);
  assert.deepStrictEqual([...Osc.fromBase64(Osc.toBase64(bytes))], [...bytes]);
});

// --- levels -----------------------------------------------------------------------------------

test('fader law: the four segments and their inverse', () => {
  assert.strictEqual(Levels.faderToDb(1), 10);
  assert.strictEqual(Levels.faderToDb(0.75), 0);
  assert.strictEqual(Levels.faderToDb(0.5), -10);
  assert.strictEqual(Levels.faderToDb(0.25), -30);
  assert.strictEqual(Levels.faderToDb(0.0625), -60);
  assert.strictEqual(Levels.faderToDb(0), -Infinity);
  for (const db of [10, 0, -5, -10, -20, -30, -45, -60, -75]) {
    assert.ok(Math.abs(Levels.faderToDb(Levels.dbToFader(db)) - db) < 0.25, 'dB ' + db);
  }
  assert.strictEqual(Levels.dbToFader(-Infinity), 0);
  assert.strictEqual(Levels.formatDb(-Infinity), '-∞');
  assert.strictEqual(Levels.formatDb(0), '0.0');
  assert.strictEqual(Levels.formatDb(3.04), '+3.0');
});

test('pan law', () => {
  assert.strictEqual(Levels.formatPan(0.5), 'C');
  assert.strictEqual(Levels.formatPan(0), 'L100');
  assert.strictEqual(Levels.formatPan(1), 'R100');
  assert.strictEqual(Levels.percentToPan(-50), 0.25);
});

test('meter blobs: both byte orders, and garbage is refused', () => {
  const vals = [-90, -60, -12.5, 0, 6];
  for (const little of [true, false]) {
    const got = Levels.parseMeterBlob(Levels.buildMeterBlob(vals, little));
    assert.deepStrictEqual([...got], vals);
  }
  assert.strictEqual(Levels.parseMeterBlob(new Uint8Array([255, 255, 255, 127])), null);
  assert.strictEqual(Levels.parseMeterBlob(null), null);
});

// --- profile ----------------------------------------------------------------------------------

test('X AIR layout per model', () => {
  const xr18 = profile.layout('XR18');
  assert.strictEqual(xr18.inputs.length, 17); // 16 channels + aux
  assert.strictEqual(xr18.returns.length, 4);
  assert.deepStrictEqual(xr18.targets.map((t) => t.id), ['bus1', 'bus2', 'bus3', 'bus4', 'bus5', 'bus6', 'fx1', 'fx2', 'fx3', 'fx4']);
  assert.strictEqual(xr18.inputs[0].fader, '/ch/01/mix/fader');
  assert.strictEqual(xr18.inputs[0].sends.bus1, '/ch/01/mix/01/level');
  assert.strictEqual(xr18.inputs[0].sends.fx1, '/ch/01/mix/07/level');
  assert.strictEqual(xr18.inputs[16].fader, '/rtn/aux/mix/fader');
  assert.strictEqual(xr18.returns[0].sends.bus2, '/rtn/1/mix/02/level');
  assert.strictEqual(xr18.targets[0].master.fader, '/bus/1/mix/fader');
  assert.strictEqual(xr18.targets[6].master.fader, '/fxsend/1/mix/fader');
  assert.strictEqual(xr18.main.fader, '/lr/mix/fader');
  assert.strictEqual(xr18.dcas[0].fader, '/dca/1/fader');
  assert.strictEqual(xr18.dcas[0].on, '/dca/1/on');
  assert.strictEqual(xr18.muteGroups[3].path, '/config/mute/4');
  assert.strictEqual(new Set(xr18.params.map((p) => p.path)).size, xr18.params.length, 'no duplicate parameters');
  assert.strictEqual(profile.layout('XR12').inputs.length, 13);
  assert.strictEqual(profile.layout('XR12').targets.length, 6);
  assert.strictEqual(profile.layout('XR16').targets.length, 8);
  assert.strictEqual(profile.layout('x-18').model, 'X18');
  assert.strictEqual(profile.layout('something new').model, 'XR18');
  assert.deepStrictEqual(profile.parseInfo([{ v: '192.168.1.1' }, { v: 'Stage' }, { v: 'XR16' }, { v: '1.22' }]),
    { ip: '192.168.1.1', name: 'Stage', model: 'XR16', modelRaw: 'XR16', version: '1.22' });
});

// --- live session against the simulator -------------------------------------------------------

test('discovery finds the console, connect loads every parameter', async () => {
  const console_ = await sim.start({ model: 'XR16' });
  const tr = nodeTransport();
  await tr.ready;
  const prof = Object.assign({}, profile, { port: console_.port });
  try {
    const found = await Mixer.discover(prof, tr, 300);
    assert.strictEqual(found.length, 1);
    assert.strictEqual(found[0].model, 'XR16');
    assert.strictEqual(found[0].name, 'XR16-SIM');
    const s = new Mixer.Session(prof, tr);
    const info = await s.connect('127.0.0.1', console_.port);
    assert.strictEqual(info.model, 'XR16');
    assert.strictEqual(s.status, 'online');
    assert.strictEqual(s.values.size, s.layout.params.length);
    assert.strictEqual(s.get('/ch/01/config/name'), 'Kick');
    assert.strictEqual(s.get('/ch/01/mix/on'), 1);
    s.disconnect();
  } finally { tr.close(); console_.close(); }
});

test('changes go both ways; drags are coalesced and end on the last value', async () => {
  const console_ = await sim.start();
  const tr = nodeTransport();
  await tr.ready;
  const prof = Object.assign({}, profile, { port: console_.port });
  try {
    const s = new Mixer.Session(prof, tr);
    await s.connect('127.0.0.1', console_.port);
    // app -> console
    s.set('/ch/02/mix/on', 0);
    s.set('/ch/02/mix/03/level', Levels.dbToFader(-6));
    await until(() => console_.state.get('/ch/02/mix/on') === 0, 1000, 'mute to reach the console');
    await until(() => Math.abs(console_.state.get('/ch/02/mix/03/level') - Levels.dbToFader(-6)) < 1e-6, 1000, 'send level');
    // a fast drag: 100 values in about 200 ms
    const before = console_.received.filter((m) => m.address === '/ch/05/mix/fader').length;
    for (let i = 0; i <= 100; i++) { s.set('/ch/05/mix/fader', i / 100); if (i % 10 === 0) await sleep(20); }
    await sleep(100);
    const msgs = console_.received.filter((m) => m.address === '/ch/05/mix/fader').length - before;
    assert.ok(msgs < 40, 'drag sent ' + msgs + ' messages');
    assert.strictEqual(console_.state.get('/ch/05/mix/fader'), 1);
    // console -> app (another remote, or the console itself)
    let heard = null;
    s.on('/lr/mix/fader', (v, local) => { heard = { v, local }; });
    console_.change('/lr/mix/fader', 0.5);
    await until(() => heard, 1000, 'console change');
    assert.deepStrictEqual(heard, { v: 0.5, local: false });
    // clear solo
    console_.change('/-stat/solosw/03', 1);
    await until(() => s.get('/-stat/solosw/03') === 1, 1000, 'solo on');
    s.action(...s.layout.clearSolo);
    await until(() => s.get('/-stat/solosw/03') === 0, 1000, 'solo cleared');
    s.disconnect();
  } finally { tr.close(); console_.close(); }
});

test('meters stream and keep streaming past the subscription time', async () => {
  const console_ = await sim.start({ subscribeMs: 400 });
  const tr = nodeTransport();
  await tr.ready;
  const prof = Object.assign({}, profile, { port: console_.port, keepAliveMs: 250 });
  try {
    const s = new Mixer.Session(prof, tr);
    await s.connect('127.0.0.1', console_.port);
    let last = null;
    let count = 0;
    s.onMeters((addr, db) => { last = db; count++; });
    await until(() => last, 1000, 'meters');
    assert.strictEqual(last.length, 40);
    count = 0;
    await sleep(1200); // three subscription periods
    assert.ok(count > 15, 'only ' + count + ' meter packets in 1.2 s');
    // remote changes still arrive after the first /xremote period ran out
    let heard = false;
    s.on('/ch/09/mix/pan', () => { heard = true; });
    console_.change('/ch/09/mix/pan', 0.2);
    await until(() => heard, 1000, 'change after renewal');
    s.disconnect();
  } finally { tr.close(); console_.close(); }
});

test('a console that does not answer fails cleanly', async () => {
  const tr = nodeTransport();
  await tr.ready;
  try {
    const s = new Mixer.Session(profile, tr);
    await assert.rejects(s.connect('127.0.0.1', 9, 300), /No answer/);
    assert.strictEqual(s.status, 'failed');
    s.disconnect();
  } finally { tr.close(); }
});

(async () => {
  for (const [name, fn] of tests) {
    try { await fn(); console.log('ok   ' + name); }
    catch (e) { failures++; console.log('FAIL ' + name + '\n     ' + (e.stack || e).toString().split('\n').slice(0, 3).join('\n     ')); }
  }
  console.log(failures ? failures + ' failed' : 'all ' + tests.length + ' passed');
  process.exit(failures ? 1 : 0);
})();
