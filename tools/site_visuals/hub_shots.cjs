// Still screenshots of the NULL JSFX Hub (hub/ui) for the site's /hub/ page: the page runs in
// Chromium with the test stand-in for the Rust side (hub/tests/mock.cjs), like the Hub video
// (tools/video/hub_tour.cjs). Writes <out dir>/hub-<view>.png at 2x.
// Usage: node tools/site_visuals/hub_shots.cjs <out dir>
'use strict';
const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');

const root = path.resolve(__dirname, '../..');
const hub = path.join(root, 'hub');
let chromium;
try { ({ chromium } = require(path.join(hub, 'node_modules/playwright'))); } catch (e) { ({ chromium } = require('playwright')); }
const { mock } = require(path.join(hub, 'tests/mock.cjs'));
const out = path.resolve(process.argv[2] || 'hub-shots');
fs.mkdirSync(out, { recursive: true });

const version = fs.readFileSync(path.join(root, 'index.html'), 'utf8').match(/id="release-version">v([\d.]+)</)[1];
const catalogFile = path.join(out, 'catalog.json');
execFileSync('python3', [path.join(root, 'tools/build_catalog.py'), '--version', version, '--out', catalogFile]);
const catalog = JSON.parse(fs.readFileSync(catalogFile, 'utf8'));
fs.unlinkSync(catalogFile);
catalog.notes = `## New in ${version}\n- New plugins and presets\n- Every plugin in every format\n\nThanks for using NULL JSFX.`;

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1200, height: 750 }, deviceScaleFactor: 2 });
  await page.route('https://nulljsfx.tech/**', (route) => {
    const file = path.join(root, new URL(route.request().url()).pathname);
    if (fs.existsSync(file)) route.fulfill({ path: file });
    else route.abort();
  });
  await page.addInitScript(mock, catalog);
  await page.goto('file://' + path.join(hub, 'ui/index.html'));
  await page.waitForSelector('.card');
  await page.waitForTimeout(1500);
  // the stand-in always offers a Hub update; a screenshot for the site shows the Hub as it usually looks
  await page.addStyleTag({ content: '#hub-update{display:none!important}' });
  const shot = async (name) => { await page.waitForTimeout(700); await page.screenshot({ path: path.join(out, `hub-${name}.png`) }); console.log(name); };
  await shot('plugins');
  await page.click('#nav button[data-view="group:one-slider"]');
  await shot('group');
  await page.click('#nav button[data-view="all"]');
  await page.click('.card[data-id="delay"]', { position: { x: 60, y: 40 } });
  await page.waitForSelector('#detail:not([hidden])');
  await shot('detail');
  await page.keyboard.press('Escape');
  await page.evaluate(() => { const d = document.querySelector('#detail'); if (d && !d.hidden) d.hidden = true; });
  await page.click('#nav button[data-view="extras"]');
  await shot('extras');
  await page.click('#nav button[data-view="settings"]');
  await shot('settings');
  await page.click('#nav button[data-view="news"]');
  await shot('news');
  await browser.close();
})().catch((e) => { console.error(e); process.exit(1); });
