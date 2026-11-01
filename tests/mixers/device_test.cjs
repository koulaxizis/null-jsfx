// Remote for Mixers on a real Android system (an emulator in CI): the debug APK, its Kotlin UDP
// bridge and its WebView, against the X AIR simulator on the host. The emulator reaches the host
// as 10.0.2.2. Playwright drives the app's WebView through adb (debug builds allow it).
//   node tests/mixers/device_test.cjs <app-mixers-github-debug.apk> [screenshot dir]
// Needs adb and one running emulator or device.
'use strict';
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { _android: android } = require('playwright');
const sim = require('./xair_sim.cjs');

const apk = process.argv[2];
const shots = process.argv[3];
const PKG = 'tech.nulljsfx.remote.mixers';
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function main() {
  const console_ = await sim.start({ model: 'XR18', name: 'Emulator XR18', host: '0.0.0.0', port: 10024 });
  const [device] = await android.devices();
  assert.ok(device, 'no Android device or emulator');
  console.log('device', device.model(), device.serial());
  if (apk) await device.installApk(fs.readFileSync(apk));
  await device.shell(`am force-stop ${PKG}`);
  await device.shell(`am start -n ${PKG}/tech.nulljsfx.remote.MainActivity`);
  const webview = await device.webView({ pkg: PKG }, { timeout: 60000 });
  const page = await webview.page();
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  const state = (p) => console_.state.get(p);
  async function until(cond, what, ms = 15000) {
    const t0 = Date.now();
    while (!(await cond())) { if (Date.now() - t0 > ms) throw new Error('timed out: ' + what); await sleep(100); }
  }
  const shot = async (name) => {
    if (!shots) return;
    fs.mkdirSync(shots, { recursive: true });
    fs.writeFileSync(path.join(shots, name + '.png'), await device.screenshot());
  };

  try {
    await page.locator('.nm-connect').waitFor({ timeout: 30000 });
    assert.deepStrictEqual(await page.locator('#tabs button').allTextContents(), ['Mixers', 'Settings']);
    await shot('device-connect');
    // the host is not on the emulator's broadcast domain, so connect by address
    await page.locator('.nm-connect input').fill('10.0.2.2');
    await page.locator('.nm-connect button', { hasText: 'Connect' }).click();
    await page.locator('.nm-desk').waitFor({ timeout: 20000 });
    assert.strictEqual(await page.locator('.nm-strips .nm-strip').count(), 17, '16 channels + aux');
    assert.match(await page.locator('.nm-console').innerText(), /Emulator XR18/);

    // app -> console through the Kotlin UDP socket
    await page.locator('.nm-strips .nm-strip').nth(1).locator('button.mute').click();
    await until(() => state('/ch/02/mix/on') === 0, 'mute reaches the simulator');
    // console -> app, and meters (blobs at 20 Hz)
    console_.change('/ch/03/config/name', 'Snare');
    await until(async () => (await page.locator('.nm-strips .nm-name', { hasText: 'Snare' }).count()) === 1, 'rename from the console');
    await until(async () => (await page.locator('.nm-meter[data-i="0"] i').evaluate((e) => parseFloat(e.style.height) || 0)) > 5, 'meters');
    // subscriptions are renewed: still live after more than 10 s
    await sleep(12000);
    console_.change('/ch/04/config/name', 'Bass');
    await until(async () => (await page.locator('.nm-strips .nm-name', { hasText: 'Bass' }).count()) === 1, 'changes after the first 10 s');
    assert.ok(console_.meterSubscribers() >= 1, 'meter subscription renewed');
    await shot('device-desk');
    assert.deepStrictEqual(errors, []);
    console.log('device test passed');
  } finally {
    await device.close();
    console_.close();
  }
}

main().catch((e) => { console.error(e); process.exit(1); });
