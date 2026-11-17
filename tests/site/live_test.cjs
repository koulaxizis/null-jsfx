// Tests the live player of the plugin pages (assets/js/live.js) in a real browser: the page is
// served from the repo, a loop plays through the plugin's web version, and the sound that reaches
// the speakers changes with the preset and with bypass. Also reads every plugin page's player data.
// Usage: node tests/site/live_test.cjs   (Playwright from tests/remote: npm ci there first)
'use strict';
const fs = require('fs');
const http = require('http');
const path = require('path');
const root = path.resolve(__dirname, '../..');
const { chromium } = require(require.resolve('playwright', { paths: [path.join(root, 'tests/remote')] }));

let failed = 0;
function check(cond, what) { console.log(`${cond ? 'ok  ' : 'FAIL'} ${what}`); if (!cond) failed++; }

const TYPES = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.mp3': 'audio/mpeg', '.json': 'application/json', '.svg': 'image/svg+xml', '.png': 'image/png', '.jpg': 'image/jpeg', '.woff2': 'font/woff2' };
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
  await browser.close();
  server.close();
  console.log(failed ? `${failed} checks failed` : 'all checks passed');
  process.exit(failed ? 1 : 0);
})();
