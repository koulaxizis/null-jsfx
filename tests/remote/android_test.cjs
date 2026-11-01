// Tests the pages of the Android remotes (android/web: the REAPER tab of Remote for DAWs, and
// the shell Remote for Mixers shares) in Chromium, the way the app
// shows them: served under /app/ with the REAPER page and the fonts copied in as the Gradle
// build does (android/app/build.gradle.kts, SharedAssets), and /_/ answered by the same
// stand-in for REAPER as remote_test.cjs (mock_reaper.lua running the real bridge under
// fengari). The Kotlin side (android/app) is tested by its unit tests and built in CI.
// Usage: node tests/remote/android_test.cjs [screenshot dir]
'use strict';
const fs = require('fs');
const http = require('http');
const path = require('path');
const { lua, lauxlib, lualib, to_luastring } = require('fengari');
const { chromium } = require('playwright');

const root = path.resolve(__dirname, '../..');
const web = path.join(root, 'android/web');
const page_file = path.join(root, 'remote/reaper_www_root/null_jsfx.html');
const fonts = path.join(root, 'native/common/fonts');
const shots = process.argv[2];

// --- REAPER stand-in ------------------------------------------------------------------------

const L = lauxlib.luaL_newstate();
lualib.luaL_openlibs(L);
function run(code, name) {
  if (lauxlib.luaL_loadbuffer(L, to_luastring(code), null, to_luastring(name || 'chunk')) !== 0 || lua.lua_pcall(L, 0, 0, 0) !== 0) {
    throw new Error(lua.lua_tojsstring(L, -1));
  }
}
function call(fn, arg) {
  lua.lua_getglobal(L, to_luastring(fn));
  if (arg !== undefined) lua.lua_pushstring(L, to_luastring(String(arg)));
  if (lua.lua_pcall(L, arg !== undefined ? 1 : 0, 1, 0) !== 0) throw new Error(lua.lua_tojsstring(L, -1));
  const out = lua.lua_isnil(L, -1) ? null : lua.lua_tojsstring(L, -1);
  lua.lua_pop(L, 1);
  return out;
}
function q(expr) {
  run(`__q = tostring(${expr})`);
  lua.lua_getglobal(L, to_luastring('__q'));
  const v = lua.lua_tojsstring(L, -1);
  lua.lua_pop(L, 1);
  return v;
}
const luaVal = v => (v === null || v === undefined ? 'nil'
  : Array.isArray(v) ? '{' + v.map(luaVal).join(',') + '}'
  : typeof v === 'string' ? JSON.stringify(v) : String(v));
const num = expr => parseFloat(q(expr));

run(fs.readFileSync(path.join(__dirname, 'mock_reaper.lua'), 'utf8'), 'mock_reaper.lua');
const html = fs.readFileSync(page_file, 'utf8');
const NULL_PLUGINS = eval(html.match(/const NULL_PLUGINS = (\[[\s\S]*?\n\]);/)[1]);
const delay = NULL_PLUGINS.find(p => p.s === 'delay');
const js = delay.sl.map(s => [(s[7].includes('h') ? '-' : '') + s[0] + (s[1] ? ` (${s[1]})` : ''), s[5], s[2], s[3] || 1, s[6]]);
js.push(['Wet', 1, 0, 1], ['Bypass', 0, 0, 1]);
run(`catalog_add(${luaVal(['JS:null_jsfx/delay.jsfx'])}, "JS: NULL Delay", "JS: NULL Delay", "null_jsfx/delay.jsfx", ${luaVal(js)})
reaper.TrackFX_AddByName(tracks[1], "JS:null_jsfx/delay.jsfx", false, -1)`);
run(fs.readFileSync(path.join(root, 'remote/Scripts/null_jsfx_remote_bridge.lua'), 'utf8'), 'null_jsfx_remote_bridge.lua');
const ticker = setInterval(() => call('tick'), 30);

// --- the app's assets, as the build lays them out -------------------------------------------

const TYPES = { '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.css': 'text/css; charset=utf-8', '.ttf': 'font/ttf' };
let needAuth = false, down = false;
const server = http.createServer((req, res) => {
  const url = decodeURIComponent(req.url.split('?')[0]);
  if (req.url.startsWith('/_/')) {
    if (down) { // what the app's proxy answers when nothing listens at that address
      res.writeHead(502);
      return res.end();
    }
    if (needAuth && req.headers.authorization !== 'Basic dXNlcjpwYXNz') {
      res.writeHead(401); // as the app's proxy answers: the code only, no browser login prompt
      return res.end();
    }
    res.writeHead(200, { 'Content-Type': 'text/plain; charset=utf-8' });
    return res.end(call('web', req.url.slice(3)));
  }
  let file = null;
  if (url === '/app/daws/reaper/null_jsfx.html') file = page_file;
  else if (url.startsWith('/app/core/fonts/')) file = path.join(fonts, path.basename(url));
  else if (url.startsWith('/app/')) file = path.join(web, url.slice(5));
  if (file && file.startsWith(root) && fs.existsSync(file) && fs.statSync(file).isFile()) {
    res.writeHead(200, { 'Content-Type': TYPES[path.extname(file)] || 'application/octet-stream' });
    return res.end(fs.readFileSync(file));
  }
  res.writeHead(404);
  res.end();
});

// --- checks ---------------------------------------------------------------------------------

let failed = 0;
function check(name, ok, detail) {
  console.log(`${ok ? 'ok  ' : 'FAIL'} ${name}${ok || detail === undefined ? '' : ': ' + detail}`);
  if (!ok) failed++;
}
const wait = ms => new Promise(r => setTimeout(r, ms));
async function until(name, fn, ms = 4000) {
  const end = Date.now() + ms;
  let v;
  while (Date.now() < end) {
    try { v = await fn(); if (v) break; } catch (e) { v = e.message; }
    await wait(50);
  }
  check(name, v === true || (v && typeof v !== 'string'), v);
}
async function dragBy(page, locator, dx, dy) {
  const b = await locator.boundingBox();
  const x = b.x + b.width / 2, y = b.y + b.height / 2;
  await page.mouse.move(x, y);
  await page.mouse.down();
  for (let i = 1; i <= 8; i++) await page.mouse.move(x + (dx * i) / 8, y + (dy * i) / 8);
  await page.mouse.up();
}

// A stand-in for Shell.kt (window.NullShellNative), so shell.js takes its in-app path. It
// records what the pages ask and answers scans through NullShell._recv, as the app's fallback
// path does when the WebView has no message channels.
const FAKE_NATIVE = `
window.__calls = [];
window.NullShellNative = {
  info: () => JSON.stringify({ app: 'daws', version: '3.0', code: 30000, flavor: 'github', updateCheck: true, sdk: 35 }),
  reaperSet: j => __calls.push(['reaperSet', JSON.parse(j)]),
  scan: (id, port) => { __calls.push(['scan', port]); setTimeout(() => NullShell._recv(JSON.stringify({ type: 'scan', id, hosts: [{ host: '127.0.0.1', auth: false }], self: '127.0.0.9' })), 30); },
  udpOpen: p => { __calls.push(['udpOpen', p]); return 7; },
  udpSend: (id, h, p, d) => { __calls.push(['udpSend', id, h, p, d]); return true; },
  udpBroadcast: (id, p, d) => { __calls.push(['udpBroadcast', id, p, d]); return true; },
  udpClose: id => __calls.push(['udpClose', id]),
  tcpOpen: (h, p, ms) => { __calls.push(['tcpOpen', h, p, ms]); return 9; },
  tcpSend: (id, d) => { __calls.push(['tcpSend', id, d]); return true; },
  tcpClose: id => __calls.push(['tcpClose', id]),
  keepAwake: on => __calls.push(['keepAwake', on]),
  fullscreen: on => __calls.push(['fullscreen', on]),
  haptic: () => __calls.push(['haptic']),
  openUrl: u => __calls.push(['openUrl', u]),
};`;

(async () => {
  await new Promise(r => server.listen(0, '127.0.0.1', r));
  const base = `http://127.0.0.1:${server.address().port}`;
  const start = `${base}/app/core/index.html`;
  // CHROMIUM: another Chromium than Playwright's own (e.g. /opt/pw-browsers/chromium)
  const browser = await chromium.launch(process.env.CHROMIUM ? { executablePath: process.env.CHROMIUM } : {});
  const errors = [];

  // --- 1. in a browser (the stub NullShell) ----------------------------------------------
  const ctx = await browser.newContext({ viewport: { width: 1180, height: 820 }, hasTouch: true });
  await ctx.route('https://api.github.com/**', r => r.abort());
  let page = await ctx.newPage();
  page.on('pageerror', e => errors.push(e.message));
  await page.goto(start);
  await until('REAPER, Ardour and Settings tabs', async () => (await page.locator('#tabs button').allTextContents()).join() === 'REAPER,Ardour,Settings');
  check('named Remote for DAWs', (await page.title()) === 'Remote for DAWs');
  check('REAPER tab first', (await page.locator('#tabs button.on').textContent()) === 'REAPER');
  check('NULL JSFX header', (await page.locator('[data-tab=reaper] .top .name').textContent()) === 'NULL JSFXRemote');
  check('no recent hosts yet', await page.locator('[data-saved]').locator('..').isHidden());
  check('no update notice in a browser', await page.locator('#update').isHidden());
  if (shots) await page.screenshot({ path: path.join(shots, 'android-connect.png') });

  await page.fill('[name=host]', '127.0.0.1');
  await page.fill('[name=port]', String(server.address().port));
  await page.click('[data-connect]');
  const frame = page.frameLocator('[data-tab=reaper] iframe');
  await until('the REAPER page opens in the app', async () => (await frame.locator('.strip').count()) === 4);
  await until('bridge detected', async () => (await frame.locator('#conn').textContent()) === 'Connected');
  check('one header: the page\'s own', await page.locator('[data-tab=reaper] > .top').isHidden());
  const vocals = frame.locator('.strip').nth(1);
  await dragBy(page, vocals.locator('.fader'), 0, -80);
  await until('fader through the app raises the volume', () => num('tracks[1].vol') > 1.2);
  await vocals.locator('.fxbtn').click();
  await until('Delay in the chain', async () => (await frame.locator('.fxitem').count()) === 1);
  await frame.locator('.fxitem').first().click();
  await until('Delay controls', async () => (await frame.locator('.plug .knobs .ctl').count()) === 15);
  const fontOk = await page.locator('[data-tab=reaper] iframe').evaluate(f => f.contentDocument.fonts.check('14px "Noto Sans"'));
  check('Noto Sans inside the page', fontOk);
  if (shots) await page.screenshot({ path: path.join(shots, 'android-reaper-delay.png') });
  check('Back closes the FX view', await page.evaluate(() => NullRemote.back()) === true);
  await until('mixer again', async () => await frame.locator('#fxview').isHidden());
  check('Back again leaves REAPER', await page.evaluate(() => NullRemote.back()) === true);
  await until('connect screen again', async () => await page.locator('[data-connect]').isVisible());
  check('remembered in Recent', (await page.locator('[data-saved] button span').allTextContents()).join() === '127.0.0.1');

  // a REAPER with a password
  needAuth = true;
  await page.click('[data-connect]');
  await until('asks for the password', async () => /user name and password/.test(await page.locator('section[data-tab=reaper] .msg').textContent()));
  needAuth = false;

  // nothing answers at that address
  down = true;
  await page.click('[data-connect]');
  await until('says when nothing answers', async () => /No answer from REAPER/.test(await page.locator('section[data-tab=reaper] .msg').textContent()));
  down = false;

  // Recent connects with one tap, and the app reconnects on start
  await page.locator('[data-saved] button').first().click();
  await until('Recent connects', async () => (await frame.locator('.strip').count()) === 4);
  await page.reload();
  await until('reconnects to the last REAPER on start', async () => (await page.frameLocator('[data-tab=reaper] iframe').locator('.strip').count()) === 4);

  // Settings
  await page.click('#tabs button:has-text("Settings")');
  check('Back from another tab goes to the first', await page.evaluate(() => NullRemote.back()) === true && (await page.locator('#tabs button.on').textContent()) === 'REAPER');
  await page.click('#tabs button:has-text("Settings")');
  const awake = page.locator('[data-pref=awake]');
  check('keep awake on by default', await awake.isChecked());
  await awake.uncheck();
  check('setting stored', await page.evaluate(() => NullRemote.prefs.get('awake', true)) === false);
  check('no update check in a browser', await page.locator('text=Check for updates').count() === 0);
  await page.reload();
  check('opens on the last tab', (await page.locator('#tabs button.on').textContent()) === 'Settings');

  // the mixer remote: the same shell, its own tabs, no REAPER
  await page.goto(start + '?app=mixers');
  await until('mixer app tabs', async () => (await page.locator('#tabs button').allTextContents()).join() === 'Mixers,Settings');
  check('no REAPER in the mixer app', await page.locator('section[data-tab=reaper]').count() === 0);
  await ctx.close();

  // --- 2. in the app (a stand-in for Shell.kt) --------------------------------------------
  const app = await browser.newContext({ viewport: { width: 400, height: 860 }, hasTouch: true, isMobile: true });
  await app.addInitScript(FAKE_NATIVE);
  let apiCalls = 0;
  await app.route('https://api.github.com/repos/koulaxizis/null-jsfx/releases/latest', r => {
    apiCalls++;
    r.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({
      tag_name: '3.1', assets: [{ name: 'NULL-JSFX-Remote-Mixers-Android.apk', browser_download_url: 'https://github.com/koulaxizis/null-jsfx/releases/download/3.1/NULL-JSFX-Remote-Mixers-Android.apk' }, { name: 'NULL-JSFX-Remote-DAWs-Android.apk', browser_download_url: 'https://github.com/koulaxizis/null-jsfx/releases/download/3.1/NULL-JSFX-Remote-DAWs-Android.apk' }],
    }) });
  });
  page = await app.newPage();
  page.on('pageerror', e => errors.push(e.message));
  await page.goto(start);
  const calls = () => page.evaluate(() => __calls);
  await until('keeps the screen on at start', async () => (await calls()).some(c => c[0] === 'keepAwake' && c[1] === true));
  await until('update notice', async () => /Remote for DAWs 3\.1 is out \(you have 3\.0\)/.test(await page.locator('#update-text').textContent()));
  await page.click('#update-get');
  await until('Download opens the APK link', async () => (await calls()).some(c => c[0] === 'openUrl' && /3\.1\/NULL-JSFX-Remote-DAWs-Android\.apk$/.test(c[1])));
  await page.click('#update-later');
  check('Later hides it', await page.locator('#update').isHidden());
  await page.reload();
  check('and it stays hidden for that version', await page.locator('#update').isHidden());
  check('checked once, not on every start', apiCalls === 1, apiCalls);

  await page.fill('[name=port]', String(server.address().port));
  await page.click('[data-scan]');
  await until('Find REAPER lists what the phone found', async () => (await page.locator('[data-tab=reaper] .card .list button span').allTextContents()).includes('127.0.0.1'));
  check('scan on the port typed', (await calls()).some(c => c[0] === 'scan' && c[1] === server.address().port));
  if (shots) await page.screenshot({ path: path.join(shots, 'android-found.png') });
  await page.locator('[data-tab=reaper] .card .list button', { hasText: '127.0.0.1' }).first().click();
  await until('connects to the found REAPER', async () => (await page.frameLocator('[data-tab=reaper] iframe').locator('.strip').count()) === 4);
  const set = (await calls()).filter(c => c[0] === 'reaperSet').pop();
  check('tells the phone where REAPER is', set && set[1].host === '127.0.0.1' && set[1].port === server.address().port, JSON.stringify(set));
  if (shots) await page.screenshot({ path: path.join(shots, 'android-reaper-phone.png') });
  await page.frameLocator('[data-tab=reaper] iframe').locator('#t-play').tap();
  await until('play from the phone', () => q('transport.state') === '1');
  check('a tap vibrates', (await calls()).some(c => c[0] === 'haptic'));

  // the UDP bridge that the Mixers tab uses
  const udp = await page.evaluate(() => {
    const got = [];
    NullShell.on('udp', m => got.push(m));
    const id = NullShell.udpOpen(0);
    NullShell.udpBroadcast(id, 10024, 'L3hpbmZvAAAALAAAAA==');
    NullShell._recv(JSON.stringify({ type: 'udp', id, host: '192.168.1.50', port: 10024, data: 'AQI=' }));
    return { id, got };
  });
  check('udpOpen returns the socket id', udp.id === 7);
  check('udpBroadcast reaches the phone', (await calls()).some(c => c[0] === 'udpBroadcast' && c[2] === 10024 && c[3] === 'L3hpbmZvAAAALAAAAA=='));
  const tcp = await page.evaluate(() => {
    const got = [];
    NullShell.on('tcp', m => got.push(m));
    const id = NullShell.tcpOpen('192.168.1.60', 51325);
    NullShell.tcpSend(id, 'AQI=');
    NullShell._recv(JSON.stringify({ type: 'tcp', id, data: 'AwQ=' }));
    return { id, got };
  });
  check('tcpOpen with a default timeout', tcp.id === 9 && (await calls()).some(c => c[0] === 'tcpOpen' && c[2] === 51325 && c[3] === 5000));
  check('tcpSend and incoming TCP data', (await calls()).some(c => c[0] === 'tcpSend' && c[2] === 'AQI=') && tcp.got.length === 1 && tcp.got[0].data === 'AwQ=');
  check('incoming packets reach NullShell.on("udp")', udp.got.length === 1 && udp.got[0].host === '192.168.1.50' && udp.got[0].data === 'AQI=');

  check('no page errors', !errors.length, errors.join('; '));
  check('no bridge errors', q('#console') === '0', q('console[1]'));
  await browser.close();
  clearInterval(ticker);
  server.close();
  console.log(failed ? `${failed} app check(s) failed` : 'android app pages: all checks passed');
  process.exit(failed ? 1 : 0);
})().catch(e => {
  console.error(e);
  process.exit(1);
});
