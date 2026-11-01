// The Ardour, Bitwig and Ableton tabs of Remote for DAWs in Chromium (Playwright), the way the
// app shows them: the real start page (android/web/core) served under /app/ with web/ as nullfx/
// (as the Gradle build copies it), a stand-in for Shell.kt whose UDP goes to the Ardour simulator
// and whose TCP goes to the software Bitwig (the real controller script) or, on port 8819, to the
// software Live (the real control surface, needs python3), all over the network.
// Usage: node tests/daws/ui_test.cjs [screenshot dir]
// NULL JSFX • https://nulljsfx.tech • MIT License
'use strict';
const assert = require('assert');
const dgram = require('dgram');
const fs = require('fs');
const http = require('http');
const path = require('path');
const { chromium } = require('playwright');
const net = require('net');
const sim = require('./ardour_sim.cjs');
const bitwig = require('./bitwig_host.cjs');
const ableton = require('./ableton_host.cjs');

const root = path.resolve(__dirname, '../..');
const shots = process.argv[2];
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const types = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.json': 'application/json', '.ttf': 'font/ttf' };

function serve() {
  const server = http.createServer((req, res) => {
    const url = decodeURIComponent(req.url.split('?')[0]);
    let file = null;
    if (url.startsWith('/app/core/fonts/')) file = path.join(root, 'native/common/fonts', path.basename(url));
    else if (url.startsWith('/app/nullfx/')) file = path.join(root, 'web', url.slice('/app/nullfx/'.length));
    else if (url.startsWith('/app/')) file = path.join(root, 'android/web', url.slice(5));
    if (!file || !file.startsWith(root) || !fs.existsSync(file) || fs.statSync(file).isDirectory()) { res.writeHead(404); res.end(); return; }
    res.writeHead(200, { 'content-type': types[path.extname(file)] || 'application/octet-stream' });
    fs.createReadStream(file).pipe(res);
  });
  return new Promise((r) => server.listen(0, '127.0.0.1', () => r(server)));
}

const FAKE_NATIVE = `
window.NullShellNative = {
  info: () => JSON.stringify({ app: 'daws', version: '3.0', code: 30000, flavor: 'github', updateCheck: false, sdk: 35 }),
  reaperSet() {}, scan() {},
  udpOpen: () => 1,
  udpSend: (id, h, p, d) => { window.daUdpSend(h, p, d); return true; },
  udpBroadcast: () => true, udpClose() {},
  tcpOpen: (h, p, ms) => { const id = (window.daTcpId = (window.daTcpId || 0) + 1); window.daTcpOpen(id, h, p); return id; },
  tcpSend: (id, d) => { window.daTcpSend(id, d); return true; },
  tcpClose: (id) => { window.daTcpClose(id); },
  keepAwake() {}, fullscreen() {}, haptic() {}, openUrl() {},
};`;

let passed = 0;
const ok = (name) => { passed++; console.log('ok  ' + name); };

async function main() {
  const ard = await sim.start();
  const bw = await bitwig.start();
  const live = await ableton.start();
  const server = await serve();
  const browser = await chromium.launch(process.env.CHROMIUM ? { executablePath: process.env.CHROMIUM } : {});
  const ctx = await browser.newContext({ viewport: { width: 1180, height: 820 }, hasTouch: true });
  const page = await ctx.newPage();
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  page.on('console', (m) => { if (m.type() === 'error') errors.push(m.text()); });

  // the phone's UDP socket: whatever the page sends goes to the simulator, replies come back
  const sock = dgram.createSocket('udp4');
  await new Promise((r) => sock.bind(0, '127.0.0.1', r));
  await page.exposeFunction('daUdpSend', (host, port, b64) => sock.send(Buffer.from(b64, 'base64'), ard.port, '127.0.0.1'));
  sock.on('message', (buf) => {
    page.evaluate((m) => NullShell._recv(m), { type: 'udp', id: 1, host: '127.0.0.1', port: 3819, data: buf.toString('base64') }).catch(() => {});
  });
  // the phone's TCP connections: to the software Live on 8819, else to the software Bitwig
  const tcps = new Map();
  const toPage = (m) => page.evaluate((x) => NullShell._recv(x), m).catch(() => {});
  await page.exposeFunction('daTcpOpen', (id, h, p) => {
    const s = net.connect(p === 8819 ? live.port : bw.port, '127.0.0.1');
    tcps.set(id, s);
    s.on('connect', () => toPage({ type: 'tcpopen', id }));
    s.on('data', (d) => toPage({ type: 'tcp', id, data: d.toString('base64') }));
    s.on('close', () => { tcps.delete(id); toPage({ type: 'tcpclose', id, error: '' }); });
    s.on('error', () => {});
  });
  await page.exposeFunction('daTcpSend', (id, b64) => { const s = tcps.get(id); if (s) s.write(Buffer.from(b64, 'base64')); });
  await page.exposeFunction('daTcpClose', (id) => { const s = tcps.get(id); if (s) s.destroy(); });
  await page.addInitScript(FAKE_NATIVE);

  const shot = async (name, size) => {
    if (!shots) return;
    fs.mkdirSync(shots, { recursive: true });
    if (size) await page.setViewportSize(size);
    await sleep(300);
    await page.screenshot({ path: path.join(shots, name + '.png') });
  };
  async function until(cond, what, ms = 4000) {
    const t0 = Date.now();
    while (!(await cond())) { if (Date.now() - t0 > ms) throw new Error('timed out: ' + what); await sleep(25); }
  }
  async function drag(locator, dx, dy) {
    const b = await locator.boundingBox();
    const x = b.x + b.width / 2, y = b.y + b.height / 2;
    await page.mouse.move(x, y);
    await page.mouse.down();
    for (let k = 1; k <= 10; k++) await page.mouse.move(x + (dx * k) / 10, y + (dy * k) / 10);
    await page.mouse.up();
  }
  const tab = page.locator('[data-tab=ardour]');
  const strip = (name) => tab.locator('.da-strip', { has: page.locator('.da-name', { hasText: name }) }).first();

  try {
    await page.goto(`http://127.0.0.1:${server.address().port}/app/core/index.html`);
    await until(async () => (await page.locator('#tabs button').allTextContents()).includes('Ardour'), 'Ardour tab');
    assert.deepStrictEqual(await page.locator('#tabs button').allTextContents(), ['REAPER', 'Ardour', 'Bitwig', 'Ableton', 'Settings']);
    await page.locator('#tabs button', { hasText: 'Ardour' }).click();
    assert.strictEqual(await tab.locator('.top .name').textContent(), 'NULL JSFXArdour · Mixbus');
    ok('Ardour tab, after REAPER, with the NULL JSFX header');
    await shot('ardour-connect');

    await tab.locator('input').first().fill('127.0.0.1');
    await tab.locator('button.primary').click();
    await until(async () => (await tab.locator('.da-strip').count()) === 4, 'four strips');
    assert.deepStrictEqual(await tab.locator('.da-name').allTextContents(), ['Vocal', 'Guitar', 'Reverb Bus', 'Master']);
    assert(await strip('Guitar').locator('.da-m').evaluate((b) => b.classList.contains('on')));
    assert.strictEqual(await strip('Reverb Bus').locator('.da-r').count(), 0, 'no record button on a bus');
    ok('connects and shows the session');

    // fader: relative drag up raises the level in Ardour
    const before = ard.session.strips[0].fader;
    await drag(strip('Vocal').locator('.da-fader'), 0, -60);
    await until(async () => ard.session.strips[0].fader > before + 0.05, 'fader up in Ardour');
    await strip('Vocal').locator('.da-m').click();
    await until(async () => ard.session.strips[0].mute === 1, 'mute in Ardour');
    ok('fader and mute reach Ardour');
    // Vocal is muted now (and Guitar from the start): the bus meter moves
    await until(async () => (await strip('Reverb Bus').locator('.da-meter-fill').evaluate((e) => parseFloat(e.style.height) || 0)) > 0, 'meters move');
    await page.locator('[data-tab=ardour] .da-play').click();
    await until(async () => page.locator('[data-tab=ardour] .da-play').evaluate((b) => b.classList.contains('on')), 'play lit');
    ok('meters and transport');
    await shot('ardour-desk');
    await shot('ardour-desk-phone', { width: 412, height: 860 });
    await page.setViewportSize({ width: 1180, height: 820 });

    // plugins of the Vocal track
    await strip('Vocal').locator('.da-fx').click();
    await until(async () => (await tab.locator('.da-plugin').count()) === 3, 'plugin list');
    assert.deepStrictEqual(await tab.locator('.da-plugin-open small').allTextContents(), ['NULL JSFX', 'NULL JSFX', 'parameters']);
    ok('plugin list marks the NULL JSFX plugins');
    await shot('ardour-plugins');

    // a NULL plugin opens with its own GFX interface; a drag on it sets the parameter in Ardour
    await tab.locator('.da-plugin-open', { hasText: 'Tilt' }).click();
    await until(async () => (await tab.locator('.da-gfx canvas').count()) === 1, 'Tilt GFX');
    await sleep(400);
    const cv = tab.locator('.da-gfx canvas');
    const lit = await cv.evaluate((c) => {
      const d = c.getContext('2d').getImageData(0, 0, c.width, c.height).data;
      let n = 0;
      for (let k = 0; k < d.length; k += 4) if (d[k] + d[k + 1] + d[k + 2] > 300) n++;
      return n;
    });
    assert(lit > 200, `the Tilt interface is drawn (${lit} light pixels)`);
    const tiltBefore = ard.session.strips[0].plugins[0].params[0].value;
    const box = await cv.boundingBox();
    // the One Slider interface: a horizontal slider across the middle
    await page.mouse.move(box.x + box.width * 0.5, box.y + box.height * 0.875);
    await page.mouse.down();
    for (let k = 1; k <= 12; k++) { await sleep(20); await page.mouse.move(box.x + box.width * (0.5 + 0.03 * k), box.y + box.height * 0.875); }
    await page.mouse.up();
    await until(async () => ard.session.strips[0].plugins[0].params[0].value !== tiltBefore, 'Tilt Amount changed in Ardour');
    ok(`NULL Tilt drawn by its own GFX; a drag sets Amount in Ardour (${tiltBefore} -> ${ard.session.strips[0].plugins[0].params[0].value})`);
    // Ardour changes it: the interface follows
    ard.session.strips[0].plugins[0].params[0].value = -77;
    await until(async () => page.evaluate(() => {
      const v = NullRemote.ardour.view;
      return v && v.gfx && Math.round(v.gfx.fx.inst.getSlider(1)) === -77;
    }), 'GFX follows Ardour');
    ok('the interface follows a change made in Ardour');
    await shot('ardour-tilt');

    // the Delay (normalized range in this host): every slider maps; set Time from the page side
    await tab.locator('.da-back').click();
    await until(async () => (await tab.locator('.da-plugin').count()) === 3, 'back to the list');
    await tab.locator('.da-plugin-open', { hasText: 'Delay' }).click();
    await until(async () => (await tab.locator('.da-gfx canvas').count()) === 1, 'Delay GFX');
    const mapped = await page.evaluate(() => NullRemote.ardour.view.map.size);
    assert.strictEqual(mapped, 15);
    await page.evaluate(() => NullRemote.ardour.view.gfx.onSlider(1, 1500));
    await until(async () => Math.abs(ard.session.strips[0].plugins[1].params[0].value - 0.75) < 1e-6, 'Delay Time 1500 ms = 0.75 normalized');
    ok('NULL Delay: 15 sliders mapped, Time 1500 ms sent as 0.75 of a normalized range');
    await shot('ardour-delay');

    // any other plugin: parameter rows; a tap on an enum steps it
    await tab.locator('.da-back').click();
    await tab.locator('.da-plugin-open', { hasText: 'ACE Reverb' }).click();
    await until(async () => (await tab.locator('.da-param').count()) === 2, 'two input parameters (the output is left out)');
    assert.strictEqual(await tab.locator('.da-param-val').nth(1).textContent(), 'Room');
    await tab.locator('.da-param-bar').nth(1).click();
    await until(async () => ard.session.strips[0].plugins[2].params[1].value === 2, 'Mode stepped to Plate');
    ok('other plugins: parameter list, enum stepping');

    // bypass from the list, then the phone's Back closes the sheet
    await tab.locator('.da-back').click();
    await tab.locator('.da-onoff').nth(2).click();
    await until(async () => !ard.session.strips[0].plugins[2].enabled, 'reverb bypassed');
    assert(await page.evaluate(() => NullRemote.back()));
    assert(await tab.locator('.da-sheet').isHidden());
    ok('bypass and Back');

    // --- Bitwig ---------------------------------------------------------------------------
    const btab = page.locator('[data-tab=bitwig]');
    const bstrip = (name) => btab.locator('.da-strip', { has: page.locator('.da-name', { hasText: name }) }).first();
    await page.locator('#tabs button', { hasText: 'Bitwig' }).click();
    assert.strictEqual(await btab.locator('.top .name').textContent(), 'NULL JSFXBitwig Studio');
    assert.strictEqual(await btab.locator('input').nth(1).inputValue(), '8818');
    await shot('bitwig-connect');
    await btab.locator('input').first().fill('127.0.0.1');
    await btab.locator('button.primary').click();
    await until(async () => (await btab.locator('.da-strip').count()) === 4, 'four Bitwig strips');
    assert.deepStrictEqual(await btab.locator('.da-name').allTextContents(), ['Vocal', 'Synth', 'FX 1', 'Master']);
    assert.deepStrictEqual(await btab.locator('.da-kind').allTextContents(), ['Audio', 'Instrument', 'Effect', 'Master']);
    assert(await bstrip('Master').evaluate((e) => e.classList.contains('da-master')));
    assert(await bstrip('Synth').locator('.da-r').evaluate((b) => b.classList.contains('on')));
    assert.strictEqual(await bstrip('FX 1').locator('.da-r').count(), 0, 'no arm button on an effect track');
    assert.strictEqual(await bstrip('Vocal').locator('.da-db').textContent(), '0.0 dB');
    ok('Bitwig tab: connects over TCP to the controller script and shows the project');

    const vol0 = bw.doc.tracks[0].vol;
    await drag(bstrip('Vocal').locator('.da-fader'), 0, -60);
    await until(async () => bw.doc.tracks[0].vol > vol0 + 0.05, 'Bitwig fader up');
    await until(async () => (await bstrip('Vocal').locator('.da-db').textContent()).startsWith('+'), "Bitwig's level text");
    await bstrip('Vocal').locator('.da-m').click();
    await until(async () => bw.doc.tracks[0].mute === true, 'mute in Bitwig');
    await btab.locator('.da-play').click();
    await until(async () => btab.locator('.da-play').evaluate((b) => b.classList.contains('on')), 'Bitwig playing');
    await until(async () => (await bstrip('Vocal').locator('.da-meter-fill').evaluate((e) => parseFloat(e.style.height) || 0)) > 0, 'Bitwig meters');
    ok('Bitwig: fader, level text, mute, transport, meters');
    await shot('bitwig-desk');
    await shot('bitwig-desk-phone', { width: 412, height: 860 });
    await page.setViewportSize({ width: 1180, height: 820 });

    await bstrip('Vocal').locator('.da-fx').click();
    await until(async () => (await btab.locator('.da-plugin').count()) === 3, 'Bitwig device list');
    assert.deepStrictEqual(await btab.locator('.da-plugin-open small').allTextContents(), ['NULL JSFX', 'NULL JSFX', 'parameters']);
    await btab.locator('.da-plugin-open', { hasText: 'Tilt' }).click();
    await until(async () => (await btab.locator('.da-gfx canvas').count()) === 1, 'Tilt GFX in Bitwig');
    await sleep(400);
    const bcv = btab.locator('.da-gfx canvas');
    const tilt = () => bw.doc.tracks[0].devices[0].params[0].value;
    const t0 = tilt();
    const bb = await bcv.boundingBox();
    await page.mouse.move(bb.x + bb.width * 0.5, bb.y + bb.height * 0.875);
    await page.mouse.down();
    for (let k = 1; k <= 12; k++) { await sleep(20); await page.mouse.move(bb.x + bb.width * (0.5 - 0.03 * k), bb.y + bb.height * 0.875); }
    await page.mouse.up();
    await until(async () => tilt() < t0 - 0.01, 'Tilt Amount down in Bitwig');
    await sleep(700); // past the time a moved control keeps its own value
    bw.change((d) => { d.tracks[0].devices[0].params[0].value = 0.115; }); // -77 on -100..100
    await until(async () => page.evaluate(() => {
      const v = NullRemote.bitwig.view;
      return v && v.gfx && Math.round(v.gfx.fx.inst.getSlider(1)) === -77;
    }), 'Bitwig GFX follows');
    ok(`Bitwig: NULL Tilt in its own GFX, a drag moves it in Bitwig (${t0} -> ${tilt().toFixed(3)}), and it follows Bitwig`);
    await shot('bitwig-tilt');

    await btab.locator('.da-back').click();
    await until(async () => (await btab.locator('.da-plugin').count()) === 3, 'back to the Bitwig list');
    await btab.locator('.da-plugin-open', { hasText: 'Delay' }).click();
    await until(async () => (await btab.locator('.da-gfx canvas').count()) === 1, 'Delay GFX in Bitwig');
    await until(async () => page.evaluate(() => NullRemote.bitwig.view.map && NullRemote.bitwig.view.map.size === 15), 'Delay mapped');
    await page.evaluate(() => NullRemote.bitwig.view.gfx.onSlider(1, 1500));
    await until(async () => Math.abs(bw.doc.tracks[0].devices[1].params[0].value - 0.75) < 1e-6, 'Delay Time 0.75 in Bitwig');
    ok('Bitwig: NULL Delay, 15 sliders mapped by name, Time 1500 ms = 0.75');

    await btab.locator('.da-back').click();
    await btab.locator('.da-plugin-open', { hasText: 'EQ+' }).click();
    await until(async () => (await btab.locator('.da-param').count()) === 2, 'EQ+ rows');
    await until(async () => (await btab.locator('.da-param-val').first().textContent()) === '0.00', "Bitwig's value text");
    const g0 = bw.doc.tracks[0].devices[2].params[0].value;
    await drag(btab.locator('.da-param-bar').first(), 80, 0);
    await until(async () => bw.doc.tracks[0].devices[2].params[0].value > g0 + 0.05, 'EQ+ gain up in Bitwig');
    await btab.locator('.da-back').click();
    await btab.locator('.da-onoff').nth(2).click();
    await until(async () => bw.doc.tracks[0].devices[2].enabled === false, 'EQ+ bypassed in Bitwig');
    assert(await page.evaluate(() => NullRemote.back()));
    assert(await btab.locator('.da-sheet').isHidden());
    ok('Bitwig: other devices as rows with their own value text, bypass, Back');

    // sends: the faders move the send to an effect track
    assert.deepStrictEqual(await btab.locator('.da-mixsel button').allTextContents(), ['Volume', 'Send FX 1']);
    await btab.locator('.da-mixsel button', { hasText: 'Send FX 1' }).click();
    assert(await bstrip('Master').locator('.da-cap').isHidden(), 'the master has no send');
    assert(await bstrip('FX 1').locator('.da-cap').isHidden(), 'nor the effect track');
    const send0 = bw.doc.tracks[0].sends[0];
    const vol1 = bw.doc.tracks[0].vol;
    await drag(bstrip('Vocal').locator('.da-fader'), 0, -60);
    await until(async () => bw.doc.tracks[0].sends[0] > send0 + 0.05, 'Vocal send up in Bitwig');
    assert.strictEqual(bw.doc.tracks[0].vol, vol1, 'the volume stays');
    await until(async () => /dB$/.test(await bstrip('Vocal').locator('.da-db').textContent()), "Bitwig's send text");
    await shot('bitwig-sends');
    await btab.locator('.da-mixsel button', { hasText: 'Volume' }).click();
    assert(await bstrip('Master').locator('.da-cap').isVisible());
    ok('Bitwig: sends on the faders, to the effect track by name; the volume untouched');

    // a container: the devices in its layers show indented under the layer's name
    bw.change((d) => { d.tracks[1].devices = [{ name: 'FX Layer', enabled: true, plugin: false, params: [], layers: [{ name: 'Wet', devices: [{ name: 'NULL Tilt', enabled: true, plugin: true, params: d.tracks[0].devices[0].params.map((p) => ({ ...p })) }] }] }]; });
    await bstrip('Synth').locator('.da-fx').click();
    await until(async () => (await btab.locator('.da-plugin').count()) === 2, 'the container and the device inside');
    assert.deepStrictEqual(await btab.locator('.da-plugin-open small').allTextContents(), ['parameters', 'Wet · NULL JSFX']);
    assert.strictEqual(await btab.locator('.da-plugin').nth(1).evaluate((e) => e.style.paddingLeft), '20px');
    await shot('bitwig-container');
    await btab.locator('.da-plugin-open').nth(1).click();
    await until(async () => (await btab.locator('.da-gfx canvas').count()) === 1, 'Tilt inside the layer in its GFX');
    assert(await page.evaluate(() => NullRemote.back()));
    assert(await page.evaluate(() => NullRemote.back()));
    ok('Bitwig: a device inside a container, indented under its layer, opens in its own GFX');

    // a bigger project: the desk pages through the script's window of 32 tracks
    assert(await btab.locator('.da-pager').isHidden(), 'no pager for 4 tracks');
    bw.change((d) => {
      const master = d.tracks.pop();
      for (let k = d.tracks.length + 1; d.tracks.length < 40; k++) d.tracks.push({ name: 'Track ' + k, type: 'Audio', vol: 0.5, pan: 0.5, mute: false, solo: false, arm: false, color: [0.4, 0.4, 0.4], devices: [] });
      d.tracks.push(master);
    });
    await until(async () => (await btab.locator('.da-strip').count()) === 32, '32 Bitwig strips');
    assert.strictEqual(await btab.locator('.da-page').textContent(), 'Tracks 1–32 of 41');
    assert(await btab.locator('.da-prev').isDisabled());
    await shot('bitwig-pager');
    await btab.locator('.da-next').click();
    await until(async () => (await btab.locator('.da-strip').count()) === 9, 'the second page');
    assert.strictEqual(await btab.locator('.da-page').textContent(), 'Tracks 33–41 of 41');
    assert(await btab.locator('.da-next').isDisabled());
    assert.strictEqual(await btab.locator('.da-name').first().textContent(), 'Track 33');
    await bstrip('Track 34').locator('.da-m').click();
    await until(async () => bw.doc.tracks[33].mute === true, 'mute on the second page');
    await btab.locator('.da-prev').click();
    await until(async () => (await btab.locator('.da-name').first().textContent()) === 'Vocal', 'back to the first page');
    ok('Bitwig: 41 tracks in pages of 32, next and previous, a mute on the second page');

    // --- Ableton Live ----------------------------------------------------------------------
    const atab = page.locator('[data-tab=ableton]');
    const astrip = (name) => atab.locator('.da-strip', { has: page.locator('.da-name', { hasText: name }) }).first();
    await page.locator('#tabs button', { hasText: 'Ableton' }).click();
    assert.strictEqual(await atab.locator('.top .name').textContent(), 'NULL JSFXAbleton Live');
    assert.strictEqual(await atab.locator('input').nth(1).inputValue(), '8819');
    await shot('ableton-connect');
    await atab.locator('input').first().fill('127.0.0.1');
    await atab.locator('button.primary').click();
    await until(async () => (await atab.locator('.da-strip').count()) === 4, 'four Live strips');
    assert.deepStrictEqual(await atab.locator('.da-name').allTextContents(), ['Vocal', 'Synth', 'FX 1', 'Master']);
    assert.deepStrictEqual(await atab.locator('.da-kind').allTextContents(), ['Audio', 'MIDI', 'Return', 'Master']);
    assert(await astrip('Synth').locator('.da-r').evaluate((b) => b.classList.contains('on')));
    assert.strictEqual(await astrip('FX 1').locator('.da-r').count(), 0, 'no arm button on a return track');
    assert.strictEqual(await astrip('Vocal').locator('.da-db').textContent(), '0.0 dB');
    ok('Ableton tab: connects over TCP to the control surface and shows the Set');

    const lv0 = (await live.get()).tracks[0].vol;
    await drag(astrip('Vocal').locator('.da-fader'), 0, -60);
    await until(async () => (await live.get()).tracks[0].vol > lv0 + 0.05, 'Live fader up');
    await until(async () => parseFloat(await astrip('Vocal').locator('.da-db').textContent()) > 0, "Live's level text");
    await astrip('Vocal').locator('.da-m').click();
    await until(async () => (await live.get()).tracks[0].mute === true, 'mute in Live');
    await atab.locator('.da-play').click();
    await until(async () => atab.locator('.da-play').evaluate((b) => b.classList.contains('on')), 'Live playing');
    await until(async () => (await astrip('Vocal').locator('.da-meter-fill').evaluate((e) => parseFloat(e.style.height) || 0)) > 0, 'Live meters');
    ok('Ableton: fader, level text, mute, transport, meters');
    await shot('ableton-desk');

    await astrip('Vocal').locator('.da-fx').click();
    await until(async () => (await atab.locator('.da-plugin').count()) === 3, 'Live device list');
    assert.deepStrictEqual(await atab.locator('.da-plugin-open small').allTextContents(), ['NULL JSFX', 'NULL JSFX', 'parameters']);
    await atab.locator('.da-plugin-open', { hasText: 'Tilt' }).click();
    await until(async () => (await atab.locator('.da-gfx canvas').count()) === 1, 'Tilt GFX in Live');
    await sleep(400);
    const acv = atab.locator('.da-gfx canvas');
    const ltilt = async () => (await live.get()).tracks[0].devices[0].parameters[1].value;
    const lt0 = await ltilt();
    const ab = await acv.boundingBox();
    await page.mouse.move(ab.x + ab.width * 0.5, ab.y + ab.height * 0.875);
    await page.mouse.down();
    for (let k = 1; k <= 12; k++) { await sleep(20); await page.mouse.move(ab.x + ab.width * (0.5 - 0.03 * k), ab.y + ab.height * 0.875); }
    await page.mouse.up();
    await until(async () => (await ltilt()) < lt0 - 0.01, 'Tilt Amount down in Live');
    await sleep(700);
    await live.set(['tracks', 0, 'devices', 0, 'parameters', 1, 'value'], 0.115);
    await until(async () => page.evaluate(() => {
      const v = NullRemote.ableton.view;
      return v && v.gfx && Math.round(v.gfx.fx.inst.getSlider(1)) === -77;
    }), 'Live GFX follows');
    ok('Ableton: NULL Tilt in its own GFX, a drag moves it in Live, and it follows Live');
    await shot('ableton-tilt');
    await atab.locator('.da-back').click();
    await atab.locator('.da-onoff').nth(2).click();
    await until(async () => (await live.get()).tracks[0].devices[2].on === false, 'EQ Eight off in Live');
    assert(await page.evaluate(() => NullRemote.back()));
    assert(await atab.locator('.da-sheet').isHidden());
    ok('Ableton: bypass and Back');

    assert.deepStrictEqual(errors, []);
    ok('no page errors');
  } finally {
    await browser.close();
    server.close();
    sock.close();
    ard.close();
    bw.close();
    live.close();
  }
  console.log(`ui_test (daws): ${passed} passed`);
}

main().catch((e) => { console.error(e); process.exit(1); });
