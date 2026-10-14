// Films a scripted tour of the NULL JSFX Hub's page (hub/ui) for the Hub video: the page runs
// in Chromium with the test stand-in for the Rust side (hub/tests/mock.cjs), a drawn mouse
// pointer moves and clicks, and every repaint is saved with its time. make_hub_video.py turns
// the frames and the chapters into the video.
// Usage: node tools/video/hub_tour.cjs <out dir>     (needs hub/node_modules: cd hub && npm ci)
// Writes <out dir>/f00000.jpg ... and <out dir>/tour.json {frames: [[file, seconds]], chapters}.
'use strict';
const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');

const root = path.resolve(__dirname, '../..');
const hub = path.join(root, 'hub');
const { chromium } = require(path.join(hub, 'node_modules/playwright'));
const { mock } = require(path.join(hub, 'tests/mock.cjs'));
const out = path.resolve(process.argv[2] || 'hub-tour');
fs.mkdirSync(out, { recursive: true });

const catalogFile = path.join(out, 'catalog.json');
execFileSync('python3', [path.join(root, 'tools/build_catalog.py'), '--version', '2.2', '--out', catalogFile]);
const catalog = JSON.parse(fs.readFileSync(catalogFile, 'utf8'));
fs.unlinkSync(catalogFile);
catalog.notes = '## New\n- **NULL JSFX Hub**: install, update and remove every plugin from one place\n- **Tape**, **Disperser** and twelve more new plugins\n- Factory presets in every format\n\nThanks for using NULL JSFX.';
catalog.__applyMs = 1400;

// a drawn mouse pointer: headless Chromium shows none
function pointer() {
  addEventListener('DOMContentLoaded', () => {
    const p = document.createElement('div');
    p.innerHTML = '<svg width="26" height="26" viewBox="0 0 26 26"><path d="M3 2 L3 21 L8 16.5 L11.5 24 L15 22.5 L11.5 15 L18 15 Z" fill="#f2f2f2" stroke="#111" stroke-width="1.5" stroke-linejoin="round"/></svg>';
    p.style.cssText = 'position:fixed;left:0;top:0;z-index:2147483647;pointer-events:none;transform:translate(-3px,-2px);transition:none;filter:drop-shadow(0 2px 3px rgba(0,0,0,.5))';
    const ring = document.createElement('div');
    ring.style.cssText = 'position:fixed;z-index:2147483646;pointer-events:none;width:34px;height:34px;margin:-17px 0 0 -17px;border-radius:50%;border:2px solid rgba(220,220,220,.9);opacity:0';
    document.body.append(ring, p);
    addEventListener('mousemove', (e) => { p.style.left = e.clientX + 'px'; p.style.top = e.clientY + 'px'; }, true);
    addEventListener('mousedown', (e) => {
      ring.style.left = e.clientX + 'px'; ring.style.top = e.clientY + 'px';
      ring.animate([{ opacity: 1, transform: 'scale(.4)' }, { opacity: 0, transform: 'scale(1.4)' }], { duration: 450, easing: 'ease-out' });
    }, true);
  });
}

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1200, height: 750 }, deviceScaleFactor: 1.2 });
  await page.route('https://nulljsfx.tech/**', (route) => {
    const file = path.join(root, new URL(route.request().url()).pathname);
    if (fs.existsSync(file)) route.fulfill({ path: file });
    else route.abort();
  });
  await page.addInitScript(mock, catalog);
  await page.addInitScript(pointer);
  await page.goto('file://' + path.join(hub, 'ui/index.html'));
  await page.waitForSelector('.card');
  await page.mouse.move(600, 380);
  await page.waitForTimeout(800);

  // every repaint, with the time it showed up
  const cdp = await page.context().newCDPSession(page);
  const frames = [];
  const chapters = [];
  let t0 = null;
  cdp.on('Page.screencastFrame', async (f) => {
    const t = f.metadata.timestamp;
    if (t0 === null) t0 = t;
    const file = `f${String(frames.length).padStart(5, '0')}.jpg`;
    fs.writeFileSync(path.join(out, file), Buffer.from(f.data, 'base64'));
    frames.push([file, t - t0]);
    await cdp.send('Page.screencastFrameAck', { sessionId: f.sessionId }).catch(() => {});
  });
  const start = Date.now();
  await cdp.send('Page.startScreencast', { format: 'jpeg', quality: 92, maxWidth: 1440, maxHeight: 900, everyNthFrame: 1 });
  const now = () => (Date.now() - start) / 1000;
  const chapter = (title, text) => chapters.push({ t: now(), title, text });
  const wait = (ms) => page.waitForTimeout(ms);
  // keep the page repainting so a still view still gets frames
  const tick = setInterval(() => page.evaluate(() => { document.body.dataset.tick = Date.now(); }).catch(() => {}), 100);

  const box = async (sel) => (await page.locator(sel).first().boundingBox());
  const moveTo = async (sel, dx = 0.5, dy = 0.5) => {
    let b = await box(sel);
    if (!b || b.y < 60 || b.y + b.height > 750) {
      await page.locator(sel).first().evaluate((e) => e.scrollIntoView({ behavior: 'smooth', block: 'center' }));
      await wait(1100);
      b = await box(sel);
    }
    await page.mouse.move(b.x + b.width * dx, b.y + b.height * dy, { steps: 28 });
    await wait(250);
  };
  const click = async (sel, dx, dy) => { await moveTo(sel, dx, dy); await page.mouse.down(); await page.mouse.up(); await wait(350); };
  const scroll = async (dy, steps = 30) => { for (let i = 0; i < steps; i++) { await page.mouse.wheel(0, dy / steps); await wait(33); } };
  const idle = async () => page.waitForSelector('#progress', { state: 'hidden' });

  chapter('Every plugin, one place', 'All 89 plugins with their interface, the formats each one comes in, and what is installed.');
  await wait(1200);
  await page.mouse.move(700, 450, { steps: 20 });
  await scroll(1400, 80);
  await wait(500);
  await scroll(-1400, 50);
  await wait(600);

  chapter('Find anything', 'Search by name or by what you need: eq, reverb, vocal, drums.');
  await click('#search');
  await page.keyboard.type('reverb', { delay: 140 });
  await wait(1800);
  await page.fill('#search', '');
  await wait(600);

  chapter('One click to install', 'Install puts the plugin in every format picked above the list: JSFX, GFX, VST3 and CLAP.');
  await click('.card[data-id="delay"] .card-foot button');
  await idle();
  await wait(1400);

  chapter('A page for every plugin', 'Its video, its controls and factory presets, a before and after listen, and each format on its own: update, add or remove.');
  await click('.card[data-id="tilt"]', 0.5, 0.3);
  await page.waitForSelector('#detail:not([hidden])');
  await wait(1200);
  await moveTo('#d-formats input[data-f="vst3"]');
  await page.locator('#d-formats input[data-f="vst3"]').check();
  await wait(700);
  await click('#d-apply');
  await idle();
  await wait(800);
  await page.mouse.move(1000, 520, { steps: 20 });
  await scroll(700, 40);
  await wait(1200);
  await page.keyboard.press('Escape');
  await wait(600);

  chapter('Whole groups at once', 'Open a group, choose the formats, and install or update all of it in one go.');
  await click('#nav button[data-view="group:one-slider"]');
  await wait(900);
  await click('#pick button[data-pick="gfx"]');
  await wait(500);
  await click('#pick button[data-pick="gfx"]');
  await wait(500);
  await moveTo('#bulk');
  await wait(900);

  chapter('Extras', 'The Rack for any DAW, the FX chains, the tablet remote and the font: turn each one on or off.');
  await click('#nav button[data-view="extras"]');
  await wait(1200);
  await page.mouse.move(700, 500, { steps: 20 });
  await scroll(500, 30);
  await wait(800);
  await click('.extra[data-id="remote"] button[data-do="add"]');
  await idle();
  await wait(1000);

  chapter('Settings: formats', 'The formats a new install gets, set once.');
  await click('#nav button[data-view="settings"]');
  await wait(1400);
  await click('#set-formats input[value="clap"]');
  await wait(500);
  await click('#set-formats input[value="clap"]');
  await wait(800);
  chapter('Settings: updates', 'It checks for a new release once a day, shows one notification per release, and can start with the computer in the tray.');
  await click('#set-autostart');
  await wait(1600);
  chapter('Settings: folders', 'Where REAPER, VST3 and CLAP plugins go. The usual folders are found for you.');
  await page.mouse.move(700, 450, { steps: 15 });
  await scroll(330, 30);
  await wait(800);
  await moveTo('#set-defaults');
  await wait(1200);
  chapter('Settings: privacy', 'No account, no id, no tracking. It only reads the release list from GitHub.');
  await scroll(260, 30);
  await wait(1400);
  await click('#set-save');
  await wait(900);
  chapter('Settings: uninstall', 'Take everything off the computer in one step, extras and all, after a question first.');
  await scroll(400, 30);
  await wait(800);
  await click('#remove-all');
  await wait(1600);
  await click('#c-cancel');
  await wait(700);

  chapter('What is new', 'The release notes of every new version, and a tag on each new plugin.');
  await click('#nav button[data-view="news"]');
  await wait(2200);
  await click('#nav button[data-view="all"]');
  await page.mouse.move(1150, 60, { steps: 25 });
  await wait(1600);
  chapter('', '');

  clearInterval(tick);
  await cdp.send('Page.stopScreencast');
  await wait(200);
  fs.writeFileSync(path.join(out, 'tour.json'), JSON.stringify({ frames, chapters, end: now() }));
  await browser.close();
  console.log(`${frames.length} frames, ${now().toFixed(1)} s`);
})().catch((e) => {
  console.error(e);
  process.exit(1);
});
