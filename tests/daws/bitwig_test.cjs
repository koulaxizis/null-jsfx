// The Bitwig connector of Remote for DAWs (android/web/daws/bitwig/bitwig.js) against the real
// controller script (remote/bitwig/NullJSFX/NullJSFX.control.js) running in a software Bitwig
// (bitwig_host.cjs) over real TCP, with and without a length header in front of Bitwig's sends.
// Usage: node tests/daws/bitwig_test.cjs
// NULL JSFX • https://nulljsfx.tech • MIT License
'use strict';
const assert = require('assert');
const fs = require('fs');
const net = require('net');
const path = require('path');
const vm = require('vm');
const BW = require('../../android/web/daws/bitwig/bitwig.js');
const host = require('./bitwig_host.cjs');

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let passed = 0;
const ok = (name) => { passed++; console.log('ok  ' + name); };

async function until(cond, what, ms = 3000) {
  const t0 = Date.now();
  while (!cond()) {
    if (Date.now() - t0 > ms) throw new Error('timed out: ' + what);
    await sleep(10);
  }
}

// a TCP connection the way NullShell gives it to the session
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

// Bitwig runs controller scripts in its own engine: keep the script to ES5
function scriptChecks() {
  const file = path.join(__dirname, '../../remote/bitwig/NullJSFX/NullJSFX.control.js');
  const src = fs.readFileSync(file, 'utf8');
  new vm.Script(src, { filename: file }); // parses
  const code = src.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/.*$/gm, '').replace(/'(?:[^'\\\n]|\\.)*'/g, "''");
  for (const [what, re] of [['arrow function', /=>/], ['let/const', /\b(let|const)\s/], ['template string', /`/],
    ['class', /\bclass\s/], ['spread', /\.\.\./], ['for...of', /\bfor\s*\([^)]*\bof\b/]]) {
    assert(!re.test(code), `the controller script uses ${what} (Bitwig's engine wants ES5)`);
  }
  assert(/^loadAPI\(\d+\);/m.test(src) && /host\.defineController\('NULL JSFX', 'Remote for DAWs'/.test(src));
  ok('controller script: ES5 only, loadAPI and defineController');
}

function readerChecks() {
  const got = [];
  const r = new BW.Reader((m) => got.push(m));
  const line = Buffer.from(JSON.stringify({ t: 'track', i: 0, name: 'Φωνή' }) + '\n' + JSON.stringify({ t: 'time', s: '001:1:1' }) + '\n');
  const head = Buffer.alloc(4);
  head.writeUInt32BE(line.length);
  const both = Buffer.concat([line, head, line]); // a plain send, then one with a header
  for (const b of both) r.push(new Uint8Array([b])); // a byte at a time
  assert.deepStrictEqual(got.map((m) => m.t), ['track', 'time', 'track', 'time']);
  assert.strictEqual(got[0].name, 'Φωνή');
  const f = BW.frame({ t: 'hello' });
  assert.strictEqual(new DataView(f.buffer).getUint32(0), f.length - 4);
  ok('reader: lines, a 4-byte header in front of a send, UTF-8 split across chunks; frames to Bitwig');
}

async function run(headerOut) {
  const tag = headerOut ? ' (Bitwig adds a header)' : '';
  const bw = await host.start({ headerOut });
  const stats = { sent: 0 };
  const s = new BW.Session({ dial: nodeDial(bw.port, stats) });
  const events = { strips: 0, meter: 0, link: [], plugins: [], params: [], strip: 0, time: [] };
  s.on('strips', () => events.strips++);
  s.on('strip', () => events.strip++);
  s.on('meter', () => events.meter++);
  s.on('link', (up) => events.link.push(up));
  s.on('plugins', (ssid, list) => events.plugins.push([ssid, list]));
  s.on('params', (ssid, piid, list) => events.params.push([ssid, piid, list]));
  s.on('time', (t) => events.time.push(t));
  try {
    s.connect();
    await until(() => s.strips.size === 4, 'four strips');
    const st = [...s.strips.values()];
    assert.deepStrictEqual(st.map((x) => x.name), ['Vocal', 'Synth', 'FX 1', 'Master']);
    assert.deepStrictEqual(st.map((x) => x.type), ['Audio', 'Instrument', 'Effect', 'Master']);
    assert.deepStrictEqual(st.map((x) => x.canRec), [true, true, false, false]);
    assert.strictEqual(st[1].mute, true);
    assert.strictEqual(st[1].rec, true);
    assert.strictEqual(st[0].volText, '0.0 dB');
    assert.deepStrictEqual(events.link, [true]);
    ok('hello: every track with type, level, pan, mute, solo, arm' + tag);

    // a fader drag: 40 moves in ~100 ms reach Bitwig as a few messages, the last value exact
    const before = stats.sent;
    for (let k = 1; k <= 40; k++) { s.setFader(0, 0.5 + k * 0.005); await sleep(2); }
    await until(() => Math.abs(bw.doc.tracks[0].vol - 0.7) < 1e-9, 'fader at 0.7 in Bitwig');
    const msgs = stats.sent - before;
    assert(msgs < 15, `coalesced: ${msgs} messages for 40 moves`);
    await until(() => s.strips.get(0).volText === '-2.2 dB', 'level text back from Bitwig');
    s.setPan(0, 0.25);
    s.setMute(0, true);
    s.setSolo(2, true);
    s.setRec(0, true);
    await until(() => bw.doc.tracks[0].mute && bw.doc.tracks[2].solo && bw.doc.tracks[0].arm && bw.doc.tracks[0].pan === 0.25, 'buttons and pan in Bitwig');
    s.resetFader(0);
    s.resetPan(0);
    await until(() => bw.doc.tracks[0].vol === host.UNITY && bw.doc.tracks[0].pan === 0.5, 'reset to 0 dB and centre');
    ok(`fader drag coalesced (${msgs} messages for 40 moves), pan, mute, solo, arm, double-tap resets` + tag);

    // Bitwig changes something: the strip follows
    const n = events.strip;
    bw.change((d) => { d.tracks[1].mute = false; d.tracks[1].vol = 0.3; });
    await until(() => !s.strips.get(1).mute && s.strips.get(1).fader === 0.3 && events.strip > n, 'Bitwig change on the phone');
    bw.change((d) => { d.tracks[0].name = 'Φωνή'; });
    await until(() => s.strips.get(0).name === 'Φωνή', 'renamed track (UTF-8)');
    await until(() => events.meter > 8 && s.strips.get(0).meter > 0, 'meters');
    assert(s.strips.get(0).meter <= 1);
    ok('changes made in Bitwig follow, a Greek name arrives whole, meters 0..1' + tag);

    // devices of the first track
    s.listPlugins(0);
    await until(() => events.plugins.some(([ssid, l]) => ssid === 0 && l.length === 3), 'device list');
    const list = s.devices.get(0);
    assert.deepStrictEqual(list.map((p) => p.name), ['NULL Tilt', 'NULL Delay', 'EQ+']);
    assert.deepStrictEqual(list.map((p) => p.plugin), [true, true, false]);
    ok('device list with plugin flags' + tag);

    // open NULL Tilt: its parameters, normalized
    s.watchPlugin(0, 0);
    await until(() => events.params.some(([a, b, l]) => a === 0 && b === 0 && l.length === 2 && l[0].text), 'Tilt parameters');
    let p = events.params[events.params.length - 1][2];
    assert.deepStrictEqual(p.map((x) => x.name), ['Amount', 'Bypass']);
    assert.strictEqual(p[0].value, 0.5);
    assert.deepStrictEqual([p[0].min, p[0].max, p[0].par], [0, 1, 1]);
    assert(bw.host.log.includes('channel 0') && bw.host.log.includes('select 0/0'), 'the script moved its own cursors');
    assert.strictEqual(bw.host.cursorFollows, false, 'its cursor track does not follow the selection');
    s.setParam(0, 0, 1, 0.875);
    await until(() => Math.abs(bw.doc.tracks[0].devices[0].params[0].value - 0.875) < 1e-6, 'Tilt Amount in Bitwig');
    // Bitwig moves it (automation, the mouse): the phone follows
    bw.change((d) => { d.tracks[0].devices[0].params[0].value = 0.115; });
    await until(() => { const l = events.params[events.params.length - 1][2]; return l[0].value === 0.115; }, 'Tilt follows Bitwig');
    p = events.params[events.params.length - 1][2];
    assert.strictEqual(p[0].text, '-77.00');
    ok('NULL Tilt: parameters by name, normalized, set to 0.875 within 1e-6 and following Bitwig' + tag);

    // switch to NULL Delay: nothing of Tilt mixes in
    const mark = events.params.length;
    s.watchPlugin(0, 1);
    await until(() => events.params.slice(mark).some(([a, b, l]) => b === 1 && l.length === 16), 'Delay parameters');
    for (const [, piid, l] of events.params.slice(mark)) {
      assert.strictEqual(piid, 1);
      assert(!l.some((x) => x.name === 'Amount'), 'no Tilt parameter in the Delay');
    }
    p = events.params[events.params.length - 1][2];
    const time = p.find((x) => x.name === 'Time');
    s.setParam(0, 1, time.par, 0.75);
    await until(() => Math.abs(bw.doc.tracks[0].devices[1].params[0].value - 0.75) < 1e-6, 'Delay Time');
    ok('NULL Delay: 15 sliders + 1, Time set; switching devices keeps the lists apart' + tag);

    // bypass, close, transport
    s.setPluginActive(0, 2, false);
    await until(() => bw.doc.tracks[0].devices[2].enabled === false, 'EQ+ bypassed');
    s.watchPlugin(null);
    s.play();
    await until(() => s.transport.play, 'playing');
    await until(() => events.time.length > 2 && events.time[events.time.length - 1] !== '001:1:1', 'clock runs');
    s.loop();
    s.record();
    await until(() => s.transport.loop && s.transport.rec, 'loop and record');
    s.stop();
    s.toStart();
    await until(() => !s.transport.play && bw.host.position === 0, 'stopped at the start');
    ok('bypass, transport, clock' + tag);

    // Bitwig goes away and comes back on the same port: the session finds it again
    const port = bw.port;
    bw.close();
    await until(() => events.link[events.link.length - 1] === false, 'link down');
    const bw2 = await host.start({ headerOut, port });
    try {
      await until(() => events.link[events.link.length - 1] === true, 'link up again', 5000);
      await until(() => s.strips.size === 4, 'strips again');
    } finally {
      bw2.close();
    }
    ok('link lost and found again' + tag);
  } finally {
    s.close();
    bw.close();
  }
}

// a project with more tracks than the script's window: the phone pages through them
async function paging() {
  const doc = host.defaultDocument();
  const master = doc.tracks.pop();
  for (let k = doc.tracks.length + 1; doc.tracks.length < 40; k++) {
    doc.tracks.push({ name: 'Track ' + k, type: 'Audio', vol: host.UNITY, pan: 0.5, mute: false, solo: false, arm: false, color: [0.4, 0.4, 0.4], devices: [] });
  }
  doc.tracks.push(master);
  const bw = await host.start({ doc });
  const s = new BW.Session({ dial: nodeDial(bw.port) });
  try {
    s.connect();
    await until(() => s.strips.size === 32 && s.total === 41 && s.size === 32, 'a window of 32 of 41 tracks');
    assert.strictEqual(s.from, 0);
    assert.strictEqual(s.strips.get(0).name, 'Vocal');
    s.scroll(32);
    await until(() => s.from === 32 && s.strips.size === 9 && s.strips.get(0).name === 'Track 33', 'the next page');
    assert.strictEqual(s.strips.get(8).name, 'Master');
    s.setMute(0, true);
    s.setFader(8, 0.5);
    await until(() => doc.tracks[32].mute === true && Math.abs(doc.tracks[40].vol - 0.5) < 1e-9, 'controls act on the tracks of the page');
    assert.strictEqual(doc.tracks[0].mute, false, 'not on the first page');
    s.scroll(1000);
    await until(() => s.from === 40 && s.strips.size === 1, 'scroll stops at the last track');
    s.scroll(-5);
    await until(() => s.from === 0 && s.strips.get(0).name === 'Vocal', 'back to the start');
    ok('more than 32 tracks: a window of 32, paging forward and back, controls on the page shown');
  } finally {
    s.close();
    bw.close();
  }
}

async function main() {
  scriptChecks();
  readerChecks();
  await run(false);
  await run(true);
  await paging();
  console.log(`bitwig_test: ${passed} passed`);
}

main().catch((e) => { console.error(e); process.exit(1); });
