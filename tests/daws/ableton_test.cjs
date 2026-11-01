// The Ableton tab's session (android/web/daws/bitwig/bitwig.js, protocol 1, shared with Bitwig)
// against the real control surface (remote/ableton/NullJSFX) running in a software Live
// (ableton_live.py, through ableton_host.cjs) over real TCP.
// Usage: node tests/daws/ableton_test.cjs   (needs python3)
// NULL JSFX • https://nulljsfx.tech • MIT License
'use strict';
const assert = require('assert');
const net = require('net');
const BW = require('../../android/web/daws/bitwig/bitwig.js');
const live = require('./ableton_host.cjs');

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

function nodeDial(port, stats) {
  return (cb) => {
    const s = net.connect(port, '127.0.0.1');
    s.on('connect', () => cb.onOpen());
    s.on('data', (d) => cb.onData(new Uint8Array(d)));
    s.on('close', () => cb.onClose(''));
    s.on('error', () => {});
    return {
      send(bytes) { if (stats) stats.sent++; s.write(Buffer.from(bytes)); },
      close() { s.destroy(); },
    };
  };
}

async function main() {
  const err = live.syntaxCheck();
  assert.strictEqual(err, '', 'the control surface parses as Python 3.7 (Live 11): ' + err);
  ok('control surface: valid Python 3.7 for Live 11 and newer');

  const L = await live.start();
  const stats = { sent: 0 };
  const s = new BW.Session({ dial: nodeDial(L.port, stats) });
  const events = { link: [], plugins: [], params: [], strip: 0, meter: 0, time: [] };
  s.on('link', (up) => events.link.push(up));
  s.on('strip', () => events.strip++);
  s.on('meter', () => events.meter++);
  s.on('plugins', (ssid, list) => events.plugins.push([ssid, list]));
  s.on('params', (ssid, piid, list) => events.params.push([ssid, piid, list]));
  s.on('time', (t) => events.time.push(t));
  const doc = () => L.get();
  try {
    s.connect();
    await until(() => s.strips.size === 4, 'four strips');
    const st = [...s.strips.values()];
    assert.deepStrictEqual(st.map((x) => x.name), ['Vocal', 'Synth', 'FX 1', 'Master']);
    assert.deepStrictEqual(st.map((x) => x.type), ['Audio', 'MIDI', 'Return', 'Master']);
    assert.deepStrictEqual(st.map((x) => x.canRec), [true, true, false, false]);
    assert.strictEqual(st[1].mute, true);
    assert.strictEqual(st[1].rec, true);
    assert.strictEqual(st[0].volText, '0.0 dB');
    assert.strictEqual(st[1].pan, 0.3);
    assert.strictEqual(st[1].panText, '20L');
    assert.strictEqual(st[0].fader, live.UNITY);
    assert.deepStrictEqual(events.link, [true]);
    ok('hello: tracks, returns and the master with type, level, pan, mute, solo, arm');

    // a fader drag: coalesced on the way, the last value exact in Live
    const before = stats.sent;
    for (let k = 1; k <= 40; k++) { s.setFader(0, 0.5 + k * 0.005); await sleep(2); }
    await until(async () => Math.abs((await doc()).tracks[0].vol - 0.7) < 1e-9, 'fader at 0.7 in Live');
    const msgs = stats.sent - before;
    assert(msgs < 15, `coalesced: ${msgs} messages for 40 moves`);
    await until(() => s.strips.get(0).volText === '-3.4 dB', "Live's level text");
    s.setPan(0, 0.25);
    s.setMute(0, true);
    s.setSolo(2, true);
    s.setRec(0, true);
    s.setMute(3, true); // the master has no mute: nothing happens, nothing breaks
    s.setRec(2, true); // a return cannot be armed
    await until(async () => {
      const d = await doc();
      return d.tracks[0].mute && d.returns[0].solo && d.tracks[0].arm && Math.abs(d.tracks[0].pan + 0.5) < 1e-9;
    }, 'buttons and pan in Live');
    assert.strictEqual((await doc()).returns[0].arm, false);
    s.resetFader(0);
    s.resetPan(0);
    await until(async () => { const d = await doc(); return d.tracks[0].vol === live.UNITY && d.tracks[0].pan === 0; }, 'reset to 0 dB and centre');
    ok(`fader drag coalesced (${msgs} messages for 40 moves), pan, mute, solo, arm, resets; the master and returns refuse what they lack`);

    // Live changes something: the strip follows
    const n = events.strip;
    await L.set(['tracks', 1, 'mute'], false);
    await L.set(['tracks', 1, 'mixer_device', 'volume', 'value'], 0.3);
    await until(() => !s.strips.get(1).mute && s.strips.get(1).fader === 0.3 && events.strip > n, 'Live change on the phone');
    await L.set(['tracks', 0, 'name'], 'Φωνή');
    await until(() => s.strips.get(0).name === 'Φωνή', 'renamed track (UTF-8)');
    await until(() => events.meter > 8 && s.strips.get(0).meter > 0, 'meters');
    assert(s.strips.get(0).meter <= 1);
    ok('changes made in Live follow, a Greek name arrives whole, meters 0..1');

    // devices
    s.listPlugins(0);
    await until(() => events.plugins.some(([ssid, l]) => ssid === 0 && l.length === 3), 'device list');
    const list = s.devices.get(0);
    assert.deepStrictEqual(list.map((p) => p.name), ['NULL Tilt', 'NULL Delay', 'EQ Eight']);
    assert.deepStrictEqual(list.map((p) => p.plugin), [true, true, false]);
    ok('device list with plugin flags');

    // NULL Tilt: its parameter, normalized; "Device On" stays the bypass
    s.watchPlugin(0, 0);
    await until(() => events.params.some(([a, b, l]) => a === 0 && b === 0 && l.length === 1 && l[0].text), 'Tilt parameters');
    let p = events.params[events.params.length - 1][2];
    assert.deepStrictEqual(p.map((x) => x.name), ['Amount']);
    assert.strictEqual(p[0].value, 0.5);
    assert.strictEqual(p[0].text, '0.00');
    s.setParam(0, 0, 1, 0.875);
    await until(async () => Math.abs((await doc()).tracks[0].devices[0].parameters[1].value - 0.875) < 1e-9, 'Tilt Amount in Live');
    await L.set(['tracks', 0, 'devices', 0, 'parameters', 1, 'value'], 0.115);
    await until(() => { const l = events.params[events.params.length - 1][2]; return l[0].value === 0.115; }, 'Tilt follows Live');
    await until(() => events.params[events.params.length - 1][2][0].text === '-77.00', "Live's value text");
    ok('NULL Tilt: its parameter by name, set to 0.875 and following Live with its value text');

    // NULL Delay: the whole list comes again, nothing of Tilt mixes in
    const mark = events.params.length;
    s.watchPlugin(0, 1);
    await until(() => events.params.slice(mark).some(([, b, l]) => b === 1 && l.length === 15), 'Delay parameters');
    for (const [, piid, l] of events.params.slice(mark)) {
      assert.strictEqual(piid, 1);
      assert(!l.some((x) => x.name === 'Amount'), 'no Tilt parameter in the Delay');
    }
    p = events.params[events.params.length - 1][2];
    const time = p.find((x) => x.name === 'Time');
    s.setParam(0, 1, time.par, 0.75);
    await until(async () => Math.abs((await doc()).tracks[0].devices[1].parameters[time.par].value - 0.75) < 1e-9, 'Delay Time');
    // back to Tilt: the list comes again, though Live did not change
    const mark2 = events.params.length;
    s.watchPlugin(0, 0);
    await until(() => events.params.slice(mark2).some(([, b, l]) => b === 0 && l.length === 1), 'Tilt again');
    ok('NULL Delay: 15 sliders, Time set; switching devices sends each list whole');

    // a quantized or ranged device of Live's own: values over its own min and max
    s.watchPlugin(0, 2);
    await until(() => events.params.some(([, b, l]) => b === 2 && l.length === 2), 'EQ Eight parameters');
    s.setParam(0, 2, 1, 0.75);
    await until(async () => Math.abs((await doc()).tracks[0].devices[2].parameters[1].value - 7.5) < 1e-9, 'EQ gain +7.5 dB');
    ok("Live's own device: normalized over the parameter's range (0.75 of -15..15 = 7.5)");

    // the open device goes away in Live
    await L.remove(['tracks', 0, 'devices', 2]);
    await until(() => (s.devices.get(0) || []).length === 2, 'device list without EQ Eight');
    ok('a device deleted in Live leaves the list');

    // bypass, close, transport
    s.setPluginActive(0, 1, false);
    await until(async () => (await doc()).tracks[0].devices[1].on === false, 'Delay bypassed');
    await until(() => s.devices.get(0)[1].enabled === false, 'bypass shown');
    s.watchPlugin(null);
    s.play();
    await until(() => s.transport.play, 'playing');
    await until(() => events.time.length > 2 && events.time[events.time.length - 1] !== '001:1:1', 'clock runs');
    s.loop();
    s.record();
    await until(() => s.transport.loop && s.transport.rec, 'loop and record');
    s.stop();
    s.toStart();
    await until(async () => { const d = await doc(); return !s.transport.play && !d.play && d.time === 0; }, 'stopped at the start');
    ok('bypass, transport, clock');

    // a second phone at the same time gets everything, the first one keeps going
    const s2 = new BW.Session({ dial: nodeDial(L.port) });
    try {
      s2.connect();
      await until(() => s2.strips.size === 4 && s2.strips.get(0).name === 'Φωνή', 'second phone');
      s2.setMute(1, true);
      await until(() => s.strips.get(1).mute === true, 'the first phone sees the second');
    } finally {
      s2.close();
    }
    ok('two phones at once');

    // the script goes away (Live closes) and comes back on the same port
    const port = L.port;
    L.close();
    await until(() => events.link[events.link.length - 1] === false, 'link down');
    const L2 = await live.start({ port });
    try {
      await until(() => events.link[events.link.length - 1] === true, 'link up again', 6000);
      await until(() => s.strips.size === 4 && s.strips.get(0).name === 'Vocal', 'strips again');
    } finally {
      L2.close();
    }
    ok('link lost and found again');
  } catch (e) {
    console.error(L.log.join(''));
    throw e;
  } finally {
    s.close();
    L.close();
  }
  await paging();
  console.log(`ableton_test: ${passed} passed`);
}

// a Set with more tracks than the surface's window: the phone pages through them
async function paging() {
  const doc = live.defaultDocument();
  for (let k = doc.tracks.length + 1; doc.tracks.length < 40; k++) doc.tracks.push({ name: 'Track ' + k, kind: 'audio', vol: live.UNITY, pan: 0, color: 0x666666, devices: [] });
  const L = await live.start({ doc });
  const s = new BW.Session({ dial: nodeDial(L.port) });
  try {
    s.connect();
    // 40 tracks, a return and the master
    await until(() => s.strips.size === 32 && s.total === 42 && s.size === 32, 'a window of 32 of 42 tracks');
    s.scroll(32);
    await until(() => s.from === 32 && s.strips.size === 10 && s.strips.get(0).name === 'Track 33', 'the next page');
    assert.deepStrictEqual([s.strips.get(8).type, s.strips.get(9).type], ['Return', 'Master']);
    s.setMute(0, true);
    s.setFader(9, 0.5);
    await until(async () => { const d = await L.get(); return d.tracks[32].mute && Math.abs(d.master.vol - 0.5) < 1e-9; }, 'controls act on the tracks of the page');
    assert.strictEqual((await L.get()).tracks[0].mute, false, 'not on the first page');
    s.scroll(-5);
    await until(() => s.from === 0 && s.strips.get(0).name === 'Vocal', 'back to the start');
    ok('more than 32 tracks: a window of 32, paging forward and back, controls on the page shown');
  } catch (e) {
    console.error(L.log.join(''));
    throw e;
  } finally {
    s.close();
    L.close();
  }
}

main().catch((e) => { console.error(e); process.exit(1); });
