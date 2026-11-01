// Drives the Mixers tab in a real browser (Playwright, Chromium) against the X AIR simulator and
// an X32/M32 simulator (xair_sim.cjs with profile x32) and a WING simulator (wing_sim.cjs), side by
// side on the network.
// The page's NullShell UDP bridge is wired to real UDP in Node, so the tab's own transport,
// session and UI code all run as in the app. Usage: node tests/mixers/ui_test.cjs [screenshot dir]
'use strict';
const assert = require('assert');
const dgram = require('dgram');
const fs = require('fs');
const http = require('http');
const path = require('path');
const { chromium } = require('playwright');
const sim = require('./xair_sim.cjs');
const wingSim = require('./wing_sim.cjs');

const root = path.resolve(__dirname, '../..');
const shots = process.argv[2];
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

function serve() {
  const types = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css' };
  const server = http.createServer((req, res) => {
    const url = decodeURIComponent(req.url.split('?')[0]);
    const file = url === '/' ? path.join(__dirname, 'harness.html') : path.join(root, url);
    if (!file.startsWith(root) || !fs.existsSync(file)) { res.writeHead(404); res.end(); return; }
    res.writeHead(200, { 'content-type': types[path.extname(file)] || 'application/octet-stream' });
    fs.createReadStream(file).pipe(res);
  });
  return new Promise((r) => server.listen(0, '127.0.0.1', () => r(server)));
}

async function main() {
  const console_ = await sim.start({ model: 'XR18', name: 'Stage XR18' });
  const m32 = await sim.start({ profile: 'x32', model: 'M32', name: 'FOH M32' });
  const wing = await wingSim.start({ model: 'ngc-compact', name: 'Mon WING' });
  const server = await serve();
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1280, height: 800 } });
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  page.on('console', (m) => { if (m.type() === 'error') errors.push(m.text()); });

  // the native bridge: the page's port 10024 is the X AIR simulator, 10023 the M32 simulator and
  // 2223 the WING simulator, which the page sees at 127.0.0.2 and 127.0.0.3 so no two consoles
  // share an address
  const sock = dgram.createSocket('udp4');
  await new Promise((r) => sock.bind(0, '127.0.0.1', r));
  const simPort = (port) => (port === 10023 ? m32.port : port === 2223 ? wing.port : console_.port);
  const toSim = (b64, port) => sock.send(Buffer.from(b64, 'base64'), simPort(port), '127.0.0.1');
  await page.exposeFunction('nmUdpSend', (host, port, b64) => toSim(b64, port));
  await page.exposeFunction('nmUdpBroadcast', (port, b64) => toSim(b64, port));
  // packets from the simulator go to the page: the harness's __nmDeliver, or in the real shell
  // page (android/web/core) the shell's own receive path, as Shell.kt posts them
  let deliverTo = page;
  sock.on('message', (buf, r) => {
    const [host, port] = r.port === m32.port ? ['127.0.0.2', 10023] : r.port === wing.port ? ['127.0.0.3', 2223] : ['127.0.0.1', 10024];
    deliverTo.evaluate((m) => (window.__nmDeliver ? window.__nmDeliver(m) : window.NullShell._recv(JSON.stringify(m))),
      { type: 'udp', id: 1, host, port, data: buf.toString('base64') }).catch(() => {});
  });

  const shot = async (name, size) => {
    if (!shots) return;
    fs.mkdirSync(shots, { recursive: true });
    if (size) await page.setViewportSize(size);
    await sleep(250);
    await page.screenshot({ path: path.join(shots, name + '.png') });
  };
  const state = (p) => console_.state.get(p);
  async function until(cond, what, ms = 3000) {
    const t0 = Date.now();
    while (!(await cond())) { if (Date.now() - t0 > ms) throw new Error('timed out: ' + what); await sleep(20); }
  }
  const strip = (label) => page.locator('.nm-strips .nm-strip', { has: page.locator('.nm-name', { hasText: label }) }).first();
  async function drag(locator, dy) {
    const box = await locator.boundingBox();
    const x = box.x + box.width / 2;
    const y = box.y + box.height / 2;
    await page.mouse.move(x, y);
    await page.mouse.down();
    for (let i = 1; i <= 10; i++) await page.mouse.move(x, y + (dy * i) / 10);
    await page.mouse.up();
  }
  async function hold(locator) {
    await locator.scrollIntoViewIfNeeded();
    const box = await locator.boundingBox();
    await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
    await page.mouse.down();
    await sleep(1700);
    await page.mouse.up();
  }

  try {
    await page.goto(`http://127.0.0.1:${server.address().port}/`);
    // connect view: the scan finds the simulator
    await page.locator('.nm-found button').first().waitFor({ timeout: 5000 });
    assert.match(await page.locator('.nm-found button').first().innerText(), /Stage XR18[\s\S]*XR18/);
    await shot('mixers-connect-phone', { width: 412, height: 860 });
    await page.setViewportSize({ width: 1280, height: 800 });
    await page.locator('.nm-found button').first().click();
    await page.locator('.nm-desk').waitFor({ timeout: 5000 });
    assert.strictEqual(await page.locator('.nm-strips .nm-strip').count(), 17, '16 channels + aux');
    assert.strictEqual(await strip('Kick').count(), 1, 'channel names from the console');

    // mute channel 2 (Vocal)
    await strip('Vocal').locator('button.mute').click();
    await until(() => state('/ch/02/mix/on') === 0, 'mute reaches the console');
    assert.ok(await strip('Vocal').locator('button.mute.on').count());

    // fader drag moves relative to where it was: up by 60 px raises the level
    const before = state('/ch/01/mix/fader');
    await drag(strip('Kick').locator('.nm-fader'), -60);
    await until(() => state('/ch/01/mix/fader') > before + 0.05, 'fader drag');
    assert.match(await strip('Kick').locator('.nm-db').innerText(), /^\+/);

    // a change on the console shows up
    console_.change('/ch/03/config/name', 'Snare');
    await until(async () => (await strip('Snare').count()) === 1, 'remote rename');

    // meters move
    await until(async () => (await page.locator('.nm-meter[data-i="0"] i').evaluate((e) => parseFloat(e.style.height) || 0)) > 5, 'meters');

    // solo and clear solo
    await strip('Kick').locator('button.solo').click();
    await until(() => state('/-stat/solosw/01') === 1, 'solo');
    await page.locator('button.clear-solo').click();
    await until(() => state('/-stat/solosw/01') === 0, 'clear solo');

    // mute group 1 with channel 3 in it
    console_.change('/ch/03/grp/mute', 1);
    await page.locator('button.mg', { hasText: 'Mute 1' }).click();
    await until(() => state('/config/mute/1') === 1, 'mute group');
    await until(async () => (await strip('Snare').locator('button.mute.grp').count()) === 1, 'group mute shown');
    await shot('mixers-desk-tablet', { width: 1280, height: 800 });
    await page.locator('button.mg', { hasText: 'Mute 1' }).click();
    await until(() => state('/config/mute/1') === 0, 'mute group off');

    // sends on faders: Bus 1 (named Drummer on the console)
    await page.locator('.nm-bar button', { hasText: 'Bus 1 Drummer' }).click();
    assert.strictEqual(await page.locator('.nm-strips .nm-strip.sends').count(), 17);
    await drag(strip('Kick').locator('.nm-fader'), -150);
    await until(() => state('/ch/01/mix/01/level') > 0.1, 'send level');
    assert.match(await page.locator('.nm-master .nm-name').innerText(), /Drummer/);
    await page.locator('.nm-bar button', { hasText: 'Main LR' }).click();

    // My mix: the musician sees only the sends to their bus, without mute or solo
    await page.locator('.nm-bar button', { hasText: 'My mix' }).click();
    await page.locator('.nm-sheet button', { hasText: 'Drummer' }).click();
    assert.ok(await page.locator('.nm.mymix').count());
    assert.strictEqual(await page.locator('.nm-strips .nm-strip.sends').count(), 21, 'inputs and FX returns');
    assert.strictEqual(await page.locator('.nm-strips button.mute:visible').count(), 0);
    await shot('mixers-mymix-phone', { width: 412, height: 860 });
    await page.setViewportSize({ width: 1280, height: 800 });
    await hold(page.locator('.nm-bar button', { hasText: 'Leave My mix' }));
    await until(async () => (await page.locator('.nm.mymix').count()) === 0, 'leave My mix');

    // screen lock: controls are covered until the lock button is held
    await hold(page.locator('.nm-bar button', { hasText: 'Lock' }));
    await until(async () => (await page.locator('.nm.locked').count()) === 1, 'lock');
    const muteBefore = state('/ch/05/mix/on');
    await strip('Ch 5').locator('button.mute').click({ force: true });
    await sleep(200);
    assert.strictEqual(state('/ch/05/mix/on'), muteBefore, 'locked screen ignores taps');
    await hold(page.locator('.nm-bar button', { hasText: 'Unlock' }));
    await until(async () => (await page.locator('.nm.locked').count()) === 0, 'unlock');

    // other layers
    await page.locator('.nm-bar button', { hasText: 'Buses' }).click();
    assert.strictEqual(await page.locator('.nm-strips .nm-strip').count(), 10, '6 buses + 4 FX sends');
    await page.locator('.nm-bar button', { hasText: 'DCA' }).click();
    assert.strictEqual(await page.locator('.nm-strips .nm-strip').count(), 4);
    await page.locator('.nm-bar button', { hasText: 'Inputs' }).click();

    await page.locator('.nm-bar button', { hasText: 'Disconnect' }).click();
    await page.locator('.nm-connect').waitFor();

    // the M32 in the same scan: its own layout, paths and meters
    await page.locator('.nm-found button', { hasText: 'FOH M32' }).waitFor({ timeout: 5000 });
    assert.match(await page.locator('.nm-found button', { hasText: 'FOH M32' }).innerText(), /M32 • 127\.0\.0\.2/);
    await page.locator('.nm-found button', { hasText: 'FOH M32' }).click();
    await page.locator('.nm-desk').waitFor({ timeout: 5000 });
    assert.match(await page.locator('.nm-console').innerText(), /FOH M32 • M32/);
    assert.strictEqual(await page.locator('.nm-strips .nm-strip').count(), 40, '32 channels + 8 aux ins');
    assert.strictEqual(await page.locator('button.mg').count(), 6, 'six mute groups');
    await strip('Ch 32').locator('button.mute').click();
    await until(() => m32.state.get('/ch/32/mix/on') === 0, 'mute channel 32 on the M32');
    await strip('Aux 1').locator('button.solo').click();
    await until(() => m32.state.get('/-stat/solosw/33') === 1, 'solo aux 1 on the M32');
    await page.locator('button.clear-solo').click();
    await until(() => m32.state.get('/-stat/solosw/33') === 0, 'clear solo on the M32');
    await until(async () => (await page.locator('.nm-meter[data-i="0"] i').evaluate((e) => parseFloat(e.style.height) || 0)) > 5, 'M32 meters (/meters/0)');
    await until(async () => (await page.locator('.nm-master .nm-meter[data-i="92"] i').evaluate((e) => parseFloat(e.style.height) || 0)) >= 0, 'Main LR meter slot');
    await page.locator('.nm-bar button', { hasText: 'Bus 16' }).click();
    await drag(strip('Kick').locator('.nm-fader'), -150);
    await until(() => m32.state.get('/ch/01/mix/16/level') > 0.1, 'send to bus 16 on the M32');
    await page.locator('.nm-bar button', { hasText: 'Main LR' }).click();
    await page.locator('.nm-bar button', { hasText: 'Buses' }).click();
    assert.strictEqual(await page.locator('.nm-strips .nm-strip').count(), 16);
    await page.locator('.nm-bar button', { hasText: 'DCA' }).click();
    assert.strictEqual(await page.locator('.nm-strips .nm-strip').count(), 8);
    await page.locator('.nm-strips .nm-strip').nth(7).locator('button.mute').click();
    await until(() => m32.state.get('/dca/8/on') === 0, 'DCA 8 mute on the M32');
    await page.locator('.nm-bar button', { hasText: 'Inputs' }).click();
    await shot('mixers-m32-tablet', { width: 1280, height: 800 });
    await page.setViewportSize({ width: 1280, height: 800 });
    await page.locator('.nm-bar button', { hasText: 'Disconnect' }).click();
    await page.locator('.nm-connect').waitFor();

    // the WING in the same scan: dB on the wire, no FX Returns layer, solo cleared strip by strip
    const wingBtn = page.locator('.nm-found button', { hasText: 'Mon WING' });
    await wingBtn.waitFor({ timeout: 5000 });
    assert.match(await wingBtn.innerText(), /WING Compact • 127\.0\.0\.3/);
    await wingBtn.click();
    await page.locator('.nm-desk').waitFor({ timeout: 5000 });
    assert.match(await page.locator('.nm-console').innerText(), /Mon WING • WING Compact/);
    assert.strictEqual(await page.locator('.nm-strips .nm-strip').count(), 48, '40 channels + 8 aux');
    assert.strictEqual(await page.locator('.nm-bar button', { hasText: 'FX Returns' }).count(), 0, 'no FX Returns layer');
    assert.strictEqual(await page.locator('button.mg').count(), 8, 'eight mute groups');
    assert.match(await strip('Kick').locator('.nm-db').innerText(), /^0\.0/, 'Kick at 0 dB');
    await strip('Ch 40').locator('button.mute').click();
    await until(() => wing.state.get('/ch/40/mute') === 1, 'mute channel 40 on the WING (1 = muted)');
    await strip('Vocal').locator('button.solo').click();
    await strip('Aux 2').locator('button.solo').click();
    await until(() => wing.state.get('/ch/2/$solo') === 1 && wing.state.get('/aux/2/$solo') === 1, 'solo on the WING');
    await page.locator('button.clear-solo').click();
    await until(() => wing.state.get('/ch/2/$solo') === 0 && wing.state.get('/aux/2/$solo') === 0, 'clear solo, strip by strip');
    await strip('Kick').scrollIntoViewIfNeeded(); // the Aux 2 click scrolled the strips
    await drag(strip('Kick').locator('.nm-fader'), 80);
    await until(() => wing.state.get('/ch/1/fdr') < -1, 'Kick fader down, in dB on the WING');
    const kickDb = wing.state.get('/ch/1/fdr') * 10;
    assert.ok(Math.abs(kickDb - Math.round(kickDb)) < 1e-4, 'whole tenths of a dB (as float32): ' + kickDb);
    wing.change('/ch/3/fdr', -20);
    wing.change('/ch/3/name', 'Gtr');
    await until(async () => (await strip('Gtr').locator('.nm-db').innerText()).startsWith('-20'), 'a change on the WING shows');
    await page.locator('.nm-bar button', { hasText: 'Bus 1 Drummer' }).click();
    await drag(strip('Kick').locator('.nm-fader'), -150);
    await until(() => wing.state.get('/ch/1/send/1/lvl') > -60, 'send to bus 1 on the WING');
    await page.locator('.nm-bar button', { hasText: 'Main 1' }).click();
    await page.locator('.nm-bar button', { hasText: 'DCA' }).click();
    assert.strictEqual(await page.locator('.nm-strips .nm-strip').count(), 16);
    await page.locator('.nm-bar button', { hasText: 'Inputs' }).click();
    await shot('mixers-wing-tablet', { width: 1280, height: 800 });
    await page.setViewportSize({ width: 1280, height: 800 });
    await page.locator('.nm-bar button', { hasText: 'Disconnect' }).click();
    await page.locator('.nm-connect').waitFor();

    // inside the real shell page (android/web/core/index.html) of the mixers app, with a
    // stand-in for Shell.kt's NullShellNative: header, tab bar, and the same session
    const app = await browser.newContext({ viewport: { width: 412, height: 860 }, hasTouch: true, isMobile: true });
    await app.exposeBinding('nmUdpSend', (src, host, port, b64) => toSim(b64, port));
    await app.addInitScript(() => {
      window.NullShellNative = {
        info: () => JSON.stringify({ app: 'mixers', version: '3.0', code: 30000, flavor: 'github', updateCheck: false, sdk: 35 }),
        reaperSet() {}, scan() {},
        udpOpen: () => 1,
        udpSend: (id, h, p, d) => { window.nmUdpSend(h, p, d); return true; },
        udpBroadcast: (id, p, d) => { window.nmUdpSend('255.255.255.255', p, d); return true; },
        udpClose() {}, tcpOpen: () => -1, tcpSend: () => false, tcpClose() {},
        keepAwake() {}, fullscreen() {}, haptic() {}, openUrl() {},
      };
    });
    const shellPage = await app.newPage();
    shellPage.on('pageerror', (e) => errors.push(e.message));
    deliverTo = shellPage;
    await shellPage.goto(`http://127.0.0.1:${server.address().port}/android/web/core/index.html`);
    await shellPage.locator('#tabs button', { hasText: 'Mixers' }).waitFor();
    assert.deepStrictEqual(await shellPage.locator('#tabs button').allTextContents(), ['Mixers', 'Settings']);
    assert.strictEqual(await shellPage.locator('section[data-tab=mixers] header.top').count(), 1, 'NULL JSFX header');
    await shellPage.locator('.nm-found button').first().waitFor({ timeout: 5000 });
    await shellPage.locator('.nm-found button').first().click();
    await shellPage.locator('.nm-desk').waitFor({ timeout: 5000 });
    assert.strictEqual(await shellPage.locator('.nm-strips .nm-strip').count(), 17);
    await shellPage.locator('.nm-strips .nm-strip').nth(3).locator('button.mute').click();
    await until(() => state('/ch/04/mix/on') === 0, 'mute from the shell page');
    await shellPage.locator('.nm-bar button', { hasText: 'My mix' }).click();
    assert.strictEqual(await shellPage.evaluate(() => NullRemote.back()), true, 'Back closes the My mix picker');
    assert.strictEqual(await shellPage.locator('.nm-sheet').count(), 0);
    if (shots) { fs.mkdirSync(shots, { recursive: true }); await shellPage.screenshot({ path: path.join(shots, 'mixers-app-phone.png') }); }
    await app.close();

    assert.deepStrictEqual(errors, []);
    console.log('ui test passed');
  } finally {
    await browser.close();
    server.close();
    sock.close();
    console_.close();
    m32.close();
    wing.close();
  }
}

main().catch((e) => { console.error(e); process.exit(1); });
