// Tests the tablet remote end to end: the page (remote/reaper_www_root/null_jsfx.html) in a
// real browser, talking to a stand-in for REAPER's web interface (mock_reaper.lua) that runs
// the real bridge script (remote/Scripts/null_jsfx_remote_bridge.lua) under fengari (Lua in
// JavaScript). Usage: node tests/remote/remote_test.cjs [screenshot dir]
'use strict';
const fs = require('fs');
const http = require('http');
const path = require('path');
const { lua, lauxlib, lualib, to_luastring } = require('fengari');
const { chromium } = require('playwright');

const root = path.resolve(__dirname, '../..');
const page_file = path.join(root, 'remote/reaper_www_root/null_jsfx.html');
const bridge_file = path.join(root, 'remote/Scripts/null_jsfx_remote_bridge.lua');
const shots = process.argv[2];

// --- Lua ------------------------------------------------------------------------------------

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

run(fs.readFileSync(path.join(__dirname, 'mock_reaper.lua'), 'utf8'), 'mock_reaper.lua');

// the catalog: every NULL plugin in every format, as REAPER would show it
const html = fs.readFileSync(page_file, 'utf8');
const NULL_PLUGINS = eval(html.match(/const NULL_PLUGINS = (\[[\s\S]*?\n\]);/)[1]);
for (const p of NULL_PLUGINS) {
  const js = p.sl.map(s => [(s[7].includes('h') ? '-' : '') + s[0] + (s[1] ? ` (${s[1]})` : ''), s[5], s[2], s[3] || 1, s[6]]);
  js.push(['Wet', 1, 0, 1], ['Bypass', 0, 0, 1]);
  const gfx = js.map(x => ['-' + x[0].replace(/^-/, ''), ...x.slice(1)]);
  const nat = p.sl.filter(s => !s[7].includes('f')).map(s => [s[0], (s[5] - s[2]) / (s[3] - s[2]), 0, 1]);
  nat.push(['Bypass', 0, 0, 1]);
  const nn = p.nn || p.n;
  run(`catalog_add(${luaVal([`JS:null_jsfx/${p.s}.jsfx`, `JS:NULL ${p.n}`])}, ${luaVal(`JS: NULL ${p.n}`)}, ${luaVal(`JS: NULL ${p.n}`)}, ${luaVal(`null_jsfx/${p.s}.jsfx`)}, ${luaVal(js)})`);
  run(`catalog_add(${luaVal([`JS:null_jsfx/gfx/${p.s}_gfx.jsfx`])}, ${luaVal(`JS: NULL ${p.n} GFX`)}, ${luaVal(`JS: NULL ${p.n} GFX`)}, ${luaVal(`C:\\REAPER\\Effects\\null_jsfx\\gfx\\${p.s}_gfx.jsfx`)}, ${luaVal(gfx)})`);
  run(`catalog_add(${luaVal([`VST3:${nn} (NULL JSFX)`])}, ${luaVal(`VST3: ${nn} (NULL JSFX)`)}, ${luaVal(`VST3: ${nn} (NULL JSFX)`)}, ${luaVal(`C:\\Program Files\\Common Files\\VST3\\NULL JSFX\\${nn}.vst3`)}, ${luaVal(nat)})`);
  run(`catalog_add(${luaVal([`CLAP:${nn} (NULL JSFX)`])}, ${luaVal(`CLAP: ${nn} (NULL JSFX)`)}, ${luaVal(`CLAP: ${nn} (NULL JSFX)`)}, ${luaVal(`tech.nulljsfx.${p.s}`)}, ${luaVal(nat)})`);
}
run(`catalog_add({"VST:ReaEQ (Cockos)"}, "VST: ReaEQ (Cockos)", "VST: ReaEQ (Cockos)", "reaeq.dll",
  {{"Freq-Low Shelf", 0.2, 0, 1}, {"Gain-Low Shelf", 0.5, 0, 1}, {"Q-Low Shelf", 0.3, 0, 1}})
reaper.TrackFX_AddByName(tracks[1], "JS:null_jsfx/delay.jsfx", false, -1)
reaper.TrackFX_AddByName(tracks[1], "VST:ReaEQ (Cockos)", false, -1)
reaper.TrackFX_AddByName(tracks[2], "VST3:Tilt (NULL JSFX)", false, -1)
tracks[1].peak = -120
tracks[1].sel = true`);

// the bridge, as REAPER runs it
run(fs.readFileSync(bridge_file, 'utf8'), 'null_jsfx_remote_bridge.lua');
const ticker = setInterval(() => call('tick'), 30);

// --- web server -----------------------------------------------------------------------------

const server = http.createServer((req, res) => {
  if (req.url.startsWith('/_/')) {
    res.writeHead(200, { 'Content-Type': 'text/plain; charset=utf-8' });
    res.end(call('web', req.url.slice(3)));
  } else if (req.url.startsWith('/null_jsfx.html')) {
    res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
    res.end(fs.readFileSync(page_file));
  } else {
    res.writeHead(404);
    res.end();
  }
});

// --- checks ---------------------------------------------------------------------------------

let failed = 0;
function check(name, ok, detail) {
  console.log(`${ok ? 'ok  ' : 'FAIL'} ${name}${ok || detail === undefined ? '' : ': ' + detail}`);
  if (!ok) failed++;
}
const wait = ms => new Promise(r => setTimeout(r, ms));
async function until(name, fn, ms = 3000) {
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
const num = expr => parseFloat(q(expr));

(async () => {
  await new Promise(r => server.listen(0, '127.0.0.1', r));
  const url = `http://127.0.0.1:${server.address().port}/null_jsfx.html`;
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1280, height: 800 } });
  const errors = [];
  page.on('pageerror', e => errors.push(e.message));
  await page.goto(url);

  // mixer
  await until('mixer shows master and 3 tracks', async () => (await page.locator('.strip').count()) === 4);
  await until('bridge detected', async () => await page.locator('#banner').isHidden() && (await page.textContent('#conn')) === 'Connected');
  check('track names', (await page.locator('.strip .tname').allTextContents()).join() === 'MASTER,Vocals,Bass,Drums');
  if (shots) await page.screenshot({ path: path.join(shots, 'remote-mixer.png') });

  const vocals = page.locator('.strip').nth(1);
  await dragBy(page, vocals.locator('.fader'), 0, -80);
  await until('fader raises the volume', () => num('tracks[1].vol') > 1.2);
  await vocals.locator('.fader').dblclick();
  await until('double tap resets to 0 dB', () => Math.abs(num('tracks[1].vol') - 1) < 0.01);
  await dragBy(page, vocals.locator('.pan'), -60, 0);
  await until('pan moves left', () => num('tracks[1].pan') < -0.5);
  await page.locator('.strip').nth(2).locator('.m').click();
  await until('mute', () => q('tracks[2].mute') === 'true');
  await until('mute shows', async () => (await page.locator('.strip').nth(2).locator('.m').getAttribute('class')).includes('on'));
  await page.locator('.strip').nth(3).locator('.r').click();
  await until('record arm', () => q('tracks[3].arm') === 'true');
  await page.click('#t-play');
  await until('play', () => q('transport.state') === '1');
  await page.click('#t-stop');
  await until('stop', () => q('transport.state') === '0');

  // FX: a NULL JSFX (Delay) and a third-party plugin on Vocals
  await vocals.locator('.fxbtn').click();
  await until('chain lists the FX', async () => (await page.locator('.fxitem').count()) === 2);
  const items = await page.locator('.fxitem .nm').allTextContents();
  check('Delay recognised as NULL JSFX', items[0] === 'DelayNULL JSFX · Essentials · JSFX', items[0]);
  check('ReaEQ shown as third-party', items[1] === 'ReaEQ (Cockos)Third-party', items[1]);
  await page.locator('.fxitem').nth(0).click();
  await until('Delay controls', async () => (await page.locator('.plug .knobs .ctl').count()) === 13);
  check('Delay header', (await page.textContent('.plug .pname')) === 'DELAY');
  const labels = (await page.locator('.plug .knobs .ctl .lbl').allTextContents()).join('|');
  check('knob labels', labels === 'Time (ms)|Feedback|Mix|Tempo Sync|Subdivision|Mode|Low Cut (Hz)|High Cut (Hz)|Drive|Modulation|Diffusion|Ducking|Freeze', labels);
  check('time value', (await page.locator('.plug .knobs .ctl .val').first().textContent()) === '250 ms');
  await dragBy(page, page.locator('.plug .knob').first(), 0, -100);
  await until('knob sets Time in ms', () => num('tracks[1].fx[1].params[1].value') > 1000);
  const time = num('tracks[1].fx[1].params[1].value');
  check('Time snapped to its 1 ms step', time === Math.round(time), time);
  await page.locator('.plug .seg button', { hasText: /^ON$/ }).click();
  await until('Tempo Sync ON', () => num('tracks[1].fx[1].params[4].value') === 1);
  await page.locator('.plug select').first().selectOption({ label: '1/16' }); // 12 notes: a list, not buttons
  await until('Subdivision 1/16', () => num('tracks[1].fx[1].params[5].value') === 2);
  await page.locator('.plug .knob').nth(1).dblclick();
  await until('double tap resets Feedback to its default', () => num('tracks[1].fx[1].params[2].value') === 0);
  if (shots) await page.screenshot({ path: path.join(shots, 'remote-delay.png') });
  await page.locator('.fxitem .pwr').first().click();
  await until('bypass', () => q('tracks[1].fx[1].enabled') === 'false');
  // the power button toggles from the last polled state, so wait for the list to show the bypass
  await until('bypass shows', async () => !(await page.locator('.fxitem .pwr').first().getAttribute('class')).includes('on'));
  await page.locator('.fxitem .pwr').first().click();
  await until('enable', () => q('tracks[1].fx[1].enabled') === 'true');
  await page.click('text=Show on PC');
  await until('show on the computer', () => q('#shown') === '1');

  await page.locator('.fxitem').nth(1).click();
  await until('ReaEQ parameters', async () => (await page.locator('.prow').count()) === 3);
  await dragBy(page, page.locator('.prow .hs').nth(1), 120, 0);
  await until('generic slider', () => num('tracks[1].fx[2].params[2].value') > 0.6);

  // a VST3 NULL plugin (One Slider) on Bass
  await page.click('#fx-back');
  await page.locator('.strip').nth(2).locator('.fxbtn').click();
  await until('Bass chain', async () => (await page.locator('.fxitem').count()) === 1);
  check('VST3 Tilt recognised', (await page.textContent('.fxitem .nm small')) === 'NULL JSFX · One Slider · VST3');
  await page.locator('.fxitem').first().click();
  await until('Tilt slider', async () => (await page.locator('.plug .ctl.one .hs').count()) === 1);
  const hs = page.locator('.plug .ctl.one .hs');
  const w = (await hs.boundingBox()).width;
  await dragBy(page, hs, w / 4, 0);
  await until('VST3 slider sets the 0..1 value', () => Math.abs(num('tracks[2].fx[1].params[1].value') - 0.75) < 0.02);
  await until('Tilt shows +50', async () => (await page.textContent('.plug .ctl.one .val')) === '+50');
  if (shots) await page.screenshot({ path: path.join(shots, 'remote-tilt.png') });

  // add, move and remove
  await page.click('#fx-add');
  await page.locator('#add-format button', { hasText: 'GFX' }).click();
  await page.fill('#add-search', 'gate');
  if (shots) await page.screenshot({ path: path.join(shots, 'remote-add.png') });
  await page.locator('#addlist button', { hasText: /^Gate$/ }).click();
  await until('Gate GFX added', () => q('#tracks[2].fx') === '2' && q('tracks[2].fx[2].name') === 'JS: NULL Gate GFX');
  await until('new FX opens', async () => (await page.textContent('.plug .pname')) === 'GATE');
  await wait(800);
  check('added once', q('#tracks[2].fx') === '2', q('#tracks[2].fx'));
  await page.click('.plug .tools button:has-text("◀")');
  await until('moved up', () => q('tracks[2].fx[1].name') === 'JS: NULL Gate GFX');
  await wait(500);
  check('moved once', q('tracks[2].fx[1].name') === 'JS: NULL Gate GFX');
  await page.click('.plug .tools button:has-text("Remove")');
  await wait(300);
  check('first tap does not remove', q('#tracks[2].fx') === '2');
  await page.click('.plug .tools button:has-text("Tap again")');
  await until('removed', () => q('#tracks[2].fx') === '1');
  await wait(500);
  check('removed once', q('#tracks[2].fx') === '1' && q('tracks[2].fx[1].name') === 'VST3: Tilt (NULL JSFX)');

  // every NULL plugin in every format is recognised and gets a control for each visible slider
  run(`tracks[3].fx = {}`);
  const names = [];
  for (const p of NULL_PLUGINS) for (const f of p.f) {
    const nn = p.nn || p.n;
    names.push({ jsfx: `JS:null_jsfx/${p.s}.jsfx`, gfx: `JS:null_jsfx/gfx/${p.s}_gfx.jsfx`, vst3: `VST3:${nn} (NULL JSFX)`, clap: `CLAP:${nn} (NULL JSFX)` }[f]);
  }
  for (const n of names) run(`reaper.TrackFX_AddByName(tracks[3], ${luaVal(n)}, false, -1)`);
  await page.click('#fx-back');
  await page.locator('.strip').nth(3).locator('.fxbtn').click();
  await until('all formats listed', async () => (await page.locator('.fxitem').count()) === names.length, 5000);
  const metas = await page.locator('.fxitem .nm small').allTextContents();
  check('every one recognised', metas.every(m => m.startsWith('NULL JSFX')), metas.find(m => !m.startsWith('NULL JSFX')));
  let bad = [];
  for (let i = 0; i < names.length; i += 7) { // a sample across plugins and formats
    await page.locator('.fxitem').nth(i).click();
    const p = NULL_PLUGINS.find(x => names[i].includes(`/${x.s}.jsfx`) || names[i].includes(`/${x.s}_gfx.jsfx`) || names[i].includes(`:${x.nn || x.n} (`));
    const want = p.sl.filter(s => !s[7].includes('h') && !(s[7].includes('f') && /VST3|CLAP/.test(names[i]))).length;
    try {
      await page.waitForFunction(n => document.querySelectorAll('.plug .knobs .ctl').length === n, want, { timeout: 3000 });
    } catch (e) {
      bad.push(`${names[i]}: ${await page.locator('.plug .knobs .ctl').count()} of ${want}`);
    }
  }
  check('controls for every slider', !bad.length, bad.join('; '));

  check('no page errors', !errors.length, errors.join('; '));
  check('no bridge errors', q('#console') === '0', q('console[1]'));
  await browser.close();
  clearInterval(ticker);
  server.close();
  console.log(failed ? `${failed} remote check(s) failed` : 'remote: all checks passed');
  process.exit(failed ? 1 : 0);
})().catch(e => {
  console.error(e);
  process.exit(1);
});
