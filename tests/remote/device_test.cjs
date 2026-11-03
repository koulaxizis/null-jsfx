// Remote for DAWs on a real Android system (an emulator in CI): the debug APK, its Kotlin side
// and its WebView, end to end against stand-ins on the host, which the emulator reaches as
// 10.0.2.2. Each tab uses another path of the shell:
//   REAPER  the /_/ proxy (MainActivity.reaper) to a REAPER stand-in on HTTP 8080
//           (reaper_standin.cjs: mock_reaper.lua running the real bridge script), with Basic
//           auth, and Find REAPER (Shell.scan) on the emulator's network
//   Ardour  UDP (Shell.udpOpen/udpSend) to the Ardour simulator on 3819 (tests/daws)
//   Bitwig  TCP (Shell.tcpOpen/tcpSend) to the software Bitwig on 8818, which runs the real
//           controller script (tests/daws), and a NULL plugin drawn by its own GFX from the
//           nullfx/ assets packed into the APK
// Playwright drives the app's WebView through adb (debug builds allow it), as
// tests/mixers/device_test.cjs does for Remote for Mixers.
//   node tests/remote/device_test.cjs <app-daws-github-debug.apk> [screenshot dir]
// Needs adb and one running emulator or device, and tests/remote's npm packages.
// NULL JSFX • https://nulljsfx.tech • MIT License
'use strict';
const assert = require('assert');
const fs = require('fs');
const http = require('http');
const path = require('path');
const { _android: android } = require('playwright');
const { standin } = require('./reaper_standin.cjs');
const ardourSim = require('../daws/ardour_sim.cjs');
const bitwigHost = require('../daws/bitwig_host.cjs');

const apk = process.argv[2];
const shots = process.argv[3];
const PKG = 'tech.nulljsfx.remote.daws';
const HOST = '10.0.2.2'; // the host, as the emulator sees it
const REAPER_PORT = 8080;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

/** REAPER's web interface on the host: /_/ only, optionally behind Basic auth (user:pass). */
function reaperServer(reaper) {
  const s = { needAuth: false, requests: 0 };
  s.server = http.createServer((req, res) => {
    if (!req.url.startsWith('/_/')) { res.writeHead(404); return res.end(); }
    s.requests++;
    if (s.needAuth && req.headers.authorization !== 'Basic dXNlcjpwYXNz') {
      res.writeHead(401, { 'WWW-Authenticate': 'Basic realm="REAPER"' });
      return res.end();
    }
    res.writeHead(200, { 'Content-Type': 'text/plain; charset=utf-8' });
    res.end(reaper.web(req.url.slice(3)));
  });
  return new Promise((r) => s.server.listen(REAPER_PORT, '0.0.0.0', () => r(s)));
}

const { step, install, open, watchdog, sleep: nap } = require('./device_steps.cjs');
const LIMIT_MS = 12 * 60 * 1000; // the whole test; it passes in about two minutes

let passed = 0;
const ok = (name) => { passed++; console.log('ok  ' + name); };

async function main() {
  const reaper = standin();
  const web = await reaperServer(reaper);
  const ard = await ardourSim.start({ port: 3819 });
  const bw = await bitwigHost.start({ port: 8818 });
  const [device] = await step('find the device', 60000, () => android.devices());
  assert.ok(device, 'no Android device or emulator');
  console.log('device', device.model(), device.serial());
  if (apk) await install(device, apk);
  const page = await open(device, PKG, async () => {
    await device.shell(`am force-stop ${PKG}`);
    await device.shell(`pm clear ${PKG}`); // no saved REAPER from an earlier run
    await device.shell(`am start -n ${PKG}/tech.nulljsfx.remote.MainActivity`);
  });
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  async function until(cond, what, ms = 15000) {
    const t0 = Date.now();
    for (;;) {
      let v = false;
      try { v = await cond(); } catch (e) { v = false; }
      if (v) return;
      if (Date.now() - t0 > ms) throw new Error('timed out: ' + what);
      await sleep(100);
    }
  }
  const shot = async (name) => {
    if (!shots) return;
    fs.mkdirSync(shots, { recursive: true });
    await sleep(300);
    fs.writeFileSync(path.join(shots, name + '.png'), await device.screenshot());
  };
  const tabButton = (name) => page.locator('#tabs button', { hasText: name });

  try {
    // --- REAPER: the /_/ proxy ---------------------------------------------------------------
    const rtab = page.locator('section[data-tab=reaper]');
    await rtab.locator('[data-connect]').waitFor({ timeout: 30000 });
    assert.deepStrictEqual(await page.locator('#tabs button').allTextContents(), ['REAPER', 'Ardour', 'Bitwig', 'Ableton', 'Cubase', 'FL Studio', 'Mackie', 'Settings']);
    ok('Remote for DAWs starts on its REAPER tab');
    await shot('daws-connect');

    // Find REAPER on the emulator's own network. The emulator's router may or may not put the
    // host on that /24, so this checks that the scan runs to its end in Kotlin and says so.
    await rtab.locator('[name=port]').fill(String(REAPER_PORT));
    await rtab.locator('[data-scan]').click();
    await until(async () => !(await rtab.locator('[data-scan]').isDisabled()) && !/Looking for/.test(await rtab.locator('.msg').textContent()), 'the scan ends', 40000);
    const found = await rtab.locator('.card .list button span').allTextContents();
    console.log(`     Find REAPER: "${await rtab.locator('.msg').textContent()}" ${JSON.stringify(found)}`);
    ok('Find REAPER scans the network and answers');

    await rtab.locator('[name=host]').fill(HOST);
    await rtab.locator('[name=port]').fill(String(REAPER_PORT));
    await rtab.locator('[data-connect]').click();
    const frame = page.frameLocator('section[data-tab=reaper] iframe');
    await until(async () => (await frame.locator('.strip').count()) === 4, 'the REAPER page opens through the proxy', 20000);
    await until(async () => (await frame.locator('#conn').textContent()) === 'Connected', 'the bridge answers through the proxy');
    ok('REAPER page through the Kotlin proxy, bridge connected');
    const vocals = frame.locator('.strip').nth(1);
    await vocals.locator('.m').click();
    await until(() => reaper.q('tracks[1].mute') === 'true', 'mute reaches REAPER');
    await frame.locator('#t-play').click();
    await until(() => reaper.q('transport.state') === '1', 'play reaches REAPER');
    ok('mute and play reach REAPER');
    await vocals.locator('.fxbtn').click();
    await until(async () => (await frame.locator('.fxitem').count()) === 1, 'Delay in the chain');
    await frame.locator('.fxitem').first().click();
    await until(async () => (await frame.locator('.plug .knobs .ctl').count()) === 15, 'Delay controls');
    ok('NULL Delay\'s controls through the bridge');
    await shot('daws-reaper-delay');

    // Basic auth: the proxy sends the user name and password the user typed
    assert.strictEqual(await page.evaluate(() => NullRemote.back()), true); // FX view
    assert.strictEqual(await page.evaluate(() => NullRemote.back()), true); // leaves REAPER
    await rtab.locator('[data-connect]').waitFor();
    web.needAuth = true;
    await rtab.locator('[data-connect]').click();
    await until(async () => /user name and password/.test(await rtab.locator('.msg').textContent()), 'asks for the password');
    await rtab.locator('[name=user]').fill('user');
    await rtab.locator('[name=pass]').fill('pass');
    await rtab.locator('[data-connect]').click();
    await until(async () => (await frame.locator('.strip').count()) === 4, 'connects with the password', 20000);
    ok('a REAPER with a password: asks, then connects with Basic auth');
    assert.strictEqual(await page.evaluate(() => NullRemote.back()), true);
    web.needAuth = false;

    // --- Ardour: UDP -------------------------------------------------------------------------
    const atab = page.locator('[data-tab=ardour]');
    const astrip = (name) => atab.locator('.da-strip', { has: page.locator('.da-name', { hasText: name }) }).first();
    await tabButton('Ardour').click();
    await atab.locator('input').first().fill(HOST);
    await atab.locator('button.primary').click();
    await until(async () => (await atab.locator('.da-strip').count()) === 4, 'four Ardour strips', 20000);
    assert.deepStrictEqual(await atab.locator('.da-name').allTextContents(), ['Vocal', 'Guitar', 'Reverb Bus', 'Master']);
    await astrip('Vocal').locator('.da-m').click();
    await until(() => ard.session.strips[0].mute === 1, 'mute reaches Ardour');
    await until(async () => (await astrip('Reverb Bus').locator('.da-meter-fill').evaluate((e) => parseFloat(e.style.height) || 0)) > 0, 'Ardour meters');
    ok('Ardour over the Kotlin UDP socket: session, mute, meters');
    await shot('daws-ardour');

    // --- Bitwig: TCP, and a NULL plugin's own GFX from the APK's nullfx/ assets --------------
    const btab = page.locator('[data-tab=bitwig]');
    const bstrip = (name) => btab.locator('.da-strip', { has: page.locator('.da-name', { hasText: name }) }).first();
    await tabButton('Bitwig').click();
    await btab.locator('input').first().fill(HOST);
    await btab.locator('button.primary').click();
    await until(async () => (await btab.locator('.da-strip').count()) === 4, 'four Bitwig strips', 20000);
    assert.deepStrictEqual(await btab.locator('.da-name').allTextContents(), ['Vocal', 'Synth', 'FX 1', 'Master']);
    await bstrip('Vocal').locator('.da-m').click();
    await until(() => bw.doc.tracks[0].mute === true, 'mute reaches Bitwig');
    ok('Bitwig over the Kotlin TCP socket: project and mute');
    await bstrip('Vocal').locator('.da-fx').click();
    await until(async () => (await btab.locator('.da-plugin').count()) === 3, 'Bitwig device list');
    await btab.locator('.da-plugin-open', { hasText: 'Tilt' }).click();
    await until(async () => (await btab.locator('.da-gfx canvas').count()) === 1, 'Tilt drawn by its GFX', 20000);
    ok('NULL Tilt drawn by its own GFX inside the app');
    await shot('daws-bitwig-tilt');

    assert.deepStrictEqual(errors, []);
    assert.strictEqual(reaper.q('#console'), '0', 'bridge errors: ' + reaper.q('console[1]'));
    console.log(`device test passed (${passed} checks, ${web.requests} REAPER requests through the proxy)`);
  } finally {
    // closing can hang on a stuck adb connection: never let it hide the result
    await Promise.race([device.close(), nap(15000)]);
    reaper.close();
    web.server.close();
    ard.close();
    bw.close();
  }
}

const dog = watchdog(LIMIT_MS);
main().then(() => { dog.done(); process.exit(0); }, (e) => { console.error('failed in step: ' + dog.step); console.error(e); process.exit(1); });
