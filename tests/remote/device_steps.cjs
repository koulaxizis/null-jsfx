// Shared steps of the Android emulator tests (tests/remote/device_test.cjs, job daws-device, and
// tests/mixers/device_test.cjs, job mixers-device).
// - step(name, ms, work): logs the step and fails with its name when it takes longer than ms.
// - install(device, apk): waits for the package manager, then installs with `adb install -r` in
//   a child process with a time limit, three tries. Playwright's own installApk once hung both
//   jobs until their 30 min limit right after the emulator reported booted (Bible Part X,
//   2026-11-02): sys.boot_completed can be 1 before the package manager answers.
// - watchdog(ms): exits with the current step's name if the whole test takes longer.
// NULL JSFX • https://nulljsfx.tech • MIT License
'use strict';
const { execFile } = require('child_process');
const fs = require('fs');
const path = require('path');

// the SDK's adb when ANDROID_HOME names one (as on GitHub's runners), else the one on PATH
const sdk = process.env.ANDROID_HOME || process.env.ANDROID_SDK_ROOT;
const ADB = sdk && fs.existsSync(path.join(sdk, 'platform-tools', 'adb')) ? path.join(sdk, 'platform-tools', 'adb') : 'adb';

let current = 'start';
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

function step(name, ms, work) {
  current = name;
  console.log('step: ' + name);
  let t;
  const late = new Promise((_, reject) => { t = setTimeout(() => reject(new Error('timed out after ' + ms / 1000 + ' s: ' + name)), ms); });
  return Promise.race([Promise.resolve().then(work), late]).finally(() => clearTimeout(t));
}

function adb(serial, args, ms) {
  return new Promise((resolve, reject) => {
    execFile(ADB, ['-s', serial].concat(args), { timeout: ms }, (err, out, errOut) => {
      if (err) reject(new Error('adb ' + args.join(' ') + ': ' + (errOut || err.message).trim()));
      else resolve(String(out));
    });
  });
}

async function install(device, apk) {
  const serial = device.serial();
  await step('wait for the package manager', 120000, async () => {
    for (;;) {
      const out = await adb(serial, ['shell', 'pm', 'path', 'android'], 15000).catch(() => '');
      if (/package:/.test(out)) return;
      await sleep(2000);
    }
  });
  await step('install the APK', 300000, async () => {
    for (let i = 1; ; i++) {
      try {
        const out = await adb(serial, ['install', '-r', '-t', apk], 90000);
        if (/Success/.test(out)) return;
        throw new Error(out.trim());
      } catch (e) {
        console.log('install try ' + i + ' failed: ' + e.message);
        if (i >= 3) throw e;
        await sleep(5000);
      }
    }
  });
}

function watchdog(ms) {
  const t = setTimeout(() => { console.error('device test timed out after ' + ms / 60000 + ' min, in step: ' + current); process.exit(1); }, ms);
  return { done() { clearTimeout(t); }, get step() { return current; } };
}

module.exports = { step, install, watchdog, sleep };
