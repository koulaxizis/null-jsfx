// Tests the FL Studio tab of Remote for DAWs end to end: the Bitwig session (android/web/daws/
// bitwig/bitwig.js) over network MIDI (core/rtpmidi.js, real UDP) to our MIDI script for FL
// Studio (remote/flstudio/...) running in Python against a software FL Studio (fl_host.cjs,
// fl_sim.py), once with Python's json module and once with the script's own JSON. Not FL Studio
// itself: Bible C20. Usage: node tests/daws/fl_test.cjs
'use strict';
const assert = require('assert');
const { execFileSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const BW = require('../../android/web/daws/bitwig/bitwig.js');
const R = require('../../android/web/daws/core/rtpmidi.js');
const Host = require('./fl_host.cjs');
const { nodeUdp } = require('./rtpmidi_peer.cjs');

const SCRIPT = path.join(__dirname, '../../remote/flstudio/NULL JSFX/device_NULLJSFX.py');
let failures = 0;
const tests = [];
const test = (name, fn) => tests.push([name, fn]);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
async function until(cond, ms = 3000, what = 'condition') {
  const t0 = Date.now();
  for (;;) {
    if (await cond()) return;
    if (Date.now() - t0 > ms) throw new Error('timed out waiting for ' + what);
    await sleep(20);
  }
}
async function connected(fl) {
  const s = new BW.Session({ dial: R.dial('127.0.0.1', fl.port, { udp: nodeUdp }) });
  const ev = { plugins: new Map(), params: null, link: false };
  s.on('link', (v) => { ev.link = v; });
  s.on('plugins', (i, list) => ev.plugins.set(i, list));
  s.on('params', (i, d, params) => { ev.params = { i, d, params }; });
  s.connect();
  await until(() => ev.link && s.strips.size > 0, 5000, 'tracks from FL Studio');
  return [s, ev];
}

test('the script: its name line, Python 3 syntax, only FL Studio\'s modules', () => {
  const src = fs.readFileSync(SCRIPT, 'utf8');
  assert.strictEqual(src.split('\n')[0], '# name=NULL JSFX Remote for DAWs', 'FL Studio lists the script by its first line');
  assert.match(path.basename(SCRIPT), /^device_[A-Za-z0-9]+\.py$/, 'FL Studio loads device_*.py');
  execFileSync(process.env.PYTHON || 'python3', ['-c', 'import ast,sys; ast.parse(open(sys.argv[1], encoding="utf-8").read())', SCRIPT]);
  const imports = [...src.matchAll(/^\s*import (\w+)/gm)].map((m) => m[1]);
  assert.deepStrictEqual(imports.sort(), ['device', 'json', 'mixer', 'plugins', 'transport']);
  assert.ok(/^[\x00-\x7f]*$/.test(src), 'ASCII only');
});

for (const noJson of [false, true]) {
  const how = noJson ? ' (the script\'s own JSON)' : '';

  test('connect: the mixer with names, values and colours; a fader and buttons back' + how, async () => {
    const fl = await Host.start({ noJson });
    const [s] = await connected(fl);
    try {
      assert.strictEqual(s.size, 32);
      assert.strictEqual(s.strips.size, 3, 'the master and the inserts in use');
      assert.strictEqual(s.total, 3);
      assert.strictEqual(s.more, false);
      const [m, v, b] = [0, 1, 2].map((k) => s.strips.get(k));
      assert.deepStrictEqual([m.name, m.type, m.canRec], ['Master', 'Master', false]);
      assert.deepStrictEqual([v.name, v.type, v.canRec], ['Vocal', 'Insert', true]);
      assert.strictEqual(b.name, 'Μπάσο');
      assert.strictEqual(v.volText, '0.0 dB');
      assert.strictEqual(v.fader, 0.8);
      assert.strictEqual(b.mute, true);
      assert.strictEqual(b.rec, true);
      assert.ok(Math.abs(b.pan - 0.3) < 1e-9, 'FL\'s pan -1..1 as 0..1');
      assert.strictEqual(v.color, '#cc4d33');
      assert.deepStrictEqual(s.sendNames, []);
      s.setFader(2, 0.3);
      s.setMute(2, false);
      s.setSolo(1, true);
      s.setPan(1, 0.9);
      s.setRec(2, false);
      s.setRec(1, true);
      await until(async () => {
        const T = (await fl.project()).tracks;
        return T[2].vol === 0.3 && T[2].mute === 0 && T[1].solo === 1 && Math.abs(T[1].pan - 0.8) < 1e-9 && T[2].arm === 0 && T[1].arm === 1;
      }, 3000, 'values in FL Studio');
      s.resetFader(2);
      s.resetPan(1);
      await until(async () => { const T = (await fl.project()).tracks; return T[2].vol === 0.8 && T[1].pan === 0; }, 3000, 'reset to 0 dB and centre');
      // a change in FL Studio itself
      fl.set(['tracks', 2, 'name'], 'Gtr');
      fl.set(['tracks', 2, 'vol'], 0.2);
      await until(() => s.strips.get(2).name === 'Gtr' && Math.abs(s.strips.get(2).fader - 0.2) < 1e-9, 3000, 'a rename and a fader from FL Studio');
      await until(() => s.strips.get(1).meter > 0.3, 3000, 'meters');
      // a new insert in use joins the window
      fl.set(['tracks', 4], { name: 'Drums' });
      await until(() => s.strips.size === 5 && s.strips.get(4).name === 'Drums' && s.strips.get(3).name === 'Insert 3', 3000, 'a new insert');
      assert.deepStrictEqual(fl.unhandled, []);
    } finally { s.close(); await fl.close(); }
  });

  test('effect slots: list, open, parameters both ways' + how, async () => {
    const fl = await Host.start({ noJson });
    const [s, ev] = await connected(fl);
    try {
      s.listPlugins(1);
      await until(() => ev.plugins.has(1), 3000, 'the Vocal slots');
      assert.deepStrictEqual(ev.plugins.get(1).map((d) => [d.piid, d.name, d.enabled]), [[0, 'NULL Delay', true], [2, 'Fruity Limiter', true]]);
      s.listPlugins(2);
      await until(() => ev.plugins.has(2), 3000, 'the slots of Μπάσο');
      assert.deepStrictEqual(ev.plugins.get(2), []);
      s.watchPlugin(1, 0);
      const delay = Host.nullPlugin('delay');
      await until(() => ev.params && ev.params.i === 1 && ev.params.params.length === delay.params.length, 4000, 'Delay parameters');
      assert.deepStrictEqual(ev.params.params.map((p) => p.name), delay.params.map((p) => p.name), 'the VST\'s own parameters, not its MIDI CCs');
      assert.ok(Math.abs(ev.params.params[0].value - delay.params[0].v) < 1e-6);
      s.setParam(1, 0, 1, 0.8);
      await until(async () => (await fl.project()).tracks[1].fx[0].params[0].v === 0.8, 3000, 'a parameter to FL Studio');
      fl.set(['tracks', 1, 'fx', 0, 'params', 1, 'v'], 0.1);
      await until(() => ev.params.params[1].value === 0.1 && /10\.0 %/.test(ev.params.params[1].text), 3000, 'a parameter from FL Studio');
      // the plugin in the slot is replaced: the phone hears the slot again
      fl.set(['tracks', 1, 'fx', 0], { name: 'Fruity Reeverb 2', params: [{ name: 'Size', v: 0.25 }] });
      await until(() => ev.plugins.get(1)[0].name === 'Fruity Reeverb 2', 3000, 'the new plugin in the list');
    } finally { s.close(); await fl.close(); }
  });
}

test('transport, and a window of 32 tracks paged through', async () => {
  const fl = await Host.start({ tracks: 40 });
  const [s] = await connected(fl);
  try {
    let tr = null;
    let time = '';
    s.on('transport', (t) => { tr = t; });
    s.on('time', (t) => { time = t; });
    s.play();
    await until(async () => (await fl.project()).transport.play === 1 && tr && tr.play, 3000, 'play');
    s.stop();
    await until(async () => (await fl.project()).transport.play === 0 && tr && !tr.play, 3000, 'stop');
    s.record();
    await until(async () => (await fl.project()).transport.rec === 1 && tr.rec, 3000, 'record');
    fl.set(['transport', 'pos'], 20);
    await until(() => time === '2:05:000', 3000, 'the song position');
    s.toStart();
    await until(async () => (await fl.project()).transport.pos === 0 && time === '1:01:000', 3000, 'to the start');
    s.loop(); // no loop switch in FL Studio's API: nothing happens, nothing breaks
    // paging
    assert.strictEqual(s.strips.size, 32);
    assert.strictEqual(s.total, 40);
    s.scroll(32);
    await until(() => s.from === 32 && s.strips.size === 8, 3000, 'the second page');
    assert.strictEqual(s.strips.get(0).name, 'Track 32');
    s.setFader(7, 0.25);
    await until(async () => (await fl.project()).tracks[39].vol === 0.25, 3000, 'a fader on the second page');
    s.scroll(0);
    await until(() => s.from === 0 && s.strips.get(1).name === 'Vocal', 3000, 'back to the first page');
  } finally { s.close(); await fl.close(); }
});

(async () => {
  for (const [name, fn] of tests) {
    try { await fn(); console.log('ok   ' + name); }
    catch (e) { failures++; console.log('FAIL ' + name + '\n     ' + (e.stack || e).toString().split('\n').slice(0, 4).join('\n     ')); }
  }
  nodeUdp.closeAll();
  console.log(failures ? failures + ' failed' : 'fl_test: ' + tests.length + ' passed');
  process.exit(failures ? 1 : 0);
})();
