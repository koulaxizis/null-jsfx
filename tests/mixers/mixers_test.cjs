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
const x32 = require(path.join(web, 'x32/profile.js'));
const sim = require('./xair_sim.cjs');
const wing = require(path.join(web, 'wing/profile.js'));
const wingSim = require('./wing_sim.cjs');
const net = require('net');
const sq = require(path.join(web, 'sq/profile.js'));
const Nrpn = require(path.join(web, 'core/nrpn.js'));
const sqSim = require('./sq_sim.cjs');
const qu = require(path.join(web, 'qu/profile.js'));
const cq = require(path.join(web, 'cq/profile.js'));
const dlive = require(path.join(web, 'dlive/profile.js'));
const dliveSim = require('./dlive_sim.cjs');
const quSim = require('./qu_sim.cjs');

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

// --- X32 / M32 ----------------------------------------------------------------------------------

test('X32 layout: paths, solo switches, meters, the same tree on every model', () => {
  const L = x32.layout('X32');
  assert.strictEqual(L.inputs.length, 40); // 32 channels + 8 aux ins
  assert.strictEqual(L.returns.length, 8);
  assert.strictEqual(L.targets.length, 16);
  assert.strictEqual(L.dcas.length, 8);
  assert.strictEqual(L.muteGroups.length, 6);
  assert.strictEqual(L.inputs[0].fader, '/ch/01/mix/fader');
  assert.strictEqual(L.inputs[31].name, '/ch/32/config/name');
  assert.strictEqual(L.inputs[32].fader, '/auxin/01/mix/fader');
  assert.strictEqual(L.returns[7].on, '/fxrtn/08/mix/on');
  assert.strictEqual(L.inputs[0].sends.bus16, '/ch/01/mix/16/level');
  assert.strictEqual(L.returns[0].sends.bus1, '/fxrtn/01/mix/01/level');
  assert.strictEqual(L.targets[9].master.fader, '/bus/10/mix/fader');
  assert.strictEqual(L.main.fader, '/main/st/mix/fader');
  assert.strictEqual(L.main.pan, '/main/st/mix/pan');
  assert.strictEqual(L.dcas[7].fader, '/dca/8/fader');
  assert.strictEqual(L.dcas[0].name, '/dca/1/config/name');
  assert.strictEqual(L.muteGroups[5].path, '/config/mute/6');
  // solo switches (Companion's map): ch 01..32, aux 33..40, FX returns 41..48, buses 49..64, main 71, DCA 73..80
  assert.deepStrictEqual([L.inputs[0], L.inputs[31], L.inputs[32], L.returns[0], L.targets[0].master, L.targets[15].master, L.main, L.dcas[0], L.dcas[7]].map((x) => x.solo),
    ['/-stat/solosw/01', '/-stat/solosw/32', '/-stat/solosw/33', '/-stat/solosw/41', '/-stat/solosw/49', '/-stat/solosw/64', '/-stat/solosw/71', '/-stat/solosw/73', '/-stat/solosw/80']);
  // meters: /meters/0 then /meters/2 in one index space
  assert.deepStrictEqual([L.inputs[0], L.inputs[32], L.returns[0], L.targets[0].master, L.main].map((x) => x.meter), [0, 32, 40, 48, 92]);
  assert.deepStrictEqual(L.meters.map((m) => [m.address, m.offset, m.count]), [['/meters/0', 0, 70], ['/meters/2', 70, 49]]);
  assert.strictEqual(new Set(L.params.map((p) => p.path)).size, L.params.length, 'no duplicate parameters');
  for (const m of ['X32', 'x32 rack', 'X32C', 'M32', 'M32R', 'M32C']) assert.strictEqual(x32.layout(m).params.length, L.params.length, m);
  assert.strictEqual(x32.model('M32R'), 'M32R');
  assert.strictEqual(x32.model('X32 Compact'), 'X32');
});

test('/xinfo answers go to the right family; X32 meter blobs (float32) round trip', () => {
  const info = (m) => [{ v: '10.0.0.5' }, { v: 'FOH' }, { v: m }, { v: '4.06' }];
  assert.strictEqual(x32.parseInfo(info('M32')).model, 'M32');
  assert.strictEqual(x32.parseInfo(info('X32RACK')).model, 'X32RACK');
  assert.strictEqual(x32.parseInfo(info('XR18')), null);
  assert.strictEqual(profile.parseInfo(info('X32')), null);
  assert.strictEqual(profile.parseInfo(info('M32C')), null);
  assert.strictEqual(profile.parseInfo(info('XR18')).model, 'XR18');
  const db = [0, -6, -20, -90, 6];
  const back = x32.parseMeter(x32.buildMeter(db));
  assert.strictEqual(back.length, 5);
  db.forEach((d, i) => assert.ok(Math.abs(back[i] - d) < 1e-3, 'meter ' + i + ': ' + back[i]));
  assert.strictEqual(x32.parseMeter(new Uint8Array([9, 0, 0, 0, 1, 2])), null, 'a count past the end is refused');
  assert.strictEqual(x32.keepAlive().map((k) => k[0] + ' ' + (k[1][0] ? k[1][0].v : '')).join(', '), '/xremote , /meters /meters/0, /meters /meters/2');
});

test('X32: identify a typed address, connect, both directions, both meter blocks', async () => {
  const xr = await sim.start({ model: 'XR18' });
  const mx = await sim.start({ profile: 'x32', model: 'M32' });
  const tr = nodeTransport();
  await tr.ready;
  // each simulator on its own port; both answer on 127.0.0.1
  const pX = Object.assign({}, x32, { port: mx.port, keepAliveMs: 250 });
  const pA = Object.assign({}, profile, { port: xr.port });
  try {
    const found = await Mixer.discover(pX, tr, 300);
    assert.deepStrictEqual(found.map((c) => c.model), ['M32']);
    assert.deepStrictEqual((await Mixer.discover(pA, tr, 300)).map((c) => c.model), ['XR18']);
    const who = await Mixer.identify([pA, pX], tr, '127.0.0.1', 600);
    assert.ok(who.profile === pA || who.profile === pX);
    const s = new Mixer.Session(pX, tr);
    const inf = await s.connect('127.0.0.1', mx.port);
    assert.strictEqual(inf.model, 'M32');
    assert.strictEqual(s.values.size, s.layout.params.length);
    assert.strictEqual(s.get('/ch/01/config/name'), 'Kick');
    assert.strictEqual(s.get('/bus/01/config/name'), 'Drummer');
    s.set('/ch/32/mix/fader', Levels.dbToFader(-10));
    s.set('/auxin/03/mix/12/level', Levels.dbToFader(-3));
    s.set('/dca/8/on', 0);
    s.set('/config/mute/6', 1);
    await until(() => mx.state.get('/dca/8/on') === 0 && mx.state.get('/config/mute/6') === 1
      && Math.abs(mx.state.get('/ch/32/mix/fader') - Levels.dbToFader(-10)) < 1e-6
      && Math.abs(mx.state.get('/auxin/03/mix/12/level') - Levels.dbToFader(-3)) < 1e-6, 1000, 'X32 sets');
    let heard = null;
    s.on('/main/st/mix/fader', (v) => { heard = v; });
    mx.change('/main/st/mix/fader', 0.5);
    await until(() => heard === 0.5, 1000, 'Main LR from the console');
    mx.change('/-stat/solosw/41', 1);
    await until(() => s.get('/-stat/solosw/41') === 1, 1000, 'solo FX return 1');
    s.action(...s.layout.clearSolo);
    await until(() => s.get('/-stat/solosw/41') === 0, 1000, 'solo cleared');
    const blocks = new Map();
    s.onMeters((addr, db) => blocks.set(addr, db));
    await until(() => blocks.has('/meters/0') && blocks.has('/meters/2'), 1500, 'both meter blocks');
    assert.strictEqual(blocks.get('/meters/0').length, 70);
    assert.strictEqual(blocks.get('/meters/2').length, 49);
    // channel 1 sits at 0 dB on the fader: its meter reads about -12 dB +-6 in the simulator
    assert.ok(blocks.get('/meters/0')[0] > -20 && blocks.get('/meters/0')[0] < -5, 'ch 1 meter ' + blocks.get('/meters/0')[0]);
    s.disconnect();
  } finally { tr.close(); xr.close(); mx.close(); }
});

// --- WING ---------------------------------------------------------------------------------------

test('WING layout and info: paths not zero padded, every model the same tree, other families refused', () => {
  const L = wing.layout('ngc-full');
  assert.strictEqual(L.inputs.length, 48);
  assert.strictEqual(L.returns.length, 0);
  assert.strictEqual(L.targets.length, 16);
  assert.strictEqual(L.dcas.length, 16);
  assert.strictEqual(L.muteGroups.length, 8);
  assert.strictEqual(L.inputs[0].fader, '/ch/1/fdr');
  assert.strictEqual(L.inputs[40].on, '/aux/1/mute');
  assert.strictEqual(L.inputs[39].sends.bus16, '/ch/40/send/16/lvl');
  assert.strictEqual(L.main.fader, '/main/1/fdr');
  assert.strictEqual(L.dcas[15].solo, '/dca/16/$solo');
  assert.strictEqual(L.muteGroups[7].path, '/mgrp/8/mute');
  assert.strictEqual(L.clearSolo, null);
  assert.deepStrictEqual(L.meters, []);
  assert.strictEqual(new Set(L.params.map((p) => p.path)).size, L.params.length, 'no parameter twice');
  for (const m of ['ngc-compact', 'ngc-rack']) assert.strictEqual(wing.layout(m).params.length, L.params.length, m);
  const info = (s) => [{ t: 's', v: s }];
  assert.deepStrictEqual(wing.parseInfo(info('WING,192.168.1.62,FOH,ngc-compact,S123,3.0.5')),
    { ip: '192.168.1.62', name: 'FOH', model: 'WING Compact', modelRaw: 'ngc-compact', version: '3.0.5' });
  assert.strictEqual(wing.parseInfo(info('XR18')), null);
  assert.strictEqual(wing.parseInfo([{ t: 's', v: '192.168.1.1' }, { t: 's', v: 'X' }, { t: 's', v: 'X32' }]), null);
  assert.strictEqual(profile.parseInfo(info('WING,1.2.3.4,FOH,ngc-full,S,3')), null);
  assert.strictEqual(x32.parseInfo(info('WING,1.2.3.4,FOH,ngc-full,S,3')), null);
});

test('WING units: dB, pan -100..100 and an inverted mute, both ways', () => {
  const f = (v) => [{ t: 'f', v }];
  const sff = (v) => [{ t: 's', v: 'x' }, { t: 'f', v: 0.5 }, { t: 'f', v }];
  for (const db of [-60, -30, -10, -3, 0, 6, 10]) {
    const pos = Levels.dbToFader(db);
    assert.strictEqual(wing.toWire('/ch/1/fdr', pos, 'f')[0].v, db, 'to dB ' + db);
    assert.ok(Math.abs(wing.fromWire('/ch/1/fdr', sff(db)) - pos) < 1e-3, 'from dB ' + db);
    assert.ok(Math.abs(wing.fromWire('/ch/3/send/2/lvl', f(db)) - pos) < 1e-3, 'event dB ' + db);
  }
  assert.strictEqual(wing.toWire('/ch/1/fdr', 0, 'f')[0].v, -144);
  assert.strictEqual(wing.fromWire('/ch/1/fdr', f(-144)), 0);
  assert.strictEqual(wing.fromWire('/ch/1/fdr', f(20)), 1, 'clamped at +10 dB');
  assert.strictEqual(wing.toWire('/ch/1/pan', 0.5, 'f')[0].v, 0);
  assert.strictEqual(wing.toWire('/ch/1/pan', 0, 'f')[0].v, -100);
  assert.strictEqual(wing.fromWire('/ch/1/pan', sff(100)), 1);
  assert.deepStrictEqual(wing.toWire('/ch/1/mute', 1, 'i'), [{ t: 'i', v: 0 }], 'on = not muted');
  assert.strictEqual(wing.fromWire('/ch/1/mute', [{ t: 's', v: 'ON' }, { t: 'f', v: 1 }, { t: 'i', v: 1 }]), 0, 'muted = off');
  assert.deepStrictEqual(wing.toWire('/mgrp/2/mute', 1, 'i'), [{ t: 'i', v: 1 }], 'a mute group is not inverted');
  assert.strictEqual(wing.fromWire('/ch/1/name', [{ t: 's', v: 'Kick' }]), 'Kick');
  assert.strictEqual(wing.fromWire('/ch/1/col', [{ t: 's', v: '9' }, { t: 'f', v: 0.7 }, { t: 'i', v: 9 }]), 9);
  assert.strictEqual(wing.fromWire('/ch/1/name', []), undefined);
});

test('WING: discover and identify next to an X AIR, connect, both directions, one subscriber', async () => {
  const wg = await wingSim.start({ model: 'ngc-rack' });
  const xr = await sim.start({ model: 'XR18' });
  const tr = nodeTransport();
  const tr2 = nodeTransport();
  await Promise.all([tr.ready, tr2.ready]);
  const pW = Object.assign({}, wing, { port: wg.port, keepAliveMs: 250 });
  const pA = Object.assign({}, profile, { port: xr.port });
  try {
    assert.deepStrictEqual((await Mixer.discover(pW, tr, 300)).map((c) => c.model), ['WING Rack']);
    // an X AIR on the same host answers /xinfo; asked "/?" on its port it stays silent
    assert.deepStrictEqual(await Mixer.discover(Object.assign({}, wing, { port: xr.port }), tr, 300), []);
    const who = await Mixer.identify([pW], tr, '127.0.0.1', 600);
    assert.strictEqual(who.profile, pW);
    assert.strictEqual(who.info.name, 'WING-SIM');
    const s = new Mixer.Session(pW, tr);
    const inf = await s.connect('127.0.0.1', wg.port);
    assert.strictEqual(inf.model, 'WING Rack');
    assert.strictEqual(s.values.size, s.layout.params.length);
    assert.strictEqual(s.get('/ch/1/name'), 'Kick');
    assert.strictEqual(s.get('/ch/1/col'), 9);
    assert.strictEqual(s.get('/bus/1/name'), 'Drummer');
    assert.ok(Math.abs(s.get('/ch/1/fdr') - Levels.dbToFader(0)) < 1e-3, '0 dB at unity');
    assert.strictEqual(s.get('/ch/1/mute'), 1, 'unmuted shows as on');
    assert.strictEqual(s.get('/ch/1/pan'), 0.5);
    assert.strictEqual(s.get('/ch/1/send/1/lvl'), 0, 'send off');
    s.set('/ch/40/fdr', Levels.dbToFader(-10));
    s.set('/aux/3/send/12/lvl', Levels.dbToFader(-3));
    s.set('/ch/5/mute', 0);
    s.set('/ch/6/pan', 0.25);
    s.set('/mgrp/8/mute', 1);
    await until(() => wg.state.get('/ch/5/mute') === 1 && wg.state.get('/mgrp/8/mute') === 1 && wg.state.get('/ch/6/pan') === -50
      && Math.abs(wg.state.get('/ch/40/fdr') + 10) < 1e-3 && Math.abs(wg.state.get('/aux/3/send/12/lvl') + 3) < 1e-3, 1000, 'WING sets in dB');
    assert.strictEqual(s.get('/ch/5/mute'), 0, 'the set holds without an echo');
    assert.ok(wg.received.some((m) => m.address === '/*S'), 'subscribed');
    let heard = null;
    s.on('/main/1/fdr', (v) => { heard = v; });
    wg.change('/main/1/fdr', -10);
    await until(() => heard !== null && Math.abs(heard - Levels.dbToFader(-10)) < 1e-3, 1000, 'Main from the console');
    wg.change('/dca/4/$solo', 1);
    await until(() => s.get('/dca/4/$solo') === 1, 1000, 'DCA solo from the console');
    // a second remote subscribes: the WING follows it only; ours comes back at its next renewal
    tr2.send('127.0.0.1', wg.port, Osc.encode('/*S'));
    await until(() => wg.subscriber() && wg.subscriber() !== '127.0.0.1:' + tr2.port, 500, 'second subscriber').catch(() => {});
    await sleep(400);
    wg.change('/ch/2/name', 'Lead');
    await until(() => s.get('/ch/2/name') === 'Lead', 1000, 'subscription renewed');
    s.disconnect();
  } finally { tr.close(); tr2.close(); wg.close(); xr.close(); }
});

// --- Allen & Heath SQ (MIDI NRPN over TCP) ---------------------------------------------------

// a TCP dial with the shape of the app's bridge (core/transport.js shellTcp)
const nodeTcp = (host, port) => (cb) => {
  const sock = net.connect(port, host);
  sock.on('connect', () => cb.onOpen());
  sock.on('data', (d) => cb.onData(new Uint8Array(d)));
  sock.on('error', () => {});
  sock.on('close', () => cb.onClose(''));
  return { send: (bytes) => sock.write(Buffer.from(bytes)), close: () => sock.destroy() };
};

test("SQ addresses match the protocol document's examples", () => {
  const hex = (path) => { const a = sq.address(path); return ((a >> 7).toString(16) + ' ' + (a & 127).toString(16)).toUpperCase(); };
  assert.strictEqual(hex('/ip/40/aux/5'), '44 1C', 'Ip40 > Aux5');
  assert.strictEqual(hex('/fx/1/aux/1'), '46 14', 'FX1 Rtn > Aux1');
  assert.strictEqual(hex('/ip/24/pan'), '50 17', 'Ip24 pan to LR');
  assert.strictEqual(hex('/ip/1/lr'), '40 0', 'Ip1 > LR');
  assert.strictEqual(hex('/fx/8/lr'), '40 43', 'FX8 Rtn > LR');
  assert.strictEqual(hex('/ip/1/mute'), '0 0');
  assert.strictEqual(hex('/fx/1/mute'), '0 3C');
  assert.strictEqual(hex('/lr/mute'), '0 44');
  assert.strictEqual(hex('/aux/12/mute'), '0 50');
  assert.strictEqual(hex('/lr/lvl'), '4F 0');
  assert.strictEqual(hex('/aux/12/lvl'), '4F C');
  assert.strictEqual(hex('/mgrp/8'), '4 7');
  // the pan to an aux (the document: Ip24 > Aux5 = 52 5C, Ip37 > Aux8 = 53 7B) is not on the
  // tab (pans follow LR), but the formula the profile uses for sends gives the same base
  const panAux = (ip, a) => 0x50 * 128 + 0x44 + (ip - 1) * 12 + (a - 1);
  assert.strictEqual(panAux(24, 5), 0x52 * 128 + 0x5c);
  assert.strictEqual(panAux(37, 8), 0x53 * 128 + 0x7b);
  const L = sq.layout();
  assert.strictEqual(L.inputs.length, 48);
  assert.strictEqual(L.returns.length, 8);
  assert.strictEqual(L.targets.length, 12);
  assert.strictEqual(L.muteGroups.length, 8);
  assert.deepStrictEqual(L.dcas, []);
  const addrs = L.params.map((p) => sq.address(p.path));
  assert.strictEqual(new Set(addrs).size, addrs.length, 'no two parameters share an address');
  assert.strictEqual(L.params.length, (48 + 8) * (3 + 12) + 12 * 2 + 2 + 8);
});

test('SQ values: the linear taper at 0 dB and +10 dB, pan centre, inverted mute', () => {
  assert.strictEqual(sq.toValue('/ip/1/lr', 0), 0);
  assert.strictEqual(sq.toValue('/ip/1/lr', 0.75), 0x76 * 128 + 0x5c, '0 dB = 76 5C');
  assert.strictEqual(sq.toValue('/ip/1/lr', 1), 16383, '+10 dB = 7F 7F');
  for (const f of [0, 0.1, 0.5, 0.75, 0.9, 1]) assert.ok(Math.abs(sq.fromValue('/lr/lvl', sq.toValue('/lr/lvl', f)) - f) < 1e-4, 'round trip ' + f);
  assert.strictEqual(sq.toValue('/ip/1/pan', 0.5), 0x3f * 128 + 0x7f);
  assert.strictEqual(sq.toValue('/ip/1/pan', 0), 0);
  assert.strictEqual(sq.toValue('/ip/1/pan', 1), 16383);
  for (const x of [0, 0.2, 0.5, 0.8, 1]) assert.ok(Math.abs(sq.fromValue('/ip/1/pan', sq.toValue('/ip/1/pan', x)) - x) < 1e-4, 'pan ' + x);
  assert.strictEqual(sq.fromValue('/ip/1/pan', 0x3f * 128 + 0x7f), 0.5, '3F 7F is the centre');
  assert.strictEqual(sq.toValue('/ip/1/mute', 1), 0, 'on = not muted');
  assert.strictEqual(sq.fromValue('/ip/1/mute', 1), 0, 'muted = off');
  assert.strictEqual(sq.toValue('/mgrp/1', 1), 1, 'a mute group is not inverted');
});

test('NRPN messages byte for byte; the parser takes running status and other channels apart', () => {
  // Ip1 > LR at 0 dB on MIDI channel 1
  assert.deepStrictEqual(Nrpn.setMessage(0, 0x40 * 128, ZERO), [0xb0, 0x63, 0x40, 0xb0, 0x62, 0, 0xb0, 6, 0x76, 0xb0, 0x26, 0x5c]);
  assert.deepStrictEqual(Nrpn.getMessage(0, 0x4f * 128), [0xb0, 0x63, 0x4f, 0xb0, 0x62, 0, 0xb0, 0x60, 0x7f]);
  const got = [];
  const p = new Nrpn.Parser((ch, a, v) => got.push([ch, a, v]));
  p.push([0xb0, 0x63, 1, 0x62, 2, 0x06, 3, 0x26, 4]); // running status
  p.push([0xb5, 0x63, 0, 0xf8, 0x62, 9]);              // real time in the middle
  p.push([0x06, 0x7f, 0x26, 0x7f]);
  p.push([0xb0, 0x06, 1, 0x26, 1]);                    // a new value on the last address
  assert.deepStrictEqual(got, [[0, 130, 388], [5, 9, 16383], [0, 130, 129]]);
});
const ZERO = 0x76 * 128 + 0x5c;

test('SQ: connect over TCP, read everything, both directions, a second remote, reconnect', async () => {
  const sim = await sqSim.start();
  const s = new Nrpn.Session(sq, nodeTcp);
  const other = new Nrpn.Session(sq, nodeTcp);
  try {
    const info = await s.connect('127.0.0.1', sim.port);
    assert.strictEqual(info.model, 'SQ');
    assert.strictEqual(s.status, 'online');
    assert.strictEqual(s.values.size, s.layout.params.length, 'every parameter answered');
    assert.strictEqual(sim.gets, s.layout.params.length);
    assert.strictEqual(s.get('/ip/1/lr'), 0.75, '0 dB');
    assert.strictEqual(s.get('/ip/1/aux/1'), 0, 'send off');
    assert.strictEqual(s.get('/ip/1/pan'), 0.5);
    assert.strictEqual(s.get('/ip/1/mute'), 1, 'unmuted shows as on');
    await other.connect('127.0.0.1', sim.port);
    // a drag: many sets, at most one message per 20 ms, ending on the last value
    const before = sim.received.length;
    for (let i = 0; i <= 50; i++) s.set('/ip/48/lr', i / 50);
    s.set('/ip/5/mute', 0);
    s.set('/fx/2/aux/12', 0.75);
    s.set('/mgrp/3', 1);
    s.set('/ip/7/pan', 0);
    await until(() => sim.state.get('/ip/48/lr') === 16383 && sim.state.get('/ip/5/mute') === 1 && sim.state.get('/fx/2/aux/12') === ZERO
      && sim.state.get('/mgrp/3') === 1 && sim.state.get('/ip/7/pan') === 0, 1000, 'sets reach the SQ');
    assert.ok(sim.received.length - before < 20, 'the drag was coalesced: ' + (sim.received.length - before));
    await until(() => other.get('/ip/48/lr') === 1 && other.get('/ip/5/mute') === 0, 1000, 'the other remote hears it');
    assert.strictEqual(s.get('/ip/48/lr'), 1, 'no echo back to the sender');
    // a change on the console
    let heard = null;
    s.on('/aux/3/lvl', (v) => { heard = v; });
    sim.change('/aux/3/lvl', 0);
    await until(() => heard === 0, 1000, "the console's change");
    // the console goes away and comes back: the session redials and reads everything again
    const states = [];
    s.onStatus((x) => states.push(x));
    sim.change('/lr/mute', 1);
    await until(() => s.get('/lr/mute') === 0, 1000, 'LR muted');
    sim.drop();
    await until(() => states.includes('lost'), 1000, 'lost');
    await until(() => s.status === 'online', 4000, 'back online');
    s.disconnect();
    other.disconnect();
    await sleep(50);
    assert.strictEqual(sim.clients(), 0, 'connections closed');
  } finally { s.disconnect(); other.disconnect(); sim.close(); }
});

test('SQ: nobody on the port fails cleanly', async () => {
  const srv = net.createServer();
  await new Promise((r) => srv.listen(0, '127.0.0.1', r));
  const port = srv.address().port;
  await new Promise((r) => srv.close(r));
  const s = new Nrpn.Session(sq, nodeTcp);
  await assert.rejects(s.connect('127.0.0.1', port, 1000), /no connection/);
  assert.strictEqual(s.status, 'failed');
});

// --- Allen & Heath Qu (MIDI over TCP, its own hooks on core/nrpn.js) -----------------------

const hx = (a) => a.map((x) => x.toString(16).toUpperCase().padStart(2, '0')).join(' ');

test('Qu messages byte for byte, as the protocol document writes them', () => {
  // Input 1 fader at 0 dB: BN 63 CH, BN 62 17, BN 06 VA, BN 26 07 (channel N = 0)
  assert.strictEqual(hx(qu.encode(0, '/ip/1/lr', qu.toValue('/ip/1/lr', 0.75))), 'B0 63 20 B0 62 17 B0 06 62 B0 26 07');
  // ST 2 pan full right, to LR: ID 16, VA 4A, VX 07
  assert.strictEqual(hx(qu.encode(0, '/st/2/pan', qu.toValue('/st/2/pan', 1))), 'B0 63 41 B0 62 16 B0 06 4A B0 26 07');
  // Input 32 send to Mix 9-10 at +10 dB on channel 3: ID 20, VX 06
  assert.strictEqual(hx(qu.encode(2, '/ip/32/mix/7', qu.toValue('/ip/32/mix/7', 1))), 'B2 63 3F B2 62 20 B2 06 7F B2 26 06');
  // mutes: note on 9N CH 7F (on) / 3F (off), then the note with velocity 00
  assert.strictEqual(hx(qu.encode(0, '/fx/1/mute', qu.toValue('/fx/1/mute', 0))), '90 08 7F 90 08 00', 'FX Rtn 1 muted');
  assert.strictEqual(hx(qu.encode(0, '/lr/mute', qu.toValue('/lr/mute', 1))), '90 67 3F 90 67 00', 'LR unmuted');
  assert.strictEqual(hx(qu.encode(0, '/mgrp/4', 1)), '90 53 7F 90 53 00', 'mute group 4 on');
  assert.strictEqual(hx(qu.encode(0, '/mix/5/lvl', 0)), 'B0 63 64 B0 62 17 B0 06 00 B0 26 07', 'Mix 5-6 master at -inf');
  // Get System State and its answer; a name request
  assert.strictEqual(hx(qu.hello()), 'F0 00 00 1A 50 11 01 00 7F 10 01 F7');
  assert.deepStrictEqual(qu.helloReply([0xf0, 0, 0, 0x1a, 0x50, 0x11, 1, 0, 3, 0x11, 2, 1, 9, 0xf7]), { channel: 3, model: 'Qu-24' });
  assert.strictEqual(qu.helloReply([0xf0, 0, 0, 0x1a, 0x50, 0x11, 1, 0, 3, 0x14, 0xf7]), null);
  assert.ok(qu.syncEnd([0xf0, 0, 0, 0x1a, 0x50, 0x11, 1, 0, 3, 0x14, 0xf7]));
  assert.strictEqual(hx(qu.query(0, '/ip/3/name')), 'F0 00 00 1A 50 11 01 00 00 01 22 F7');
  assert.strictEqual(qu.query(0, '/ip/3/lr'), null, 'values come in the push, not asked');
  // levels: -inf, 0 dB and +10 dB exact; pan centre 25
  assert.strictEqual(qu.toValue('/lr/lvl', 0), 0);
  assert.strictEqual(qu.toValue('/lr/lvl', 1), 0x7f);
  assert.strictEqual(qu.fromValue('/lr/lvl', 0x62), 0.75);
  assert.strictEqual(qu.toValue('/ip/1/pan', 0.5), 0x25);
  assert.strictEqual(qu.fromValue('/ip/1/pan', 0x25), 0.5);
  for (const x of [0, 0.25, 0.5, 0.75, 1]) assert.ok(Math.abs(qu.fromValue('/ip/1/pan', qu.toValue('/ip/1/pan', x)) - x) < 0.02, 'pan ' + x);
  // layouts per model: mono inputs, then ST 1-3
  assert.strictEqual(qu.layoutFor('Qu-16').inputs.length, 19);
  assert.strictEqual(qu.layoutFor('Qu-24').inputs.length, 27);
  assert.strictEqual(qu.layoutFor('Qu-SB').inputs.length, 35);
  const L = qu.layoutFor('Qu-32');
  assert.deepStrictEqual(L.targets.map((t) => t.label), ['Mix 1', 'Mix 2', 'Mix 3', 'Mix 4', 'Mix 5-6', 'Mix 7-8', 'Mix 9-10']);
  const addrs = L.params.map((p) => qu.address(p.path));
  assert.strictEqual(new Set(addrs).size, addrs.length, 'every parameter has its own address');
});

test('Qu: connect over TCP, the hello and the push, names, both directions, keep-alive, one connection, reconnect', async () => {
  const sim = await quSim.start({ model: 'Qu-24', channel: 2, quietMs: 1500 });
  const s = new Nrpn.Session(qu, nodeTcp);
  const other = new Nrpn.Session(qu, nodeTcp);
  try {
    const info = await s.connect('127.0.0.1', sim.port);
    assert.strictEqual(info.model, 'Qu-24', 'the model from the box id');
    assert.strictEqual(s.channel, 2, 'the MIDI channel the Qu said');
    assert.strictEqual(sim.hellos, 1);
    assert.strictEqual(s.layout.inputs.length, 27, 'a Qu-24: 24 inputs and ST 1-3');
    assert.strictEqual(s.values.size, s.layout.params.length, 'every value pushed and every name asked');
    assert.strictEqual(s.get('/ip/1/lr'), 0.75, '0 dB');
    assert.strictEqual(s.get('/ip/1/pan'), 0.5);
    assert.strictEqual(s.get('/ip/1/mute'), 1, 'unmuted shows as on');
    assert.strictEqual(s.get('/ip/1/name'), 'Kick');
    assert.strictEqual(s.get('/mix/1/name'), 'Wedge 1');
    assert.strictEqual(s.get('/ip/3/name'), '');
    // the Qu takes one TCP MIDI connection: a second remote gets nothing
    await assert.rejects(other.connect('127.0.0.1', sim.port, 800));
    assert.strictEqual(sim.refused, 1);
    // a drag is coalesced, mutes go as notes
    const before = sim.received.length;
    for (let i = 0; i <= 50; i++) s.set('/ip/24/lr', i / 50);
    s.set('/st/1/mute', 0);
    s.set('/fx/4/mix/5', 0.75);
    s.set('/mgrp/2', 1);
    s.set('/ip/7/pan', 0);
    await until(() => sim.state.get('/ip/24/lr') === 0x7f && sim.state.get('/st/1/mute') === 1 && sim.state.get('/fx/4/mix/5') === 0x62
      && sim.state.get('/mgrp/2') === 1 && sim.state.get('/ip/7/pan') === 0, 1000, 'sets reach the Qu');
    assert.ok(sim.received.length - before < 20, 'the drag was coalesced: ' + (sim.received.length - before));
    // a change on the console's surface
    let heard = null;
    s.on('/mix/3/lvl', (v) => { heard = v; });
    sim.change('/mix/3/lvl', 0);
    await until(() => heard === 0, 1000, "the console's change");
    sim.change('/lr/mute', 1);
    await until(() => s.get('/lr/mute') === 0, 1000, 'LR muted on the console');
    // Active Sensing keeps a quiet connection open past the Qu's limit (1.5 s here)
    await sleep(2200);
    assert.ok(sim.connected() && s.status === 'online', 'still connected after a quiet spell');
    // the console goes away and comes back: the session redials, says hello and reads again
    const states = [];
    s.onStatus((x) => states.push(x));
    sim.drop();
    await until(() => states.includes('lost'), 1000, 'lost');
    await until(() => s.status === 'online', 5000, 'back online');
    assert.strictEqual(sim.hellos, 2);
    s.disconnect();
    await sleep(50);
    assert.ok(!sim.connected(), 'connection closed');
  } finally { s.disconnect(); other.disconnect(); sim.close(); }
});

test('Qu meters: the request, the 7-bit packing and 7Q8 dB as the document gives them, the strip map', () => {
  assert.strictEqual(hx(qu.meterRequest(0, true)), 'F0 00 00 1A 50 11 01 00 00 12 01 F7');
  assert.strictEqual(hx(qu.meterRequest(3, false)), 'F0 00 00 1A 50 11 01 00 03 12 00 F7');
  // the document's worked example: 20 7C 00 unpacks to 7C 80, which is -3.5 dB
  assert.deepStrictEqual(qu.unpack7([0x20, 0x7c, 0x00]), [0x7c, 0x80]);
  assert.strictEqual(qu.meterDb(0x7c80), -3.5);
  const bytes = Array.from({ length: 23 }, (_, i) => (i * 37 + 128) & 255);
  assert.deepStrictEqual(qu.unpack7(qu.pack7(bytes)), bytes, 'pack and unpack, a partial last group too');
  assert.ok(qu.pack7(bytes).every((b) => b < 128), 'MIDI data bytes only');
  // where a strip reads its meter: input 1 Post Delay, input 25 in its own block on a Qu-32, ST 1
  // Post Delay L/R, Mix 1 Post Fader, LR Post Fader L/R
  const P = qu.meterPoints('Qu-32');
  assert.deepStrictEqual(P[0], [3]);
  assert.deepStrictEqual(P[24], [323]);
  assert.deepStrictEqual(P[32], [246, 247]);
  assert.deepStrictEqual(P[35], [405]);
  assert.deepStrictEqual(P[42], [510, 511]);
  assert.deepStrictEqual(qu.meterPoints('Qu-24')[27], [485], 'Qu-24: Mix 1 after 180 unused meters');
  assert.deepStrictEqual(qu.meterPoints('Qu-16')[16], [246, 247], 'Qu-16: ST 1 after 80 unused meters');
  for (const m of qu.models) {
    const L = qu.layoutFor(m);
    const strips = L.inputs.concat(L.targets.map((t) => t.master), [L.main]);
    assert.deepStrictEqual(strips.map((st) => st.meter), strips.map((_, i) => i), m + ': one meter per strip, in order');
    assert.strictEqual(qu.meterPoints(m).length, strips.length, m);
  }
  // a whole answer: the loudest side of a stereo strip, -90 where the data ends
  const data = [];
  for (let i = 0; i < 520; i++) { const u = qu.dbMeter(i === 3 ? -12 : i === 247 ? -6 : i === 510 ? 4.5 : -60); data.push(u >> 8, u & 255); }
  const m = qu.parseMeters([0xf0, 0, 0, 0x1a, 0x50, 0x11, 1, 0, 0, 0x13].concat(qu.pack7(data), [0xf7]), 'Qu-32');
  assert.strictEqual(m.address, 'qu');
  assert.strictEqual(m.db[0], -12);
  assert.strictEqual(m.db[32], -6);
  assert.strictEqual(m.db[42], 4.5);
  assert.strictEqual(m.db[1], -60);
  assert.strictEqual(qu.parseMeters([0xf0, 0, 0, 0x1a, 0x50, 0x11, 1, 0, 0, 0x13, 0xf7], 'Qu-32').db[0], -90);
  assert.strictEqual(qu.parseMeters([0xf0, 0, 0, 0x1a, 0x50, 0x11, 1, 0, 0, 0x14, 0xf7], 'Qu-32'), null);
});

test('Qu meters over TCP: on while someone listens, asked again after each answer, off when nobody does; a streaming Qu is not asked again', async () => {
  for (const stream of [0, 40]) {
    const sim = await quSim.start({ model: 'Qu-16', channel: 1, meterStreamMs: stream || undefined });
    const s = new Nrpn.Session(qu, nodeTcp);
    try {
      await s.connect('127.0.0.1', sim.port);
      await sleep(150);
      assert.strictEqual(sim.meterAsks, 0, 'no meters while nobody shows them');
      const P = qu.meterPoints('Qu-16');
      sim.meter(P[0][0], -20);
      sim.meter(P[16][1], -3);
      sim.meter(P[26][0], 2);
      let got = 0;
      let last = null;
      const off = s.onMeters((addr, db) => { assert.strictEqual(addr, 'qu'); got++; last = db; });
      await until(() => got >= 5, 2000, 'meters keep coming');
      assert.strictEqual(last[0], -20, 'input 1');
      assert.strictEqual(last[16], -3, 'ST 1, its louder side');
      assert.strictEqual(last[26], 2, 'LR');
      assert.strictEqual(last[1], -90);
      if (stream) assert.strictEqual(sim.meterAsks, 1, 'a Qu that keeps sending is asked once');
      else assert.ok(sim.meterAsks >= 5, 'a Qu that answers once is asked again: ' + sim.meterAsks);
      off();
      await until(() => !sim.meterOn, 1000, 'meters off');
      const asks = sim.meterAsks;
      await sleep(300);
      assert.strictEqual(sim.meterAsks, asks, 'nobody listens: nothing asked');
      // a redial with someone listening asks again
      s.onMeters(() => {});
      await until(() => sim.meterOn, 1000, 'meters on again');
      sim.drop();
      await until(() => s.status === 'online' && sim.hellos === 2 && sim.meterOn, 5000, 'meters after a redial');
    } finally { s.disconnect(); sim.close(); }
  }
});

test('Qu: an SQ (or anything without the Qu hello) is not taken for a Qu', async () => {
  const sim = await sqSim.start();
  const s = new Nrpn.Session(qu, nodeTcp);
  try {
    await assert.rejects(s.connect('127.0.0.1', sim.port, 800), /No answer from the Qu/);
    assert.strictEqual(s.status, 'failed');
  } finally { s.disconnect(); sim.close(); }
});

// --- Allen & Heath CQ (the SQ's scheme on fewer channels, core/nrpn.js with no hooks) --------

test('CQ addresses and values as the protocol document gives them', () => {
  const ad = (p) => { const a = cq.address(p); return hx([a >> 7, a & 127]); };
  // mutes
  assert.strictEqual(ad('/ip/1/mute'), '00 00');
  assert.strictEqual(ad('/st/1/mute'), '00 18');
  assert.strictEqual(ad('/bt/1/mute'), '00 1E');
  assert.strictEqual(ad('/lr/mute'), '00 44');
  assert.strictEqual(ad('/out/6/mute'), '00 4A');
  assert.strictEqual(ad('/dca/4/mute'), '02 03');
  assert.strictEqual(ad('/mgrp/1'), '04 00');
  // levels: to LR, to the outputs, masters
  assert.strictEqual(ad('/usb/1/lr'), '40 1C');
  assert.strictEqual(ad('/fx/4/lr'), '40 3F');
  assert.strictEqual(ad('/ip/16/out/6'), '41 7D');
  assert.strictEqual(ad('/st/1/out/1'), '42 64');
  assert.strictEqual(ad('/st/2/out/5'), '43 00');
  assert.strictEqual(ad('/bt/1/out/1'), '43 2C');
  assert.strictEqual(ad('/fx/1/out/1'), '46 14');
  assert.strictEqual(ad('/fx/4/out/6'), '46 3D');
  assert.strictEqual(ad('/lr/lvl'), '4F 00');
  assert.strictEqual(ad('/out/1/lvl'), '4F 01');
  assert.strictEqual(ad('/dca/1/lvl'), '4F 20');
  // pan to LR
  assert.strictEqual(ad('/st/2/pan'), '50 1A');
  assert.strictEqual(ad('/fx/1/pan'), '50 3C');
  // the document's values: -inf 00 00, 0 dB 62 00, +10 dB 7F 40; pan 00 00, 40 00, 7F 7F; mute VF 01
  assert.strictEqual(cq.toValue('/ip/1/lr', 0), 0);
  assert.strictEqual(cq.toValue('/ip/1/lr', 0.75), 0x62 * 128);
  assert.strictEqual(cq.toValue('/ip/1/lr', 1), 0x7f * 128 + 0x40);
  assert.strictEqual(cq.fromValue('/ip/1/lr', 16383), 1, 'above +10 dB reads as the top');
  for (const f of [0, 0.1, 0.5, 0.75, 0.9, 1]) assert.ok(Math.abs(cq.fromValue('/lr/lvl', cq.toValue('/lr/lvl', f)) - f) < 1e-4, 'round trip ' + f);
  assert.strictEqual(cq.toValue('/ip/1/pan', 0.5), 0x40 * 128, 'centre 40 00');
  assert.strictEqual(cq.toValue('/ip/1/pan', 1), 16383);
  assert.strictEqual(cq.fromValue('/ip/1/pan', 0x40 * 128), 0.5);
  assert.strictEqual(cq.toValue('/ip/1/mute', 0), 1, 'muted: VF 01');
  assert.deepStrictEqual(Nrpn.setMessage(0, cq.address('/ip/1/mute'), 1), [0xb0, 0x63, 0, 0xb0, 0x62, 0, 0xb0, 6, 0, 0xb0, 0x26, 1], 'Ip1 mute on, as written');
  const L = cq.layout();
  assert.deepStrictEqual(L.inputs.map((s) => s.label), ['Ip 1', 'Ip 2', 'Ip 3', 'Ip 4', 'Ip 5', 'Ip 6', 'Ip 7', 'Ip 8', 'Ip 9', 'Ip 10', 'Ip 11', 'Ip 12', 'Ip 13', 'Ip 14', 'Ip 15', 'Ip 16', 'ST 1', 'ST 2', 'USB', 'BT']);
  assert.strictEqual(L.targets.length, 6);
  assert.strictEqual(L.dcas.length, 4);
  assert.ok(L.returns.every((r) => r.on === null), 'no FX return mute in the document');
  const addrs = L.params.map((p) => cq.address(p.path));
  assert.strictEqual(new Set(addrs).size, addrs.length, 'every parameter has its own address');
  assert.ok(!addrs.includes(cq.sqOnlyAddress), 'the SQ-only address is not a CQ parameter');
  assert.ok(sq.layout().params.some((p) => sq.address(p.path) === cq.sqOnlyAddress), 'it is an SQ parameter (Aux 12 mute)');
});

test('CQ: connect over TCP, read everything, both directions', async () => {
  const sim = await sqSim.start({ profile: cq });
  const s = new Nrpn.Session(cq, nodeTcp);
  try {
    const info = await s.connect('127.0.0.1', sim.port);
    assert.strictEqual(info.model, 'CQ');
    assert.strictEqual(s.values.size, s.layout.params.length, 'every parameter answered');
    assert.strictEqual(s.get('/st/2/lr'), 0.75, '0 dB');
    assert.strictEqual(s.get('/bt/1/pan'), 0.5, 'centre');
    assert.strictEqual(s.get('/dca/3/mute'), 1);
    s.set('/usb/1/out/4', 0.75);
    s.set('/dca/2/lvl', 1);
    s.set('/out/6/mute', 0);
    await until(() => sim.state.get('/usb/1/out/4') === 0x62 * 128 && sim.state.get('/dca/2/lvl') === 0x7f * 128 + 0x40 && sim.state.get('/out/6/mute') === 1, 1000, 'sets reach the CQ');
    sim.change('/ip/9/pan', 0);
    await until(() => s.get('/ip/9/pan') === 0, 1000, "the console's change");
    // an SQ session on a CQ: everything the two share answers, the SQ-only address does not
    const probe = new Nrpn.Session(sq, nodeTcp);
    await probe.connect('127.0.0.1', sim.port);
    const sqOnly = probe.layout.params.find((p) => sq.address(p.path) === cq.sqOnlyAddress).path;
    assert.ok(probe.values.size > 0 && !probe.values.has(sqOnly), 'tells a CQ from an SQ');
    probe.disconnect();
  } finally { s.disconnect(); sim.close(); }
});

// --- Allen & Heath dLive and Avantis (one scheme: a MIDI channel per channel type) ----------

test('dLive/Avantis messages byte for byte, as the protocol documents write them', () => {
  dlive.layoutFor('dLive');
  // base channel N = 12 (B): inputs on B, aux on N+2 = 1 (wraps), FX returns, mains, DCAs on N+4
  assert.strictEqual(hx(dlive.encode(11, '/ip/1/lvl', dlive.toValue('/ip/1/lvl', 0.75))), 'BB 63 00 BB 62 17 BB 06 6B', 'Ip 1 fader at 0 dB');
  assert.strictEqual(hx(dlive.encode(0, '/ip/128/lvl', dlive.toValue('/ip/128/lvl', 1))), 'B0 63 7F B0 62 17 B0 06 7F', '+10 dB');
  assert.strictEqual(hx(dlive.encode(0, '/ip/2/mute', dlive.toValue('/ip/2/mute', 0))), '90 01 7F 90 01 00', 'muted');
  assert.strictEqual(hx(dlive.encode(0, '/ip/2/mute', dlive.toValue('/ip/2/mute', 1))), '90 01 3F 90 01 00', 'not muted');
  assert.strictEqual(hx(dlive.encode(0, '/main/1/lvl', 0)), 'B4 63 30 B4 62 17 B4 06 00', 'Main 1 on N+4');
  assert.strictEqual(hx(dlive.encode(0, '/dca/24/mute', 1)), '94 4D 7F 94 4D 00');
  assert.strictEqual(hx(dlive.encode(0, '/mgrp/1', 1)), '94 4E 7F 94 4E 00', 'mute group 1 (dLive 4E)');
  assert.strictEqual(hx(dlive.encode(0, '/ip/3/send/saux2', 0x6b)), 'F0 00 00 1A 50 10 01 00 00 0D 02 02 41 6B F7', 'Ip 3 to stereo Aux 2');
  assert.strictEqual(hx(dlive.encode(0, '/fxr/1/send/aux62', 0x10)), 'F0 00 00 1A 50 10 01 00 04 0D 20 02 3D 10 F7', 'FX Rtn 1 to Aux 62');
  assert.strictEqual(hx(dlive.query(0, '/ip/5/name')), 'F0 00 00 1A 50 10 01 00 00 01 04 F7');
  assert.strictEqual(hx(dlive.query(0, '/aux/1/col')), 'F0 00 00 1A 50 10 01 00 02 04 00 F7');
  assert.strictEqual(hx(dlive.query(0, '/ip/1/lvl')), 'F0 00 00 1A 50 10 01 00 00 05 0B 17 00 F7');
  assert.strictEqual(hx(dlive.query(0, '/ip/1/mute')), 'F0 00 00 1A 50 10 01 00 00 05 09 00 F7');
  assert.strictEqual(hx(dlive.query(0, '/ip/1/send/aux1')), 'F0 00 00 1A 50 10 01 00 00 05 0F 0D 00 02 00 F7');
  // what comes back, on any of the five channels from N
  assert.deepStrictEqual(dlive.key(0x00, 0x17, 0x6b, -1, 3, 3), { addr: dlive.address('/ip/1/lvl'), raw: 0x6b });
  assert.strictEqual(dlive.key(0x00, 0x18, 0x7f, -1, 3, 3), null, 'main assign: not shown');
  assert.deepStrictEqual(dlive.noteKey(0x36, 0x7f, 7, 3), { addr: dlive.address('/dca/1/mute'), raw: 1 });
  assert.deepStrictEqual(dlive.noteKey(0x4e, 0x3f, 7, 3), { addr: dlive.address('/mgrp/1'), raw: 0 });
  const H = [0xf0, 0, 0, 0x1a, 0x50, 0x10, 1, 0];
  assert.deepStrictEqual(dlive.fromSysex(H.concat([5, 0x02, 0x00, 0x57, 0x65, 0x64, 0x67, 0x65, 0xf7]), 3), [{ addr: dlive.address('/aux/1/name'), raw: 'Wedge' }]);
  assert.deepStrictEqual(dlive.fromSysex(H.concat([3, 0x05, 0x00, 0x01, 0xf7]), 3), [{ addr: dlive.address('/ip/1/col'), raw: 1 }]);
  assert.deepStrictEqual(dlive.fromSysex(H.concat([3, 0x0d, 0x01, 0x05, 0x40, 0x22, 0xf7]), 3), [{ addr: dlive.address('/ip/2/send/saux1'), raw: 0x22 }]);
  // connect: Input 65's name, then Input 1's, on channels 1-12; the answer says N and the model
  const hello = dlive.hello();
  assert.strictEqual(hello.length, 12 * 24);
  assert.strictEqual(hx(hello.slice(0, 24)), 'F0 00 00 1A 50 10 01 00 00 01 40 F7 F0 00 00 1A 50 10 01 00 00 01 00 F7');
  assert.deepStrictEqual(dlive.helloReply(H.concat([9, 0x02, 0x40, 0xf7])), { channel: 9, model: 'dLive' });
  assert.deepStrictEqual(dlive.helloReply(H.concat([9, 0x02, 0x00, 0x4b, 0xf7])), { channel: 9, model: 'Avantis' });
  assert.strictEqual(dlive.helloReply([0xf0, 0, 0, 0x1a, 0x50, 0x11, 1, 0, 0, 0x11, 1, 1, 9, 0xf7]), null, 'a Qu answer is not ours');
  // layouts
  const D = dlive.layoutFor('dLive');
  assert.strictEqual(D.inputs.length, 128);
  assert.strictEqual(D.returns.length, 16);
  assert.strictEqual(D.dcas.length, 24);
  assert.strictEqual(D.targets.length, 62 + 31);
  const A = dlive.layoutFor('Avantis');
  assert.strictEqual(A.inputs.length, 64);
  assert.strictEqual(A.dcas.length, 16);
  assert.strictEqual(A.targets.length, 40 + 20);
  assert.strictEqual(hx(dlive.encode(0, '/mgrp/8', 1)), '94 4D 7F 94 4D 00', 'Avantis mute group 8 at 4D');
  const addrs = A.params.map((p) => dlive.address(p.path));
  assert.strictEqual(new Set(addrs).size, addrs.length, 'every parameter has its own address');
  const P = dlive.prune(A, (p) => p === '/aux/3/name' || p === '/saux/2/name');
  assert.deepStrictEqual(P.targets.map((t) => t.label), ['Aux 3', 'St Aux 2']);
  assert.deepStrictEqual(Object.keys(P.inputs[0].sends), ['aux3', 'saux2']);
  assert.deepStrictEqual(dlive.prune(dlive.layoutFor('Avantis'), () => false).targets.length, 8, 'no names: the first eight aux');
  // levels: -inf 00, 0 dB 6B, +10 dB 7F
  assert.strictEqual(dlive.toValue('/ip/1/lvl', 0), 0);
  assert.strictEqual(dlive.fromValue('/ip/1/lvl', 0x6b), 0.75);
  assert.strictEqual(dlive.fromValue('/ip/1/lvl', 0x7f), 1);
  dlive.layoutFor('dLive');
});

for (const [model, channel, gets] of [['dLive', 11, true], ['Avantis', 4, false]]) {
  test(model + ': connect over TCP, the MIDI channel found, the mixes set up, both directions' + (gets ? '' : ' (no gets)'), async () => {
    const sim = await dliveSim.start({ model, channel, gets, mixes: ['aux1', 'aux4', 'saux1'] });
    const s = new Nrpn.Session(dlive, nodeTcp);
    const other = new Nrpn.Session(dlive, nodeTcp);
    try {
      const info = await s.connect('127.0.0.1', sim.port);
      assert.strictEqual(info.model, model);
      assert.strictEqual(s.channel, channel, 'the base MIDI channel from the answer');
      assert.strictEqual(s.layout.inputs.length, model === 'dLive' ? 128 : 64);
      assert.deepStrictEqual(s.layout.targets.map((t) => t.label), ['Aux 1', 'Aux 4', 'St Aux 1'], 'only the mixes set up on the console');
      assert.strictEqual(s.get('/ip/1/name'), 'Kick');
      assert.strictEqual(s.get('/ip/1/col'), 1, 'red');
      assert.strictEqual(s.get('/aux/1/name'), 'Wedge');
      assert.strictEqual(s.get('/saux/1/name'), 'IEM Drums');
      assert.strictEqual(s.get('/fxr/1/name'), 'Reverb 1');
      if (gets) {
        assert.strictEqual(s.values.size, s.layout.params.length, 'every value answered');
        assert.strictEqual(s.get('/ip/1/lvl'), 0.75, '0 dB');
        assert.strictEqual(s.get('/ip/1/mute'), 1, 'not muted');
        assert.strictEqual(s.get('/ip/1/send/aux4'), 0, 'send off');
      } else {
        assert.strictEqual(s.get('/ip/1/lvl'), undefined, 'no gets: a level is unknown until it changes');
      }
      await other.connect('127.0.0.1', sim.port);
      // sets: a fader, a mute, a send, a mute group, a DCA
      s.set('/ip/7/lvl', 1);
      s.set('/ip/2/mute', 0);
      s.set('/fxr/2/send/saux1', 0.75);
      s.set('/mgrp/3', 1);
      s.set('/dca/2/lvl', 0);
      const mg = (model === 'dLive' ? 0x4e : 0x46) + 2;
      await until(() => sim.chans.get('0/6').fader === 0x7f && sim.chans.get('0/1').mute === 1 && sim.sends.get('4/33>2/64') === 0x6b
        && sim.chans.get('4/' + mg).mute === 1 && sim.chans.get('4/55').fader === 0, 1000, 'sets reach the console');
      await until(() => other.get('/ip/7/lvl') === 1 && other.get('/ip/2/mute') === 0 && other.get('/fxr/2/send/saux1') === 0.75
        && other.get('/mgrp/3') === 1, 1000, 'the other remote hears them');
      // changes on the console
      sim.fader('0/2', 0);
      sim.mute('4/48', 1);
      sim.name('0/9', 'Gtr');
      sim.send('0/0>2/3', 0x7f);
      await until(() => s.get('/ip/3/lvl') === 0 && s.get('/main/1/mute') === 0 && s.get('/ip/10/name') === 'Gtr' && s.get('/ip/1/send/aux4') === 1, 1000, "the console's changes");
      // a dropped link: redial, find the channel again, read again
      sim.drop();
      await until(() => s.status === 'lost', 1000, 'lost');
      await until(() => s.status === 'online', 8000, 'back online');
      assert.strictEqual(s.get('/ip/10/name'), 'Gtr');
    } finally { s.disconnect(); other.disconnect(); sim.close(); }
  });
}

(async () => {
  for (const [name, fn] of tests) {
    try { await fn(); console.log('ok   ' + name); }
    catch (e) { failures++; console.log('FAIL ' + name + '\n     ' + (e.stack || e).toString().split('\n').slice(0, 3).join('\n     ')); }
  }
  console.log(failures ? failures + ' failed' : 'all ' + tests.length + ' passed');
  process.exit(failures ? 1 : 0);
})();
