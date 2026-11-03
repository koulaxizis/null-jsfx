// Tests the Mackie Control tab of Remote for DAWs end to end: the Mackie session
// (android/web/daws/mackie/mackie.js) over network MIDI (core/rtpmidi.js, real UDP) to a software
// DAW that drives a Mackie Control surface (mackie_host.cjs). Not a real DAW: Bible C26.
// Usage: node tests/daws/mackie_test.cjs
// NULL JSFX • https://nulljsfx.tech • MIT License
'use strict';
const assert = require('assert');
const M = require('../../android/web/daws/mackie/mackie.js');
const Host = require('./mackie_host.cjs');
const { nodeUdp } = require('./rtpmidi_peer.cjs');

let failures = 0;
const tests = [];
const test = (name, fn) => tests.push([name, fn]);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
async function until(cond, ms = 3000, what = 'condition') {
  const t0 = Date.now();
  while (!cond()) { if (Date.now() - t0 > ms) throw new Error('timed out waiting for ' + what); await sleep(10); }
}
async function connected(daw) {
  const s = new M.Session({ host: '127.0.0.1', port: daw.port, udp: nodeUdp });
  const ev = { strips: null, link: false, time: '', transport: null, meters: [] };
  s.on('link', (v) => { ev.link = v; });
  s.on('strips', (l) => { ev.strips = l; });
  s.on('time', (t) => { ev.time = t; });
  s.on('transport', (t) => { ev.transport = Object.assign({}, t); });
  s.on('meter', (st) => ev.meters.push([st.ssid, st.meter]));
  s.connect();
  await until(() => ev.strips && ev.strips.length === 9, 4000, 'the first bank from the DAW');
  return [s, ev];
}

test('protocol helpers: the time display and the names', () => {
  // "001.01.01.000": the digits come from the right (CC 0x40 is the last one)
  const d = [0x30, 0x30, 0x30, 0x71, 0x30, 0x71, 0x30, 0x71, 0x30, 0x30];
  assert.strictEqual(M.timeText(d), '001.01.01.000');
  assert.strictEqual(M.timeText([0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01]), 'A'); // 0x01 stands for 0x41
  const lcd = Array.from('Kick   Snare  ' + ' '.repeat(98));
  assert.strictEqual(M.nameOf(lcd, 0), 'Kick');
  assert.strictEqual(M.nameOf(lcd, 1), 'Snare');
  assert.strictEqual(M.nameOf(lcd, 2), '');
});

test('connect: the hello is confirmed, the DAW sends the bank: names, faders, lights, pans, time', async () => {
  const daw = await Host.start();
  const [s, ev] = await connected(daw);
  try {
    assert.strictEqual(ev.link, true);
    assert.strictEqual(daw.refreshes, 1, 'one full refresh after the hello');
    assert.strictEqual(daw.panMode, true, 'the V-pots are put on pan');
    await until(() => daw.versionAsked, 2000, 'the firmware answer');
    assert.match(daw.versionAsked, /^V\d/);
    assert.deepStrictEqual(ev.strips.map((x) => x.name), ['Kick', 'Snare', 'Hats', 'Bass', 'Keys', 'Guitar', 'Vox', 'Pad', 'Master']);
    assert.strictEqual(ev.strips[8].ssid, M.MASTER);
    assert.strictEqual(ev.strips[8].canRec, false);
    await until(() => Math.abs(s.strips.get(0).fader - 0.75) < 1e-3 && Math.abs(s.strips.get(M.MASTER).fader - 0.8) < 1e-3, 2000, 'faders');
    assert.strictEqual(s.strips.get(0).pan, 0.5);
    assert.strictEqual(s.more, true, 'a full bank: more may follow');
    assert.strictEqual(s.total, 16);
    await until(() => ev.time === '001.01.01.000', 2000, 'the time display');
    assert.deepStrictEqual(ev.transport, null, 'stopped: no change of the transport lights');
    daw.tick();
    await until(() => ev.time === '001.02.01.000', 2000, 'the time moving');
  } finally { s.close(); daw.close(); }
});

test('to the DAW: a fader with touch, mute, solo, arm, pan by V-pot ticks and its push', async () => {
  const daw = await Host.start();
  const [s] = await connected(daw);
  try {
    for (const v of [0.6, 0.5, 0.4]) { s.setFader(1, v); await sleep(30); }
    await until(() => Math.abs(daw.tracks[1].vol - 0.4) < 1e-3, 2000, 'the volume');
    assert.deepStrictEqual(daw.touches[0], [1, true], 'touched before it moves');
    await until(() => daw.touches.some(([n, on]) => n === 1 && !on), 2000, 'let go after the last move');
    s.setFader(M.MASTER, 0.3);
    await until(() => Math.abs(daw.master.vol - 0.3) < 1e-3, 2000, 'the master');

    s.setMute(2, true); s.setSolo(3, true); s.setRec(4, true);
    await until(() => s.strips.get(2).mute && s.strips.get(3).solo && s.strips.get(4).rec, 2000, 'the lights back');
    assert.ok(daw.tracks[2].mute && daw.tracks[3].solo && daw.tracks[4].arm);
    s.setMute(2, false);
    await until(() => !daw.tracks[2].mute && !s.strips.get(2).mute, 2000, 'mute off');
    s.setMute(2, false); // already off: nothing goes out, so it is not toggled back on
    await sleep(100);
    assert.strictEqual(daw.tracks[2].mute, false);

    for (const v of [0.4, 0.3, 0.25]) { s.setPan(0, v); await sleep(20); }
    await until(() => Math.abs(daw.tracks[0].pan - 0.25) < 1 / 64 + 1e-9, 2000, 'the pan');
    await sleep(600); // the ring from the DAW is shown again once let go
    assert.ok(Math.abs(s.strips.get(0).pan - 0.25) <= 0.05, 'the ring: ' + s.strips.get(0).pan);
    s.resetPan(0);
    await until(() => daw.tracks[0].pan === 0.5 && s.strips.get(0).pan === 0.5, 2000, 'centred by the push');
  } finally { s.close(); daw.close(); }
});

test('from the DAW: a volume, a new name, meters falling by themselves', async () => {
  const daw = await Host.start();
  const [s, ev] = await connected(daw);
  try {
    daw.setVolume(5, 0.2);
    await until(() => Math.abs(s.strips.get(5).fader - 0.2) < 1e-3, 2000, 'the volume');
    daw.rename(6, 'Lead');
    await until(() => ev.strips[6].name === 'Lead', 2000, 'the new name');
    daw.meter(0, 12);
    await until(() => s.strips.get(0).meter === 1, 2000, 'the meter');
    await until(() => s.strips.get(0).meter === 0, 5000, 'the meter falling');
    assert.ok(ev.meters.filter(([n]) => n === 0).length >= 5, 'it fell step by step');
  } finally { s.close(); daw.close(); }
});

test('paging by banks of eight, the transport and its lights', async () => {
  const daw = await Host.start({ tracks: 11 });
  const [s, ev] = await connected(daw);
  try {
    s.scroll(8);
    await until(() => ev.strips.length === 4 && ev.strips[0].name === 'Choir', 2000, 'the next bank');
    assert.strictEqual(daw.bank, 8);
    assert.deepStrictEqual(ev.strips.map((x) => x.name), ['Choir', 'FX', 'Organ', 'Master']);
    assert.strictEqual(s.more, false);
    assert.strictEqual(s.total, 11);
    assert.strictEqual(s.from, 8);
    s.scroll(0);
    await until(() => ev.strips[0].name === 'Kick', 2000, 'back');
    assert.strictEqual(s.from, 0);

    s.play();
    await until(() => ev.transport && ev.transport.play && !ev.transport.stop, 2000, 'playing');
    s.loop();
    await until(() => ev.transport.loop, 2000, 'cycle');
    s.record();
    await until(() => ev.transport.rec, 2000, 'recording');
    daw.tick(); daw.tick(); daw.tick(); daw.tick();
    await until(() => ev.time === '002.01.01.000', 2000, 'bar 2');
    s.toStart();
    await until(() => ev.transport.stop && !ev.transport.play && !ev.transport.rec, 2000, 'stopped');
    await until(() => ev.time === '001.01.01.000', 2000, 'back at the start');
  } finally { s.close(); daw.close(); }
});

test('the DAW asks who is there; the link drops and comes back with everything again', async () => {
  const daw = await Host.start();
  const [s, ev] = await connected(daw);
  try {
    daw.peer.send([[0xf0, 0x00, 0x00, 0x66, 0x14, 0x00, 0xf7]]);
    await until(() => daw.refreshes === 2, 2000, 'a second refresh after the device query');
    daw.peer.bye();
    await until(() => ev.link === false, 3000, 'the link down');
    await until(() => ev.link === true, 5000, 'the link back');
    await until(() => daw.refreshes === 3, 3000, 'a refresh after the new hello');
    s.setMute(0, true);
    await until(() => daw.tracks[0].mute, 2000, 'it works again');
  } finally { s.close(); daw.close(); }
});

(async () => {
  for (const [name, fn] of tests) {
    try { await fn(); console.log('ok   ' + name); } catch (e) { failures++; console.log('FAIL ' + name + '\n     ' + (e.stack || e).toString().split('\n').slice(0, 3).join('\n     ')); }
  }
  nodeUdp.closeAll();
  console.log(failures ? failures + ' of ' + tests.length + ' failed' : 'all ' + tests.length + ' passed');
  process.exit(failures ? 1 : 0);
})();
