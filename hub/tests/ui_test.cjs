// Tests the Hub's page (hub/ui) in a real browser, with a stand-in for the Rust side: the real
// catalog from tools/build_catalog.py, a fake computer with Tilt's JSFX from an older release,
// and every command the page sends recorded. Screenshots come from the repo's assets.
// Usage: node tests/ui_test.cjs [screenshot dir]
'use strict';
const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');
const { chromium } = require('playwright');

const hub = path.resolve(__dirname, '..');
const root = path.resolve(hub, '..');
const shots = process.argv[2];

const catalogFile = path.join(fs.mkdtempSync(path.join(require('os').tmpdir(), 'hub-')), 'catalog.json');
execFileSync('python3', [path.join(root, 'tools/build_catalog.py'), '--version', '2.2', '--out', catalogFile]);
const catalog = JSON.parse(fs.readFileSync(catalogFile, 'utf8'));
catalog.previous = ['2.1'];
catalog.notes = '## New\n- **NULL JSFX Hub**: install and update from one place\n- Faster `Delay`\n\nThanks for using NULL JSFX.';

// --- the fake Rust side ----------------------------------------------------------------------

function mock(catalog) {
  const all = ['jsfx', 'gfx', 'vst3', 'clap'];
  const state = {};
  for (const p of catalog.plugins) state[p.id] = Object.fromEntries(p.formats.map((f) => [f, { installed: false, version: null, outdated: false }]));
  state.tilt.jsfx = { installed: true, version: '2.1', outdated: true };
  const extras = Object.fromEntries(catalog.extras.map((x) => [x.id, { installed: false, version: null, outdated: false }]));
  extras.chains = { installed: true, version: null, outdated: true }; // from the installer
  const settings = { targets: { reaper: '/home/me/.config/REAPER', vst3: '/home/me/.vst3', clap: '/home/me/.clap', fonts: '/home/me/.local/share/fonts' }, formats: all, check_updates: true, notify: true, autostart: false, new_ids: ['tape'], notified: null, last_check: 0 };
  const view = () => {
    let installed = 0, outdated = 0;
    for (const m of Object.values(state)) for (const s of Object.values(m)) { installed += s.installed; outdated += s.outdated; }
    for (const s of Object.values(extras)) outdated += s.outdated;
    return JSON.parse(JSON.stringify({ catalog, state: { plugins: state, extras, installed, outdated }, settings, os: 'Linux', hubVersion: '2.1.0', hubUpdate: ['2.2', 'https://github.com/koulaxizis/null-jsfx/releases/latest/download/NULL-JSFX-Hub-Linux.AppImage'], dataDir: '/home/me/.local/share/null-jsfx' }));
  };
  const listeners = {};
  window.__calls = [];
  window.__TAURI__ = {
    core: {
      async invoke(cmd, args) {
        window.__calls.push([cmd, args ? JSON.parse(JSON.stringify(args)) : null]);
        await new Promise((r) => setTimeout(r, 30));
        if (cmd === 'hub_load' || cmd === 'hub_refresh') return view();
        if (cmd === 'hub_default_targets') return settings.targets;
        if (cmd === 'hub_hosts') return window.__hosts || [];
        if (cmd === 'hub_export') return '/home/me/Downloads/NULL-JSFX-Selection.json';
        if (cmd === 'hub_verify') return window.__broken || [];
        if (cmd === 'hub_self_update') return '/home/me/.local/share/null-jsfx/hub-update/2.2/NULL-JSFX-Hub-Linux.AppImage';
        if (cmd === 'hub_save_settings') { Object.assign(settings, args.settings); return view(); }
        if (cmd === 'hub_apply') {
          (listeners['hub-progress'] || []).forEach((f) => f({ payload: { stage: 'download', text: 'NULL-JSFX-JSFX.zip', done: 1, total: 2 } }));
          const from = args.request.from;
          for (const [id, f] of args.request.install) state[id][f] = { installed: true, version: from || catalog.version, outdated: false, held: !!from };
          for (const [id, f] of args.request.remove) state[id][f] = { installed: false, version: null, outdated: false };
          for (const id of args.request.extras || []) extras[id] = { installed: true, version: catalog.version, outdated: false };
          for (const id of args.request.extras_remove || []) extras[id] = { installed: false, version: null, outdated: false };
          return view();
        }
        throw new Error('unknown command ' + cmd);
      },
    },
    event: { listen(name, fn) { (listeners[name] = listeners[name] || []).push(fn); return Promise.resolve(() => {}); } },
    opener: { openUrl(u) { window.__calls.push(['open', u]); }, revealItemInDir(p) { window.__calls.push(['reveal', p]); } },
  };
}

// --- checks ----------------------------------------------------------------------------------

let failed = 0;
function check(cond, what) {
  console.log(`${cond ? 'ok  ' : 'FAIL'} ${what}`);
  if (!cond) failed++;
}

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1280, height: 820 } });
  const errors = [];
  page.on('pageerror', (e) => errors.push(String(e)));
  await page.route('https://nulljsfx.tech/**', (route) => {
    const file = path.join(root, new URL(route.request().url()).pathname);
    if (fs.existsSync(file)) route.fulfill({ path: file });
    else route.abort();
  });
  await page.addInitScript(mock, catalog);
  await page.goto('file://' + path.join(hub, 'ui/index.html'));
  await page.waitForSelector('.card');
  const calls = () => page.evaluate(() => window.__calls);

  const effects = catalog.plugins.filter((p) => p.group !== 'extras').length;
  check((await page.locator('.card').count()) === effects, `${effects} plugin cards (the Rack is under Extras)`);
  check((await page.textContent('#release')).includes('2 to update'), 'header shows Tilt and the FX chains to update');
  check(await page.isVisible('#update-all'), 'Update all button shows');
  check((await page.textContent('#n-updates')) === '1', 'Updates count in the menu (plugins)');
  check((await calls()).some(([c]) => c === 'hub_refresh'), 'checks for a new release on start');

  check(await page.isVisible('#nav-new'), 'New in this release shows in the menu');
  check((await page.locator('.card[data-id="tape"] .chip.new').count()) === 1, 'a new plugin carries a NEW mark');
  check((await page.textContent('#hub-update')).includes('A new NULL JSFX Hub (2.2)'), 'a newer Hub is announced');

  await page.fill('#search', 'reverb');
  const n = await page.locator('.card').count();
  check(n > 0 && n < 10, `search "reverb" narrows to ${n}`);
  await page.fill('#search', '');
  await page.waitForTimeout(100);
  if (shots) await page.screenshot({ path: path.join(shots, 'hub-plugins.png') });

  // Install from a card: the formats from Settings (all four)
  await page.click('.card[data-id="delay"] .card-foot button');
  await page.waitForFunction(() => window.__calls.some(([c]) => c === 'hub_apply'));
  await page.waitForSelector('#progress', { state: 'hidden' });
  const apply = (await calls()).find(([c]) => c === 'hub_apply')[1].request;
  check(JSON.stringify(apply.install) === JSON.stringify([['delay', 'jsfx'], ['delay', 'gfx'], ['delay', 'vst3'], ['delay', 'clap']]), 'Install on a card asks for all four formats');
  check((await page.textContent('.card[data-id="delay"] .card-foot button:last-child')) === 'Installed', 'card then says Installed');
  check((await page.locator('.card[data-id="delay"] button[data-act="remove"]').count()) === 1, 'an installed card has a remove button');
  check((await page.locator('.card[data-id="delay"] .chip.on').count()) === 4, 'four format chips lit');

  // the plugin page: update JSFX, add VST3
  await page.click('.card[data-id="tilt"]');
  await page.waitForSelector('#detail:not([hidden])');
  check((await page.textContent('#d-name')) === 'Tilt', 'plugin page opens');
  check((await page.locator('#d-sliders dt').count()) >= 1, 'plugin page lists the controls');
  check((await page.textContent('#d-formats')).includes('update: 2.1 → 2.2'), 'shows the JSFX update from 2.1 to 2.2');
  await page.check('#d-formats input[data-f="vst3"]');
  check((await page.textContent('#d-apply')) === 'Install 1, update 1', 'Apply button says what will happen');
  if (shots) await page.screenshot({ path: path.join(shots, 'hub-plugin-page.png') });
  await page.click('#d-apply');
  await page.waitForFunction(() => window.__calls.filter(([c]) => c === 'hub_apply').length === 2);
  await page.waitForSelector('#progress', { state: 'hidden' });
  const second = (await calls()).filter(([c]) => c === 'hub_apply')[1][1].request;
  check(JSON.stringify(second) === JSON.stringify({ install: [['tilt', 'jsfx'], ['tilt', 'vst3']], remove: [] }), 'plugin page request: update JSFX, install VST3');
  check((await page.textContent('#update-all')) === 'Update all (1)', 'the FX chains are left to update');

  // the demo: the plugin and the dry music, switched while playing
  check(await page.isVisible('#d-demo'), 'the plugin page has a demo');
  check((await page.textContent('#d-wet')) === 'Tilt' && (await page.textContent('#d-demo-note')).includes('Preset "'), 'the demo names the plugin and its preset');
  await page.click('#d-play');
  await page.waitForFunction(() => document.getElementById('d-play').textContent === 'Stop', null, { timeout: 10000 }).catch(() => {});
  check((await page.textContent('#d-play')) === 'Stop', 'the demo plays');
  await page.click('#d-dry');
  check(await page.evaluate(() => document.getElementById('d-dry').classList.contains('on')), 'Dry switches to the music without the plugin');
  if (shots) await page.screenshot({ path: path.join(shots, 'hub-demo.png') });

  // removing from the plugin page
  await page.uncheck('#d-formats input[data-f="vst3"]');
  check((await page.textContent('#d-apply')) === 'Remove 1', 'unticking an installed format removes it');
  await page.keyboard.press('Escape');
  check((await page.textContent('#d-play')) === 'Play', 'closing the plugin page stops the demo');

  // Settings: drop CLAP
  await page.click('#nav button[data-view="settings"]');
  await page.uncheck('#set-formats input[value="clap"]');
  await page.check('#set-autostart');
  await page.click('#set-save');
  await page.waitForFunction(() => window.__calls.some(([c]) => c === 'hub_save_settings'));
  const saved = (await calls()).find(([c]) => c === 'hub_save_settings')[1].settings;
  check(JSON.stringify(saved.formats) === '["jsfx","gfx","vst3"]', 'Settings saves the default formats');
  check(saved.autostart === true && saved.notify === true && saved.check_updates === true, 'Settings saves start with the computer and notifications');
  if (shots) await page.screenshot({ path: path.join(shots, 'hub-settings.png') });

  // a group page and Install all shown
  await page.click('#nav button[data-view="group:one-slider"]');
  const os = catalog.plugins.filter((p) => p.group === 'one-slider').length;
  check((await page.locator('.card').count()) === os, `One Slider shows its ${os} plugins`);
  check((await page.locator('#pick .chip-btn.on').count()) === 3, 'the format toggles above the list follow Settings');
  check((await page.textContent('.card[data-id="tilt"] button[data-act="install"]')) === '+ GFX', 'Tilt (JSFX and VST3 installed) offers the missing GFX');
  check((await page.textContent('#bulk')) === `Install ${os} plugins (${(os - 1) * 3 + 1} files)`, 'bulk install counts plugins that miss a picked format, installed ones too');
  await page.click('#bulk');
  await page.waitForFunction(() => window.__calls.filter(([c]) => c === 'hub_apply').length === 3);
  const bulk = (await calls()).filter(([c]) => c === 'hub_apply')[2][1].request;
  check(bulk.install.length === (os - 1) * 3 + 1 && bulk.install.every(([, f]) => f !== 'clap'), 'group install uses the saved formats (no CLAP)');
  check(bulk.install.some(([id, f]) => id === 'tilt' && f === 'gfx') && !bulk.install.some(([id, f]) => id === 'tilt' && f === 'jsfx'), 'bulk install adds only the missing formats of an installed plugin');
  await page.waitForSelector('#progress', { state: 'hidden' });

  // Extras: the Rack card and the extras
  await page.click('#nav button[data-view="extras"]');
  check((await page.locator('.card').count()) === 1 && (await page.textContent('.card .card-name')) === 'Rack', 'Extras lists the Rack');
  check((await page.locator('.extra').count()) === catalog.extras.length, `Extras lists the ${catalog.extras.length} extras`);
  check((await page.textContent('.extra[data-id="chains"] .st')).startsWith('update'), 'FX chains from the installer show an update');
  await page.click('.extra[data-id="remote"] button[data-do="add"]');
  await page.waitForFunction(() => window.__calls.filter(([c]) => c === 'hub_apply').length === 4);
  const ex = (await calls()).filter(([c]) => c === 'hub_apply')[3][1].request;
  check(JSON.stringify(ex.extras) === '["remote"]', 'Turn on asks for the tablet remote');
  await page.waitForSelector('#progress', { state: 'hidden' });
  check((await page.textContent('.extra[data-id="remote"] .st')).startsWith('on'), 'the remote shows as on');
  if (shots) await page.screenshot({ path: path.join(shots, 'hub-extras.png') });
  await page.click('button[data-steps]');
  check((await calls()).some(([c, p]) => c === 'reveal' && p.endsWith('/Remote-INSTALL.txt')), 'the remote setup steps open');
  await page.click('#update-all');
  await page.waitForFunction(() => window.__calls.filter(([c]) => c === 'hub_apply').length === 5);
  const ua = (await calls()).filter(([c]) => c === 'hub_apply')[4][1].request;
  check(JSON.stringify(ua.extras) === '["chains"]', 'Update all includes the extras');
  await page.waitForSelector('#progress', { state: 'hidden' });

  const applies = async () => (await calls()).filter(([c]) => c === 'hub_apply').map(([, a]) => a.request);
  const nextApply = async (n) => { await page.waitForFunction((n) => window.__calls.filter(([c]) => c === 'hub_apply').length === n, n); await page.waitForSelector('#progress', { state: 'hidden' }); return (await applies())[n - 1]; };

  // the format toggles: CLAP back on, then a whole group in another format
  await page.click('#nav button[data-view="group:one-slider"]');
  await page.click('#pick button[data-pick="clap"]');
  await page.waitForFunction(() => document.querySelectorAll('#pick .chip-btn.on').length === 4);
  check((await page.locator('#pick .chip-btn.on').count()) === 4, 'CLAP toggled on above the list');
  check((await page.textContent('#bulk')) === `Install ${os} plugins (${os} files)`, 'the whole group can now get its CLAP version at once');
  await page.click('#bulk');
  const clap = await nextApply(6);
  check(clap.install.length === os && clap.install.every(([, f]) => f === 'clap'), 'bulk install of one more format: only CLAP');
  if (shots) await page.screenshot({ path: path.join(shots, 'hub-group.png') });

  // removing one plugin from its card, with a question first
  await page.click('#nav button[data-view="all"]');
  await page.click('.card[data-id="delay"] button[data-act="remove"]');
  check(await page.isVisible('#confirm'), 'Remove asks first');
  check((await page.textContent('#c-title')) === 'Remove Delay?', 'the question names the plugin');
  if (shots) await page.screenshot({ path: path.join(shots, 'hub-remove.png') });
  await page.click('#c-ok');
  const rmDelay = await nextApply(7);
  check(JSON.stringify(rmDelay) === JSON.stringify({ install: [], remove: [['delay', 'jsfx'], ['delay', 'gfx'], ['delay', 'vst3'], ['delay', 'clap']] }), 'card Remove takes out every format of Delay');
  check((await page.textContent('.card[data-id="delay"] .card-foot button')) === 'Install', 'Delay can be installed again');

  // removing from the plugin page
  await page.click('.card[data-id="tilt"]');
  check(await page.isVisible('#d-remove'), 'the plugin page has Remove all formats');
  await page.click('#d-remove');
  await page.click('#c-ok');
  const rmTilt = await nextApply(8);
  check(rmTilt.remove.length === 4 && rmTilt.remove.every(([id]) => id === 'tilt'), 'plugin page removes every installed format of Tilt');
  await page.keyboard.press('Escape');

  // Installed: remove all shown, cancelled
  await page.click('#nav button[data-view="installed"]');
  const shownInstalled = await page.locator('.card').count();
  check((await page.textContent('#bulk-remove')) === `Remove ${shownInstalled} plugins`, 'Installed offers to remove all shown');
  await page.click('#bulk-remove');
  await page.click('#c-cancel');
  check(!(await page.isVisible('#confirm')) && (await applies()).length === 8, 'Cancel removes nothing');

  // Settings: remove everything
  await page.click('#nav button[data-view="settings"]');
  check((await page.textContent('#remove-all-n')).includes(`${shownInstalled} plugins installed`), 'Settings says how many plugins are installed');
  await page.click('#remove-all');
  check((await page.textContent('#c-title')) === 'Remove everything?', 'Remove everything asks first');
  await page.click('#c-ok');
  const all = await nextApply(9);
  check(all.install.length === 0 && new Set(all.remove.map(([id]) => id)).size === shownInstalled, 'Remove everything takes out every installed plugin');
  check(JSON.stringify(all.extras_remove.sort()) === '["chains","remote"]', 'and the extras that are on');
  check((await page.textContent('#n-installed')) === '' && (await page.isDisabled('#remove-all')), 'nothing is left installed');

  // a running DAW: asked to close it before VST3/CLAP files change; Cancel does nothing
  await page.evaluate(() => (window.__hosts = ['REAPER']));
  await page.click('#nav button[data-view="all"]');
  await page.click('.card[data-id="delay"] .card-foot button');
  await page.waitForSelector('#confirm:not([hidden])');
  check((await page.textContent('#c-title')) === 'Close REAPER first?', 'a running DAW is named before installing VST3/CLAP');
  if (shots) await page.screenshot({ path: path.join(shots, 'hub-daw.png') });
  await page.click('#c-cancel');
  check((await applies()).length === 9, 'Cancel installs nothing');
  await page.click('.card[data-id="delay"] .card-foot button');
  await page.waitForSelector('#confirm:not([hidden])');
  await page.click('#c-ok');
  const daw = await nextApply(10);
  check(daw.install.length === 4 && daw.install.every(([id]) => id === 'delay'), 'Continue installs anyway');
  await page.evaluate(() => (window.__hosts = []));

  // check and repair: all fine, then one damaged plugin file
  await page.click('#nav button[data-view="settings"]');
  await page.click('#verify');
  await page.waitForFunction(() => document.getElementById('toast').textContent.includes('match'));
  check((await page.textContent('#toast')).startsWith('All 4 plugin files match release 2.2'), 'check says everything matches');
  await page.evaluate(() => (window.__broken = [['delay', 'vst3']]));
  await page.click('#verify');
  await page.waitForSelector('#confirm:not([hidden])');
  check((await page.textContent('#c-title')) === 'Repair 1 plugin file?' && (await page.textContent('#c-text')).includes('Delay VST3'), 'check lists what to repair');
  await page.click('#c-ok');
  const fix = await nextApply(11);
  check(JSON.stringify(fix) === JSON.stringify({ install: [['delay', 'vst3']], remove: [] }), 'Repair installs the damaged file again');

  // the Hub updates itself; when it can't put the file in place it shows where it is
  await page.click('#hub-self');
  await page.waitForFunction(() => window.__calls.some(([c, p]) => c === 'reveal' && String(p).endsWith('.AppImage')));
  check((await calls()).some(([c]) => c === 'hub_self_update'), 'Update the Hub downloads it in the Hub, and shows the file when it has to be opened by hand');

  // roll Delay back to the release before, then update it again
  await page.click('#nav button[data-view="all"]');
  await page.click('.card[data-id="delay"]');
  check((await page.textContent('#d-rollback')) === 'Roll back to 2.1', 'the plugin page offers the release before');
  await page.click('#d-rollback');
  check((await page.textContent('#c-title')) === 'Roll Delay back to 2.1?', 'roll back asks first');
  await page.click('#c-ok');
  const back = await nextApply(12);
  check(back.from === '2.1' && back.install.length === 4 && back.install.every(([id]) => id === 'delay'), 'roll back installs every format of Delay from 2.1');
  check((await page.textContent('#d-formats')).includes('kept at 2.1'), 'the plugin page says it is kept at 2.1');
  check((await page.textContent('#d-rollback')) === 'Update to 2.2', 'and offers to update it again');
  await page.click('#d-rollback');
  const fwd = await nextApply(13);
  check(!fwd.from && fwd.install.length === 4, 'Update installs the latest again');
  await page.keyboard.press('Escape');

  // the list for another computer: save it, then install from one
  await page.click('#nav button[data-view="settings"]');
  await page.click('#export');
  await page.waitForFunction(() => window.__calls.some(([c, p]) => c === 'reveal' && String(p).endsWith('Selection.json')));
  check((await calls()).some(([c]) => c === 'hub_export'), 'Save the list writes the file and shows it');
  await page.setInputFiles('#import-file', { name: 'bad.json', mimeType: 'application/json', buffer: Buffer.from('{"x":1}') });
  await page.waitForFunction(() => document.getElementById('toast').textContent.includes('not a NULL JSFX list'));
  check(true, 'a file that is not a list is refused');
  const list = { kind: 'null-jsfx-selection', release: '2.2', plugins: { tilt: ['jsfx', 'clap'], delay: ['jsfx'], nope: ['jsfx'] }, extras: ['fonts'] };
  await page.setInputFiles('#import-file', { name: 'NULL-JSFX-Selection.json', mimeType: 'application/json', buffer: Buffer.from(JSON.stringify(list)) });
  await page.waitForSelector('#confirm:not([hidden])');
  check((await page.textContent('#c-title')) === 'Install 1 plugin from the list?', 'the list asks for what is missing here');
  await page.click('#c-ok');
  const imp = await nextApply(14);
  check(JSON.stringify(imp.install) === '[["tilt","jsfx"],["tilt","clap"]]' && JSON.stringify(imp.extras) === '["fonts"]', 'installs the missing plugin files and extras from the list');

  // What's new
  await page.click('#nav button[data-view="news"]');
  check((await page.textContent('#news-title')).startsWith('Release 2.2'), "What's new shows the release");
  check((await page.locator('#news li').count()) === 2, 'release notes render as a list');

  // the Hub download link and the others open in the browser
  await page.click('#hub-update a[data-ext]');
  check((await calls()).some(([c, u]) => c === 'open' && u.endsWith('NULL-JSFX-Hub-Linux.AppImage')), 'the new Hub link opens the download');
  await page.click('.side-foot a[href*="ko-fi"]');
  check((await calls()).some(([c, u]) => c === 'open' && u.includes('ko-fi.com/koulaxizis')), 'Ko-fi link opens outside the Hub');

  // the Hub's CSP allows no inline handlers: they would silently do nothing there
  const src = fs.readFileSync(path.join(hub, 'ui/index.html'), 'utf8') + fs.readFileSync(path.join(hub, 'ui/hub.js'), 'utf8');
  check(!/\son[a-z]+\s*=\s*["']/.test(src), 'no inline event handlers (blocked by the CSP)');
  check(errors.length === 0, `no page errors ${errors.join('; ')}`);
  await browser.close();
  if (failed) {
    console.log(`${failed} checks failed`);
    process.exit(1);
  }
  console.log('all checks passed');
})().catch((e) => {
  console.error(e);
  process.exit(1);
});
