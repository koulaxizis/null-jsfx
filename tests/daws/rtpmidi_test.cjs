// Tests network MIDI in Remote for DAWs (android/web/daws/core/rtpmidi.js): the packets byte for
// byte, the MIDI list with running status, deltas and System Exclusive in segments, our JSON in
// System Exclusive, and a session over real UDP against rtpmidi_peer.cjs (the computer's side):
// invitation on both ports, clock sync, MIDI both ways, a long message, BY, a refused and a
// silent computer, a computer that stops answering, and the Bitwig session running over it.
// Usage: node tests/daws/rtpmidi_test.cjs
'use strict';
const assert = require('assert');
const path = require('path');
const R = require('../../android/web/daws/core/rtpmidi.js');
const BW = require('../../android/web/daws/bitwig/bitwig.js');
const Peer = require('./rtpmidi_peer.cjs');

let failures = 0;
const tests = [];
const test = (name, fn) => tests.push([name, fn]);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
async function until(cond, ms = 2000, what = 'condition') {
  const t0 = Date.now();
  while (!cond()) { if (Date.now() - t0 > ms) throw new Error('timed out waiting for ' + what); await sleep(5); }
}

const { nodeUdp } = Peer;
const hex = (b) => Array.from(b, (x) => x.toString(16).padStart(2, '0')).join(' ');

test('session packets byte for byte', () => {
  assert.strictEqual(hex(R.control(0x494e, 0x01020304, 0xaabbccdd, 'Ph')),
    'ff ff 49 4e 00 00 00 02 01 02 03 04 aa bb cc dd 50 68 00');
  assert.strictEqual(hex(R.control(0x4259, 1, 2, 'ignored')), 'ff ff 42 59 00 00 00 02 00 00 00 01 00 00 00 02');
  const ck = R.clock(7, 1, [1, 2 ** 40 + 5, 3]);
  assert.strictEqual(ck.length, 36);
  assert.deepStrictEqual(R.parseControl(ck), { command: 0x434b, ssrc: 7, count: 1, t: [1, 2 ** 40 + 5, 3] });
  const ok = R.parseControl(R.control(0x4f4b, 9, 10, 'Studio PC'));
  assert.deepStrictEqual(ok, { command: 0x4f4b, version: 2, token: 9, ssrc: 10, name: 'Studio PC' });
  assert.strictEqual(R.parseControl(new Uint8Array([0x80, 0x61])), null);
});

test('RTP-MIDI: header, short and long lists, running status, deltas', () => {
  const p = R.rtp(0x1234, 0x01020304, 0x0a0b0c0d, [[0x90, 60, 100]]);
  assert.strictEqual(hex(p), '80 61 12 34 01 02 03 04 0a 0b 0c 0d 03 90 3c 64');
  const msgs = [[0x90, 60, 100], [0x90, 62, 90], [0xb0, 7, 127], [0xf8], [0xc0, 5], [0xf0, 0x7d, 1, 2, 3, 0xf7], [0xe0, 0, 64]];
  const q = R.rtp(1, 2, 3, msgs);
  assert.ok(q[12] & 0x80, 'long header');
  const got = [];
  R.parseRtp(q, {}, (m) => got.push(m));
  assert.deepStrictEqual(got, msgs);
  // what another sender may do: Z set (a delta before the first command), long deltas, a
  // journal after the list (J), running status across a real time byte
  const list = [0x00, 0x90, 60, 1, 0x81, 0x00, 61, 2, 0x00, 0xf8, 0x00, 62, 3];
  const r = new Uint8Array([0x80, 0x61, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0x40 | 0x20 | list.length, ...list, 0xde, 0xad]);
  const out = [];
  R.parseRtp(r, {}, (m) => out.push(m));
  assert.deepStrictEqual(out, [[0x90, 60, 1], [0x90, 61, 2], [0xf8], [0x90, 62, 3]]);
});

test('System Exclusive in segments over three packets; a cut one is dropped', () => {
  const st = {};
  const got = [];
  const seg = (bytes) => R.parseRtp(R.rtp(1, 0, 0, [bytes]), st, (m) => got.push(m));
  seg([0xf0, 0x7d, 1, 2, 0xf0]);
  seg([0xf7, 3, 4, 0xf0]);
  assert.deepStrictEqual(got, []);
  seg([0xf7, 5, 0xf7]);
  assert.deepStrictEqual(got, [[0xf0, 0x7d, 1, 2, 3, 4, 5, 0xf7]]);
  // a middle segment without a start is ignored, a start without an end then a new start
  got.length = 0;
  seg([0xf7, 9, 0xf0]);
  seg([0xf0, 1, 0xf0]);
  seg([0xf0, 2, 0xf7]);
  assert.deepStrictEqual(got, [[0xf0, 2, 0xf7]]);
});

test('our JSON in System Exclusive: ASCII only, every character back', () => {
  const obj = { t: 'tracks', list: [{ name: 'Φωνή 1 — lead "x"\n', v: 0.5 }] };
  const m = R.toSysex(obj);
  assert.deepStrictEqual(m.slice(0, 4), [0xf0, 0x7d, 0x4e, 0x4a]);
  assert.strictEqual(m[m.length - 1], 0xf7);
  assert.ok(m.slice(1, -1).every((x) => x < 0x80), 'seven bits');
  assert.deepStrictEqual(JSON.parse(R.fromSysex(m)), obj);
  assert.strictEqual(R.fromSysex([0xf0, 0x43, 1, 2, 3, 0xf7]), null, 'another maker\'s message');
});

test('a session: invitation on both ports, clock sync, MIDI both ways, long messages, BY', async () => {
  const peer = await Peer.start();
  const s = new R.Session({ udp: nodeUdp, name: 'NULL Remote', syncEvery: [50] });
  try {
    const info = await s.connect('127.0.0.1', peer.port);
    assert.strictEqual(info.name, 'Studio PC');
    assert.strictEqual(peer.phone.name, 'NULL Remote');
    assert.ok(peer.phone.data > 0, 'data port invited');
    assert.strictEqual(peer.invited, 2);
    await until(() => peer.syncs >= 3, 1000, 'clock syncs');
    const heard = [];
    s.onMidi((m) => heard.push(m));
    s.send([[0x90, 60, 100], [0x80, 60, 0]]);
    await until(() => peer.received.length === 2, 1000, 'notes at the computer');
    assert.deepStrictEqual(peer.received, [[0x90, 60, 100], [0x80, 60, 0]]);
    const big = [0xf0, 0x7d, 0x4e, 0x4a].concat(Array.from({ length: 5000 }, (_, i) => 0x20 + (i % 90)), [0xf7]);
    s.send([big]);
    await until(() => peer.received.length === 3, 1000, 'a long message, in segments');
    assert.deepStrictEqual(peer.received[2], big);
    peer.send([[0xb0, 7, 99], big]);
    await until(() => heard.length === 2, 1000, 'MIDI from the computer');
    assert.deepStrictEqual(heard, [[0xb0, 7, 99], big]);
    let why = null;
    s.onClose((w) => { why = w; });
    peer.bye();
    await until(() => why !== null, 1000, 'BY');
    assert.match(why, /ended/);
    assert.strictEqual(s.up, false);
  } finally { s.close(); peer.close(); }
});

test('a refused, a silent and a vanished computer', async () => {
  const no = await Peer.start({ refuse: true });
  await assert.rejects(new R.Session({ udp: nodeUdp }).connect('127.0.0.1', no.port), /refused/);
  no.close();
  const mute = await Peer.start({ silent: true });
  await assert.rejects(new R.Session({ udp: nodeUdp, inviteMs: 50 }).connect('127.0.0.1', mute.port), /No answer.*network MIDI session/);
  mute.close();
  const gone = await Peer.start();
  const s = new R.Session({ udp: nodeUdp, syncEvery: [40], lostMs: 300 });
  let why = null;
  s.onClose((w) => { why = w; });
  await s.connect('127.0.0.1', gone.port);
  gone.mute();
  await until(() => why !== null, 2000, 'lost');
  assert.match(why, /stopped answering/);
  gone.close();
  // closing says BY to the computer
  const polite = await Peer.start();
  const t = new R.Session({ udp: nodeUdp });
  await t.connect('127.0.0.1', polite.port);
  t.close();
  await until(() => polite.byes === 1, 1000, 'BY from the phone');
  polite.close();
});

test('the Bitwig session over network MIDI: hello, tracks with a Greek name, a fader back', async () => {
  const peer = await Peer.start();
  // the DAW script's side: answers hello with hello and tracks, records what the phone sends
  const fromPhone = [];
  peer.onMidi((m) => {
    const t = R.fromSysex(m);
    if (t == null) return;
    const msg = JSON.parse(t);
    fromPhone.push(msg);
    if (msg.t === 'hello') {
      peer.send([R.toSysex({ t: 'hello', proto: 1, ext: 'test', size: 8 }),
        R.toSysex({ t: 'tracks', total: 1, from: 0, list: [{ i: 0, name: 'Μπάσο', type: 'Audio', vol: 0.5, pan: 0.5, mute: false, solo: false, arm: false }] })]);
    }
  });
  const s = new BW.Session({ dial: R.dial('127.0.0.1', peer.port, { udp: nodeUdp }) });
  try {
    let link = false;
    s.on('link', (v) => { link = v; });
    s.connect();
    await until(() => link && s.strips.size === 1, 3000, 'tracks over network MIDI');
    assert.strictEqual(s.strips.get(0).name, 'Μπάσο');
    assert.strictEqual(s.size, 8);
    s.setFader(0, 0.25);
    await until(() => fromPhone.some((m) => m.t === 'vol' && m.v === 0.25), 1000, 'a fader to the DAW');
  } finally { s.close(); peer.close(); }
});

(async () => {
  for (const [name, fn] of tests) {
    try { await fn(); console.log('ok   ' + name); }
    catch (e) { failures++; console.log('FAIL ' + name + '\n     ' + (e.stack || e).toString().split('\n').slice(0, 4).join('\n     ')); }
  }
  nodeUdp.closeAll();
  console.log(failures ? failures + ' failed' : 'rtpmidi_test: ' + tests.length + ' passed');
  process.exit(failures ? 1 : 0);
})();
