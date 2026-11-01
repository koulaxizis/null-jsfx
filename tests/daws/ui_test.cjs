// The Ardour tab of Remote for DAWs in Chromium (Playwright), the way the app shows it: the real
// start page (android/web/core) served under /app/ with web/ as nullfx/ (as the Gradle build
// copies it), a stand-in for Shell.kt whose UDP goes to the Ardour simulator over real UDP.
// Usage: node tests/daws/ui_test.cjs [screenshot dir]
// NULL JSFX • https://nulljsfx.tech • MIT License
'use strict';
const assert = require('assert');
const dgram = require('dgram');
const fs = require('fs');
const http = require('http');
const path = require('path');
const { chromium } = require('playwright');
const sim = require('./ardour_sim.cjs');

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
  tcpOpen: () => -1, tcpSend: () => false, tcpClose() {},
  keepAwake() {}, fullscreen() {}, haptic() {}, openUrl() {},
};`;

let passed = 0;
const ok = (name) => { passed++; console.log('ok  ' + name); };

async function main() {
  const ard = await sim.start();
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
    assert.deepStrictEqual(await page.locator('#tabs button').allTextContents(), ['REAPER', 'Ardour', 'Settings']);
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
    await until(async () => (await strip('Vocal').locator('.da-meter-fill').evaluate((e) => parseFloat(e.style.height) || 0)) > 0, 'meters move');
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

    assert.deepStrictEqual(errors, []);
    ok('no page errors');
  } finally {
    await browser.close();
    server.close();
    sock.close();
    ard.close();
  }
  console.log(`ui_test (daws): ${passed} passed`);
}

main().catch((e) => { console.error(e); process.exit(1); });
