// The stand-in for the Hub's Rust side, shared by tests/ui_test.cjs and the Hub video
// (tools/video/hub_tour.cjs): the real catalog, a fake computer with Tilt's JSFX from an older
// release, and every command the page sends recorded in window.__calls. It runs in the page
// (page.addInitScript(mock, catalog)), so it must not use anything from Node.
'use strict';

function mock(catalog) {
  const all = ['jsfx', 'gfx', 'vst3', 'clap'];
  const state = {};
  for (const p of catalog.plugins) state[p.id] = Object.fromEntries(p.formats.map((f) => [f, { installed: false, version: null, outdated: false }]));
  state.tilt.jsfx = { installed: true, version: '2.1', outdated: true };
  const extras = Object.fromEntries(catalog.extras.map((x) => [x.id, { installed: false, version: null, outdated: false }]));
  extras.chains = { installed: true, version: null, outdated: true }; // from the installer
  const settings = { targets: { reaper: '/home/me/.config/REAPER', vst3: '/home/me/.vst3', clap: '/home/me/.clap', fonts: '/home/me/.local/share/fonts' }, formats: all, check_updates: true, notify: true, autostart: false, new_ids: ['tape'], notified: null, last_check: 0, held: [], language: '' };
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
          const progress = (done, total) => (listeners['hub-progress'] || []).forEach((f) => f({ payload: { stage: 'download', text: 'NULL-JSFX-JSFX.zip', done, total } }));
          progress(0, 24e6);
          // the Hub video (tools/video/hub_tour.cjs) sets __applyMs so the progress can be seen
          for (let i = 1; i <= 10 && catalog.__applyMs; i++) { await new Promise((r) => setTimeout(r, catalog.__applyMs / 10)); progress(i * 2.4e6, 24e6); }
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

module.exports = { mock };
