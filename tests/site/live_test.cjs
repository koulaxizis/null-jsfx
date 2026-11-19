// Tests the live player of the plugin pages (assets/js/live.js) in a real browser: the page is
// served from the repo, a loop plays through the plugin's web version, and the sound that reaches
// the speakers changes with the preset and with bypass. Also reads every plugin page's player data,
// and does the same for the FX chain pages (chains/<slug>/): their chain data, their pictures, the
// chain window and a whole chain playing live. Last, the product pages (rack/, hub/, remotes/) and the
// homepage's "One plugin, every form": every file they show is there, and their silent loops start only when they come on screen.
// Usage: node tests/site/live_test.cjs   (Playwright from tests/remote: npm ci there first)
'use strict';
const fs = require('fs');
const http = require('http');
const path = require('path');
const root = path.resolve(__dirname, '../..');
const { chromium } = require(require.resolve('playwright', { paths: [path.join(root, 'tests/remote')] }));

let failed = 0;
function check(cond, what) { console.log(`${cond ? 'ok  ' : 'FAIL'} ${what}`); if (!cond) failed++; }

const TYPES = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.mp3': 'audio/mpeg', '.json': 'application/json', '.svg': 'image/svg+xml', '.png': 'image/png', '.webp': 'image/webp', '.jpg': 'image/jpeg', '.woff2': 'font/woff2', '.mp4': 'video/mp4' };
const server = http.createServer((req, res) => {
  let p = decodeURIComponent(new URL(req.url, 'http://x').pathname);
  if (p.endsWith('/')) p += 'index.html';
  const file = path.join(root, p);
  if (!file.startsWith(root) || !fs.existsSync(file) || fs.statSync(file).isDirectory()) { res.writeHead(404); return res.end(); }
  res.writeHead(200, { 'Content-Type': TYPES[path.extname(file)] || 'application/octet-stream' });
  fs.createReadStream(file).pipe(res);
});

(async () => {
  // every page's player data: valid JSON, Default first, one option per preset
  const pages = fs.readdirSync(path.join(root, 'plugins')).filter((d) => fs.existsSync(path.join(root, 'plugins', d, 'index.html')));
  let players = 0, bad = [];
  for (const d of pages) {
    const html = fs.readFileSync(path.join(root, 'plugins', d, 'index.html'), 'utf8');
    const m = html.match(/data-live="(\w+)" data-presets="([^"]*)"/);
    if (!m) continue;
    players++;
    try {
      const data = JSON.parse(m[2].replace(/&quot;/g, '"').replace(/&#x27;/g, "'").replace(/&amp;/g, '&').replace(/&lt;/g, '<').replace(/&gt;/g, '>'));
      const options = (html.match(/<select class="live-preset">([\s\S]*?)<\/select>/)[1].match(/<option/g) || []).length;
      if (data.p[0][0] !== 'Default' || data.p.length !== options || !fs.existsSync(path.join(root, 'web/fx', `${m[1]}.js`))) bad.push(d);
    } catch (e) { bad.push(d); }
  }
  check(players > 100 && !bad.length, `${players} plugin pages have a live player with good data ${bad.join(' ')}`);
  check(!/data-live=/.test(fs.readFileSync(path.join(root, 'plugins/drum-machine/index.html'), 'utf8')), 'an instrument gets no live player');

  // every chain page: a picture for each plugin of its window, and good live data where it has a player
  const chainDirs = fs.readdirSync(path.join(root, 'chains')).filter((d) => fs.existsSync(path.join(root, 'chains', d, 'index.html')));
  let chainPlayers = 0, badChains = [];
  for (const d of chainDirs) {
    const html = fs.readFileSync(path.join(root, 'chains', d, 'index.html'), 'utf8');
    const shots = (html.match(/class="cw-item[^"]*" data-shot="(\d+)"/g) || []).length;
    if (!shots || !Array.from({ length: shots }, (_, k) => path.join(root, 'assets/img/chains', d, `${k + 1}.webp`)).every((f) => fs.existsSync(f))) badChains.push(d);
    const m = html.match(/data-live="chain" data-chain="([^"]*)"/);
    if (!m) continue;
    chainPlayers++;
    try {
      const data = JSON.parse(m[1].replace(/&quot;/g, '"').replace(/&#x27;/g, "'").replace(/&amp;/g, '&'));
      if (!data.length || !data.every((f) => fs.existsSync(path.join(root, 'web/fx', `${f.id}.js`)))) badChains.push(d);
    } catch (e) { badChains.push(d); }
  }
  check(chainDirs.length > 80 && chainPlayers > 10 && !badChains.length, `${chainDirs.length} chain pages with their pictures, ${chainPlayers} with a live player ${badChains.join(' ')}`);
  check(!/data-live=/.test(fs.readFileSync(path.join(root, 'chains/acid-line/index.html'), 'utf8')), 'a chain with an instrument gets no live player');
  check(!/data-live=/.test(fs.readFileSync(path.join(root, 'chains/auto-filter/index.html'), 'utf8')), 'a chain with modulation gets no live player');

  // the product pages and the homepage: every picture, loop and poster they show from /assets/ is in the repo
  const home = fs.readFileSync(path.join(root, 'index.html'), 'utf8');
  check((home.match(/class="fm-tile"/g) || []).length === 5 && /poster="\/assets\/video\/trailer\.jpg"/.test(home), 'the homepage shows Delay in five forms and the trailer');
  for (const d of ['rack', 'hub', 'remotes', '']) {
    const html = fs.readFileSync(path.join(root, d, 'index.html'), 'utf8');
    const refs = [...html.matchAll(/(?:src|poster|srcset)="([^"]+)"/g)].flatMap((m) => m[1].split(',').map((x) => x.trim().split(' ')[0]))
      .filter((u) => u.startsWith('/assets/'));
    const missing = refs.filter((u) => !fs.existsSync(path.join(root, u)));
    const loops = (html.match(/data-clip/g) || []).length;
    check(refs.length > 5 && !missing.length && loops === (d === 'rack' || d === 'hub' ? 3 : 0), `/${d}${d ? '/' : ''} shows ${refs.length} files from the repo, all there, and ${loops} loops ${missing.join(' ')}`);
  }

  await new Promise((ok) => server.listen(0, '127.0.0.1', ok));
  const base = `http://127.0.0.1:${server.address().port}`;
  const browser = await chromium.launch({ args: ['--autoplay-policy=no-user-gesture-required'] });
  const page = await browser.newPage();
  const errors = [];
  page.on('pageerror', (e) => errors.push(String(e)));
  page.on('console', (m) => { if (m.type() === 'error') errors.push(m.text()); });
  // listen to what reaches the speakers: an analyser beside the player's output
  await page.addInitScript(() => {
    const connect = AudioNode.prototype.connect;
    // the spectral centroid (Hz) of what plays: Tilt's bright presets raise it, bypass brings it back
    window.__spectrum = () => { const a = new Float32Array(window.__an.frequencyBinCount); window.__an.getFloatFrequencyData(a);
      let s = 0, w = 0; a.forEach((db, i) => { const p = Math.pow(10, db / 10); s += p * i; w += p; }); return [s * window.__an.context.sampleRate / window.__an.fftSize, w]; };
    window.__level = () => { if (!window.__an) return -1; const a = new Float32Array(8192); window.__an.getFloatTimeDomainData(a); return Math.sqrt(a.reduce((s, x) => s + x * x, 0) / a.length); };
    AudioNode.prototype.connect = function (dst, ...rest) {
      if (this instanceof GainNode && dst === this.context.destination && !window.__an) { window.__an = this.context.createAnalyser(); window.__an.fftSize = 8192; connect.call(this, window.__an); }
      return connect.call(this, dst, ...rest);
    };
  });
  const level = () => page.evaluate(() => window.__level());
  // over one bar of the loop (2.4 s: the drum pattern repeats every bar), power-weighted
  const centroid = async () => { let s = 0, w = 0; for (let k = 0; k < 24; k++) { const [a, b] = await page.evaluate(() => window.__spectrum()); s += a; w += b; await page.waitForTimeout(100); } return s / w; };
  await page.goto(`${base}/plugins/tilt/`);
  await page.click('.live-play');
  await page.waitForFunction(() => window.__level() > 0.01, null, { timeout: 15000 }).catch(() => {});
  check((await page.getAttribute('.live', 'class')).includes('demo-playing') && (await level()) > 0.01, 'Play runs the loop through the plugin');
  await page.waitForTimeout(1000);
  const plain = await centroid();
  await page.selectOption('.live-preset', { label: 'Very Bright' });
  await page.waitForTimeout(1000);
  const bright = await centroid();
  await page.click('.live-ab');
  await page.waitForTimeout(1000);
  const bypassed = await centroid();
  check(bright > plain * 1.15 && bypassed < bright / 1.15, `the preset and bypass change the sound (centroid ${plain.toFixed(0)} Hz, Very Bright ${bright.toFixed(0)} Hz, bypassed ${bypassed.toFixed(0)} Hz)`);
  check((await page.getAttribute('.live-ab', 'aria-pressed')) === 'true', 'the bypass button says it is pressed');
  await page.selectOption('.live-loop', 'drums');
  await page.waitForTimeout(1500);
  check((await level()) > 0.01, 'another loop plays');
  await page.click('.live-play');
  check(!(await page.getAttribute('.live', 'class')).includes('demo-playing'), 'Stop stops it');
  check(errors.length === 0, `no errors on the page ${errors.join(' | ')}`);

  // a chain page: the window shows the plugin picked in its list, and the whole chain plays live
  await page.goto(`${base}/chains/lo-fi/`);
  await page.click('.cw-item[data-shot="2"]');
  await page.waitForFunction(() => { const i = document.querySelector('.cw-shot'); return i.complete && i.naturalWidth > 0 && /\/2\.webp$/.test(i.src); }, null, { timeout: 5000 }).catch(() => {});
  check(await page.evaluate(() => { const i = document.querySelector('.cw-shot'); return /\/2\.webp$/.test(i.src) && i.naturalWidth === +i.getAttribute('width'); }),
    'the chain window shows the second plugin when it is picked');
  check(await page.$('svg.chain-diagram') !== null, 'the chain page has its signal-flow diagram');
  await page.click('.live-play');
  await page.waitForFunction(() => window.__level() > 0.01, null, { timeout: 15000 }).catch(() => {});
  check((await level()) > 0.01, 'Play runs the loop through the whole chain');
  await page.waitForTimeout(1000);
  const chained = await centroid();
  await page.click('.live-ab');
  await page.waitForTimeout(1000);
  const dry = await centroid();
  check(dry > chained * 1.15, `the Lo-Fi chain darkens the loop and bypass brings it back (centroid ${chained.toFixed(0)} Hz, bypassed ${dry.toFixed(0)} Hz)`);
  check(errors.length === 0, `no errors on the chain page ${errors.join(' | ')}`);
  // a product page: no errors, and a loop is only fetched once it is on screen
  await page.goto(`${base}/rack/`);
  await page.waitForTimeout(500);
  const preloads = () => page.evaluate(() => [...document.querySelectorAll('video[data-clip]')].map((v) => v.preload));
  check((await preloads()).every((p) => p === 'none'), 'the Rack page fetches no loop before it is seen');
  await page.evaluate(() => document.querySelector('video[data-clip]').scrollIntoView({ block: 'center' }));
  await page.waitForTimeout(800);
  const after = await preloads();
  check(after[0] === 'auto' && after.slice(1).includes('none'), `the loop on screen starts and the others wait (${after.join(' ')})`);
  check(errors.length === 0, `no errors on the Rack page ${errors.join(' | ')}`);
  const still = await browser.newPage({ reducedMotion: 'reduce' });
  await still.goto(`${base}/hub/`);
  await still.waitForTimeout(500);
  check(await still.evaluate(() => [...document.querySelectorAll('video[data-clip]')].every((v) => v.controls && v.paused)), 'with reduced motion the loops stay still, with controls');
  await browser.close();
  server.close();
  console.log(failed ? `${failed} checks failed` : 'all checks passed');
  process.exit(failed ? 1 : 0);
})();
