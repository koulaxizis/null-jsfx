// Pairing of Remote for DAWs with our Live control surface and Bitwig controller script
// (scripts 1.3): a phone without a known key gets only 'auth' and moves nothing; the code the
// DAW shows pairs it once, with a new key kept on both sides; wrong codes lead to a pause;
// connections that never pair make room for those that do. The real scripts run in the
// software Live (ableton_host.cjs) and the software Bitwig (bitwig_host.cjs) over real TCP.
// Usage: node tests/daws/pairing_test.cjs   (needs python3)
// NULL JSFX • https://nulljsfx.tech • MIT License
'use strict';
const assert = require('assert');
const fs = require('fs');
const net = require('net');
const crypto = require('crypto');
const BW = require('../../android/web/daws/bitwig/bitwig.js');
const liveHost = require('./ableton_host.cjs');
const bitwigHost = require('./bitwig_host.cjs');

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let passed = 0;
const ok = (name) => { passed++; console.log('ok  ' + name); };

async function until(cond, what, ms = 4000) {
  const t0 = Date.now();
  while (!(await cond())) {
    if (Date.now() - t0 > ms) throw new Error('timed out: ' + what);
    await sleep(20);
  }
}

function nodeDial(port) {
  return (cb) => {
    const s = net.connect(port, '127.0.0.1');
    s.on('connect', () => cb.onOpen());
    s.on('data', (d) => cb.onData(new Uint8Array(d)));
    s.on('close', () => cb.onClose(''));
    s.on('error', () => {});
    return { send(bytes) { s.write(Buffer.from(bytes)); }, close() { s.destroy(); } };
  };
}

/** A plain TCP client speaking the protocol by hand: send(obj), lines (parsed), closed. */
function raw(port) {
  const s = net.connect(port, '127.0.0.1');
  const c = { lines: [], closed: false, buf: '' };
  s.on('data', (d) => {
    c.buf += d;
    let nl;
    while ((nl = c.buf.indexOf('\n')) >= 0) {
      const line = c.buf.slice(0, nl).replace(/^[\0-\x1f]+/, '');
      c.buf = c.buf.slice(nl + 1);
      try { c.lines.push(JSON.parse(line)); } catch (e) { /* a length header */ }
    }
  });
  s.on('close', () => { c.closed = true; });
  s.on('error', () => {});
  c.ready = new Promise((r) => s.on('connect', r));
  c.send = (obj) => {
    const body = Buffer.from(JSON.stringify(obj));
    const h = Buffer.alloc(4);
    h.writeUInt32BE(body.length);
    s.write(Buffer.concat([h, body]));
  };
  c.close = () => s.destroy();
  c.types = () => c.lines.map((m) => m.t);
  return c;
}

/** A key store as the app keeps one (desk.js keysFor), in memory. */
function memoryKeys(initial = '') {
  const k = { value: initial, sets: 0 };
  return { store: k, keys: { get: () => k.value, set: (v) => { k.value = v; k.sets++; } } };
}

// The two DAWs behind one face: start(opts), the code shown, the document's first fader and
// mute, and the hashes kept for the paired phones.
const DAWS = [
  {
    name: 'Live',
    start: (o) => liveHost.start(o),
    code: (d) => d.code(),
    state: async (d) => { const t = (await d.get()).tracks[0]; return { vol: t.vol, mute: t.mute, play: (await d.get()).play }; },
    hashes: (d) => JSON.parse(fs.readFileSync(d.keysFile, 'utf8')).keys,
    hash: (k) => crypto.createHash('sha256').update(k).digest('hex'),
  },
  {
    name: 'Bitwig',
    start: (o) => bitwigHost.start(o),
    code: (d) => d.host.code(),
    state: async (d) => ({ vol: d.doc.tracks[0].vol, mute: !!d.doc.tracks[0].mute, play: d.host.transport.play }),
    hashes: (d) => d.host.keys.split(',').filter(Boolean),
    hash: (k) => crypto.createHash('sha256').update(k).digest('hex').slice(0, 32),
  },
];

async function unpairedDoesNothing(D) {
  const d = await D.start({ keys: [] });
  try {
    const before = await D.state(d);
    const c = raw(d.port);
    await c.ready;
    c.send({ t: 'hello' });
    await until(() => c.types().includes('auth'), 'auth');
    assert.deepStrictEqual(c.lines[0], { t: 'auth', need: 'code' });
    await until(() => D.code(d).length === 6, 'a code on screen');
    // everything a phone could ask for, with a key nobody paired and with none
    c.send({ t: 'hello', key: 'b2'.repeat(32) });
    c.send({ t: 'vol', i: 0, v: 0 });
    c.send({ t: 'mute', i: 0, on: true });
    c.send({ t: 'transport', do: 'play' });
    c.send({ t: 'open', i: 0, d: 0 });
    c.send({ t: 'param', id: '1', v: 0 });
    for (let k = 0; k < 500; k++) c.send({ t: 'vol', i: 0, v: (k % 10) / 10 });
    await sleep(600);
    assert.deepStrictEqual(await D.state(d), before, 'nothing moved');
    assert(c.types().every((t) => t === 'auth'), 'only auth came back: ' + [...new Set(c.types())]);
    c.close();
  } finally { d.close(); }
  ok(`${D.name}: without a key only auth comes back, and nothing moves`);
}

async function pairOnce(D) {
  const d = await D.start({ keys: [] });
  try {
    const mem = memoryKeys();
    const s = new BW.Session({ dial: nodeDial(d.port), keys: mem.keys });
    const auths = [];
    s.on('auth', (a) => auths.push(a));
    s.connect();
    await until(() => auths.length === 1, 'asked for a code');
    assert.deepStrictEqual(auths[0], { need: 'code', wrong: false, s: 0 });
    assert.strictEqual(s.strips.size, 0);
    await until(() => D.code(d).length === 6, 'a code on screen');
    const code = D.code(d);
    const wrong = String((Number(code) + 1) % 1000000).padStart(6, '0');
    s.pair(wrong);
    await until(() => auths.length === 2, 'wrong code answered');
    assert.deepStrictEqual(auths[1], { need: 'code', wrong: true, s: 0 });
    assert.strictEqual(mem.store.value, '', 'no key kept for a wrong code');
    s.pair(code.slice(0, 3) + ' ' + code.slice(3)); // as the DAW writes it
    await until(() => s.strips.size === 4, 'the strips after pairing');
    assert.strictEqual(auths[auths.length - 1], null);
    assert.match(mem.store.value, /^[0-9a-f]{64}$/);
    assert.deepStrictEqual(D.hashes(d), [D.hash(mem.store.value)], 'the DAW keeps a hash of the key, not the key');
    s.setMute(0, true);
    await until(async () => (await D.state(d)).mute === true, 'mute after pairing');
    s.close();

    // the same phone again: straight in, no code
    const shown = D.code(d);
    const s2 = new BW.Session({ dial: nodeDial(d.port), keys: mem.keys });
    let asked = 0;
    s2.on('auth', (a) => { if (a) asked++; });
    s2.connect();
    await until(() => s2.strips.size === 4, 'the strips again');
    assert.strictEqual(asked, 0);
    assert.strictEqual(mem.store.sets, 1);
    s2.close();
    // another phone still needs its own code; the old code is spent
    const c = raw(d.port);
    await c.ready;
    c.send({ t: 'pair', code: shown, key: 'c3'.repeat(32) });
    await until(() => c.types().includes('auth'), 'auth for a spent code');
    assert.strictEqual(c.lines[0].wrong, true);
    c.close();
  } finally { d.close(); }
  ok(`${D.name}: the code shown pairs once, the key is kept, the phone comes back without a code`);
}

async function tooManyWrong(D) {
  const d = await D.start({ keys: [] });
  try {
    const c = raw(d.port);
    await c.ready;
    c.send({ t: 'hello' });
    await until(() => D.code(d).length === 6, 'a code on screen');
    const code = D.code(d);
    let guess = 0;
    for (let k = 0; k < 5; k++) {
      if (String(guess).padStart(6, '0') === code) guess++;
      c.send({ t: 'pair', code: String(guess++).padStart(6, '0'), key: 'd4'.repeat(32) });
    }
    await until(() => c.lines.some((m) => m.need === 'wait'), 'a pause after five wrong codes');
    assert.strictEqual(c.lines.filter((m) => m.wrong).length, 4);
    // the right code during the pause does not pair
    c.send({ t: 'pair', code, key: 'd4'.repeat(32) });
    await until(() => c.lines.filter((m) => m.need === 'wait').length === 2, 'still waiting');
    assert(c.lines[c.lines.length - 1].s > 0 && c.lines[c.lines.length - 1].s <= 30);
    assert.deepStrictEqual(D.hashes(d), []);
    c.close();
  } finally { d.close(); }
  ok(`${D.name}: five wrong codes, then a pause of 30 s in which no code pairs`);
}

async function roomForPhones(D) {
  const d = await D.start();
  try {
    const squat = [];
    for (let k = 0; k < 8; k++) { const c = raw(d.port); await c.ready; squat.push(c); }
    await sleep(300);
    const s = new BW.Session({ dial: nodeDial(d.port), keys: (D.name === 'Live' ? liveHost : bitwigHost).testKeys() });
    s.connect();
    await until(() => s.strips.size === 4, 'a paired phone gets in past eight idle connections');
    await until(() => squat.some((c) => c.closed), 'the oldest idle connection made room');
    s.close();
    for (const c of squat) c.close();
  } finally { d.close(); }
  ok(`${D.name}: eight connections that never pair do not keep a paired phone out`);
}

async function forgetBitwig() {
  const d = await bitwigHost.start();
  try {
    const s = new BW.Session({ dial: nodeDial(d.port), keys: bitwigHost.testKeys() });
    const auths = [];
    s.on('auth', (a) => auths.push(a));
    s.connect();
    await until(() => s.strips.size === 4, 'paired phone in');
    d.host.forget(); // Settings > Controllers > Paired phones > Forget all phones
    assert.strictEqual(d.host.keys, '');
    await until(() => auths.some((a) => a && a.need === 'code'), 'asked for a code again', 8000);
    s.close();
  } finally { d.close(); }
  ok('Bitwig: "Forget all phones" drops the phones and they need a code again');
}

async function main() {
  const err = liveHost.syntaxCheck();
  assert.strictEqual(err, '', err);
  for (const D of DAWS) {
    await unpairedDoesNothing(D);
    await pairOnce(D);
    await tooManyWrong(D);
    await roomForPhones(D);
  }
  await forgetBitwig();
  console.log(`pairing_test: ${passed} passed`);
}

main().then(() => process.exit(0), (e) => { console.error(e); process.exit(1); });
