// The Ardour/Mixbus connector (android/web/daws/ardour/ardour.js) against the software Ardour
// surface (ardour_sim.cjs) over real UDP: connect, strips, feedback, plugins, NULL plugin
// parameters in both directions (plain and normalized ranges), transport, link loss.
// Usage: node tests/daws/ardour_test.cjs
// NULL JSFX • https://nulljsfx.tech • MIT License
'use strict';
const assert = require('assert');
const dgram = require('dgram');
const path = require('path');
const A = require('../../android/web/daws/ardour/ardour.js');
const Osc = require('../../android/web/mixers/core/osc.js');
const sim = require('./ardour_sim.cjs');

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
async function until(cond, what, ms = 3000) {
  const t0 = Date.now();
  while (!cond()) { if (Date.now() - t0 > ms) throw new Error('timed out: ' + what); await sleep(10); }
}

function udp() {
  const sock = dgram.createSocket('udp4');
  const cbs = new Set();
  sock.on('message', (b, r) => { for (const cb of cbs) cb(new Uint8Array(b), r.address, r.port); });
  return new Promise((r) => sock.bind(0, '127.0.0.1', () => r({
    send: (host, port, bytes) => sock.send(Buffer.from(bytes), port, host),
    onPacket(cb) { cbs.add(cb); return () => cbs.delete(cb); },
    close: () => sock.close(),
  })));
}

let passed = 0;
function ok(name) { passed++; console.log('ok  ' + name); }

function unit() {
  // OSC 64-bit types Ardour sends
  const m = Osc.decode(Osc.encode('/x', [{ t: 'd', v: 0.1 }, { t: 'i', v: 3 }]))[0];
  assert.strictEqual(m.args[0].v, 0.1);
  assert.strictEqual(m.args[1].v, 3);
  ok('OSC d round trip');
  // the fader law is Ardour's (0 dB at 0.7818, +6 dB at the top)
  assert(Math.abs(A.faderToDb(A.dbToFader(-12)) + 12) < 1e-9);
  assert(Math.abs(A.dbToFader(0) - 0.78179) < 1e-4);
  assert.strictEqual(A.faderToDb(0), -Infinity);
  ok('fader law');
  // NULL plugins by name (bare, with the JSFX prefix or the GFX suffix), others not
  const fx = require('../../web/fx/index.json').effects;
  assert.strictEqual(A.nullEffect('Tilt', fx).id, 'tilt');
  assert.strictEqual(A.nullEffect('NULL Delay GFX', fx).id, 'delay');
  assert.strictEqual(A.nullEffect('ACE Reverb', fx), null);
  ok('NULL plugins found by name');
  // range mapping: identity for the slider range, linear for a normalized one
  const p = { min: 0, max: 1 };
  assert.strictEqual(A.toParam(1000, 0, 2000, p), 0.5);
  assert.strictEqual(A.fromParam(0.5, 0, 2000, p), 1000);
  assert.strictEqual(A.toParam(-150, -100, 100, { min: -100, max: 100 }), -100);
  assert.strictEqual(A.toParam(37, -100, 100, { min: -100, max: 100 }), 37);
  ok('slider ranges to parameter ranges');
}

async function live() {
  const ard = await sim.start();
  const t = await udp();
  const s = new A.Session({ transport: t, host: '127.0.0.1', port: ard.port });
  const events = { strips: 0, link: [] };
  s.on('strips', () => events.strips++);
  s.on('link', (up) => events.link.push(up));
  try {
    s.connect();
    await until(() => events.strips > 0, 'strip list');
    assert.deepStrictEqual(ard.log.slice(0, 2), ['/set_surface', '/strip/list']);
    assert.strictEqual(ard.surface.feedback & 16384, 16384, 'asks for /reply');
    const strips = [...s.strips.values()];
    assert.deepStrictEqual(strips.map((x) => [x.ssid, x.type, x.name]), [[1, 'AT', 'Vocal'], [2, 'AT', 'Guitar'], [3, 'B', 'Reverb Bus'], [4, 'MA', 'Master']]);
    assert.strictEqual(s.strips.get(2).mute, true);
    assert.strictEqual(s.strips.get(2).rec, true);
    assert.strictEqual(s.strips.get(3).canRec, false);
    assert.strictEqual(s.rate, 48000);
    ok('strip list (types, names, mute, record, sample rate)');

    await until(() => s.strips.get(1).fader != null && s.strips.get(1).meter > 0, 'fader feedback and meters');
    await until(() => s.up, 'link up');
    ok('feedback, meters and link');

    // a drag: many moves, few messages, the last value wins
    const before = ard.log.filter((a) => a === '/strip/fader').length;
    for (let k = 0; k <= 40; k++) { s.setFader(1, 0.3 + k * 0.01); await sleep(2); }
    await until(() => Math.abs(ard.session.strips[0].fader - 0.7) < 1e-6, 'last fader value');
    const sent = ard.log.filter((a) => a === '/strip/fader').length - before;
    assert(sent < 20, `drag coalesced (${sent} messages for 41 moves)`);
    s.setMute(1, true); s.setPan(3, 0.25); s.setSolo(2, true); s.setRec(1, true);
    await until(() => ard.session.strips[0].mute === 1 && ard.session.strips[2].pan === 0.25 && ard.session.strips[1].solo === 1 && ard.session.strips[0].rec === 1, 'buttons and pan');
    ok(`fader drag (${sent} messages), mute, solo, record, pan`);

    // the console side moves: the page hears it
    ard.session.strips[1].mute = 0;
    t.send('127.0.0.1', ard.port, Osc.encode('/strip/mute', [{ t: 'i', v: 2 }, { t: 'f', v: 0 }]));
    await until(() => s.strips.get(2).mute === false, 'mute feedback');
    ok('changes made in Ardour reach the page');

    // plugins
    let plugins = null;
    s.on('plugins', (ssid, list) => { if (ssid === 1) plugins = list; });
    s.listPlugins(1);
    await until(() => plugins, 'plugin list');
    assert.deepStrictEqual(plugins.map((p) => [p.piid, p.name, p.enabled]), [[1, 'Tilt', true], [2, 'Delay', true], [3, 'ACE Reverb', true]]);
    s.setPluginActive(1, 3, false);
    await until(() => !ard.session.strips[0].plugins[2].enabled, 'deactivate');
    ok('plugin list and bypass');

    const read = async (piid) => {
      let got = null;
      const off = s.on('params', (ssid, p, list) => { if (ssid === 1 && p === piid) got = list; });
      s.readPlugin(1, piid);
      await until(() => got, 'descriptor ' + piid);
      off();
      return got;
    };
    const fxIndex = require('../../web/fx/index.json').effects;
    for (const [piid, id] of [[1, 'tilt'], [2, 'delay']]) {
      const params = await read(piid);
      const e = A.nullEffect(plugins[piid - 1].name, fxIndex);
      assert.strictEqual(e.id, id);
      require(path.join('../../web/fx', id + '.js'));
      const FX = globalThis.NULLFX[id];
      const map = A.mapSliders(FX.sliders, params);
      assert.strictEqual(map.size, FX.sliders.length, `every ${id} slider has a parameter`);
      for (const sl of FX.sliders) assert(!map.get(sl.i).name.includes('latency'));
      // set every slider to 3/4 of its range through the page's mapping
      for (const sl of FX.sliders) {
        const p = map.get(sl.i);
        s.setParam(1, piid, p.par, A.toParam(sl.min + 0.75 * (sl.max - sl.min), sl.min, sl.max, p));
      }
      await sleep(150);
      const sp = ard.session.strips[0].plugins[piid - 1].params;
      for (const sl of FX.sliders) {
        const p = map.get(sl.i);
        const back = A.fromParam(sp[p.par - 1].value, sl.min, sl.max, p);
        assert(Math.abs(back - (sl.min + 0.75 * (sl.max - sl.min))) < 1e-3 * (sl.max - sl.min), `${id} ${sl.name}: ${back}`);
      }
      assert(!ard.log.includes('out of range'), 'no value outside a range');
      ok(`${id}: every slider mapped and set (${piid === 1 ? 'plain' : 'normalized'} range)`);
    }

    // a value changed in Ardour shows up while the plugin is open
    s.watchPlugin(1, 1);
    ard.session.strips[0].plugins[0].params[0].value = -42;
    await until(() => (s.params.get('1/1') || [])[0]?.value === -42, 'plugin re-read');
    s.watchPlugin(null);
    ok('open plugin follows Ardour');

    // generic plugin: scale points
    const rev = await read(3);
    assert.deepStrictEqual(rev[1].points, [[0, 'Hall'], [1, 'Room'], [2, 'Plate']]);
    assert.strictEqual(rev[2].flags & A.FLAG.INPUT, 0);
    ok('other plugins: parameters, scale points, outputs');

    // transport
    s.play();
    await until(() => s.transport.play && !s.transport.stop, 'play');
    await until(() => s.transport.time && s.transport.time !== '00:00:00.000', 'clock');
    s.record(); s.loop();
    await until(() => s.transport.rec && s.transport.loop, 'rec and loop');
    s.stop();
    await until(() => s.transport.stop && !s.transport.play, 'stop');
    ok('transport and clock');

    // Ardour goes away and comes back
    ard.close();
    await until(() => !s.up, 'link down', 6000);
    const again = await sim.start({ port: ard.port });
    await until(() => s.up && again.log.includes('/strip/list'), 'reconnect', 6000);
    again.close();
    ok('link lost and found again');
  } finally {
    s.close();
    try { ard.close(); } catch (e) { /* closed above */ }
  }
}

unit();
live().then(() => { console.log(`ardour_test: ${passed} passed`); process.exit(0); }, (e) => { console.error(e); process.exit(1); });
