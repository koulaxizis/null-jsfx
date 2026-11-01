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

(async () => {
  for (const [name, fn] of tests) {
    try { await fn(); console.log('ok   ' + name); }
    catch (e) { failures++; console.log('FAIL ' + name + '\n     ' + (e.stack || e).toString().split('\n').slice(0, 3).join('\n     ')); }
  }
  console.log(failures ? failures + ' failed' : 'all ' + tests.length + ' passed');
  process.exit(failures ? 1 : 0);
})();
