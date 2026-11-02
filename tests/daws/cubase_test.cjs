// Tests the Cubase tab of Remote for DAWs end to end: the Bitwig session (android/web/daws/
// bitwig/bitwig.js) over network MIDI (core/rtpmidi.js, real UDP) to our MIDI Remote script for
// Cubase (remote/cubase/...) running in a software Cubase (cubase_host.cjs). Not Cubase itself:
// Bible C19. Usage: node tests/daws/cubase_test.cjs
'use strict';
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const BW = require('../../android/web/daws/bitwig/bitwig.js');
const R = require('../../android/web/daws/core/rtpmidi.js');
const Host = require('./cubase_host.cjs');
const { nodeUdp } = require('./rtpmidi_peer.cjs');

let failures = 0;
const tests = [];
const test = (name, fn) => tests.push([name, fn]);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
async function until(cond, ms = 3000, what = 'condition') {
  const t0 = Date.now();
  while (!cond()) { if (Date.now() - t0 > ms) throw new Error('timed out waiting for ' + what); await sleep(10); }
}
async function connected(cubase) {
  const s = new BW.Session({ dial: R.dial('127.0.0.1', cubase.port, { udp: nodeUdp }) });
  const ev = { plugins: new Map(), params: null, link: false };
  s.on('link', (v) => { ev.link = v; });
  s.on('plugins', (i, list) => ev.plugins.set(i, list));
  s.on('params', (i, d, params) => { ev.params = { i, d, params }; });
  s.connect();
  await until(() => ev.link && s.strips.size > 0, 4000, 'tracks from Cubase');
  return [s, ev];
}

test('the script: ES5 only, its driver and the port it looks for', () => {
  const file = path.join(__dirname, '../../remote/cubase/nulljsfx/remotefordaws/nulljsfx_remotefordaws.js');
  const src = fs.readFileSync(file, 'utf8');
  new vm.Script(src, { filename: file });
  const code = src.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/.*$/gm, '').replace(/'(?:[^'\\\n]|\\.)*'/g, "''");
  for (const [what, re] of [['arrow function', /=>/], ['let/const', /\b(let|const)\s/], ['template string', /`/],
    ['class', /\bclass\s/], ['spread', /\.\.\./], ['for...of', /\bfor\s*\([^)]*\bof\b/]]) {
    assert(!re.test(code), `the Cubase script uses ${what} (kept to ES5)`);
  }
  assert.match(path.basename(file), /^nulljsfx_remotefordaws\.js$/, 'vendor_device.js in vendor/device/');
});

test('connect: tracks with names, values, colours and sends; a fader, buttons and sends back', async () => {
  const cubase = await Host.start();
  const [s] = await connected(cubase);
  try {
    assert.deepStrictEqual(cubase.driver.detection[0].expect, [['in', 'NULL Remote'], ['out', 'NULL Remote']]);
    assert.strictEqual(cubase.driver.vendor, 'NULL JSFX');
    assert.strictEqual(s.size, 16);
    assert.strictEqual(s.strips.size, 3);
    const v = s.strips.get(0);
    const b = s.strips.get(1);
    assert.strictEqual(v.name, 'Vocal');
    assert.strictEqual(b.name, 'Μπάσο');
    assert.strictEqual(v.volText, '0.00');
    assert.strictEqual(b.mute, true);
    assert.strictEqual(b.rec, true);
    assert.strictEqual(v.color, '#cc4d33');
    assert.deepStrictEqual(v.sends, [0.25, 0, 0, 0]);
    assert.deepStrictEqual(s.sendNames, ['1', '2', '3', '4']);
    assert.strictEqual(s.total, 3, 'a window that is not full: no more pages');
    const P = cubase.project.tracks;
    s.setFader(1, 0.3);
    s.setMute(1, false);
    s.setSolo(0, true);
    s.setSend(0, 1, 0.6);
    s.setPan(2, 0.9);
    await until(() => P[1].vol === 0.3 && P[1].mute === 0 && P[0].solo === 1 && P[0].sends[1] === 0.6 && P[2].pan === 0.9, 2000, 'values in Cubase');
    s.resetFader(1);
    s.resetPan(2);
    await until(() => P[1].vol === Host.UNITY && P[2].pan === 0.5, 2000, 'reset to 0 dB (learnt) and centre');
    // a change in Cubase itself
    cubase.change((p) => { p.tracks[2].name = 'Gtr'; p.tracks[2].vol = 0.2; });
    await until(() => s.strips.get(2).name === 'Gtr' && Math.abs(s.strips.get(2).fader - 0.2) < 1e-9, 2000, 'a rename and a fader from Cubase');
    await until(() => s.strips.get(0).meter > 0.3, 2000, 'meters');
  } finally { s.close(); cubase.close(); }
});

test('inserts of the selected track: list, open, parameters both ways, bypass', async () => {
  const cubase = await Host.start();
  const [s, ev] = await connected(cubase);
  try {
    s.listPlugins(0);
    await until(() => ev.plugins.has(0), 2000, 'the Vocal inserts');
    assert.deepStrictEqual(ev.plugins.get(0).map((d) => [d.piid, d.name, d.enabled]), [[0, 'NULL Delay', true], [2, 'Frequency 2', true]]);
    // another track: Cubase selects it first
    s.listPlugins(1);
    await until(() => ev.plugins.has(1), 2000, 'the inserts of track 2');
    assert.strictEqual(cubase.project.tracks[1].sel, 1, 'track 2 selected in Cubase');
    assert.deepStrictEqual(ev.plugins.get(1), []);
    // open NULL Delay on Vocal: Vocal is selected again, the parameters come
    s.watchPlugin(0, 0);
    await until(() => ev.params && ev.params.i === 0 && ev.params.params.length >= 2, 3000, 'Delay parameters');
    const tilt = Host.nullPlugin('delay');
    assert.deepStrictEqual(ev.params.params.map((p) => p.name), tilt.params.map((p) => p.name));
    assert.ok(Math.abs(ev.params.params[0].value - tilt.params[0].v) < 1e-9);
    s.setParam(0, 0, 1, 0.8);
    const ins = cubase.project.tracks[0].inserts[0];
    await until(() => ins.params[0].v === 0.8, 2000, 'a parameter to Cubase');
    cubase.change(() => { ins.params[1].v = 0.1; });
    await until(() => ev.params.params[1].value === 0.1, 2000, 'a parameter from Cubase');
    assert.match(ev.params.params[1].text, /10\.0 %/);
    s.setPluginActive(0, 2, false);
    await until(() => cubase.project.tracks[0].inserts[2].bypass === 1, 2000, 'bypass');
    await until(() => ev.plugins.get(0).find((d) => d.piid === 2).enabled === false, 2000, 'bypass reported');
  } finally { s.close(); cubase.close(); }
});

test('transport, and a window of 16 tracks paged through', async () => {
  const cubase = await Host.start({ tracks: 20 });
  const [s] = await connected(cubase);
  try {
    const tp = cubase.project.transport;
    let tr = null;
    s.on('transport', (t) => { tr = t; });
    s.play();
    await until(() => tp.play === 1 && tr && tr.play, 2000, 'play');
    s.stop();
    await until(() => tp.play === 0 && tr && !tr.play, 2000, 'stop');
    s.loop();
    s.record();
    await until(() => tp.cycle === 1 && tp.rec === 1, 2000, 'cycle and record');
    s.toStart();
    await until(() => tp.position === 0, 2000, 'to project start');
    assert.deepStrictEqual(cubase.commands, ['Transport/To Project Start']);
    // paging
    assert.strictEqual(s.strips.size, 16);
    assert.ok(s.total > s.size, 'the pager shows: more tracks may follow');
    assert.strictEqual(s.more, true);
    s.scroll(16);
    await until(() => s.from === 16 && s.strips.size === 4, 2000, 'the second page');
    assert.strictEqual(s.strips.get(0).name, 'Track 17');
    assert.strictEqual(s.total, 20);
    assert.strictEqual(s.more, false);
    s.setFader(3, 0.25);
    await until(() => cubase.project.tracks[19].vol === 0.25, 2000, 'a fader on the second page');
    s.scroll(0);
    await until(() => s.from === 0 && s.strips.get(0).name === 'Vocal', 2000, 'back to the first page');
  } finally { s.close(); cubase.close(); }
});

(async () => {
  for (const [name, fn] of tests) {
    try { await fn(); console.log('ok   ' + name); }
    catch (e) { failures++; console.log('FAIL ' + name + '\n     ' + (e.stack || e).toString().split('\n').slice(0, 4).join('\n     ')); }
  }
  nodeUdp.closeAll();
  console.log(failures ? failures + ' failed' : 'cubase_test: ' + tests.length + ' passed');
  process.exit(failures ? 1 : 0);
})();
