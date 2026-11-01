// NULL JSFX Android remotes: the tabs, settings storage and update notice.
// NULL JSFX • https://nulljsfx.tech • MIT License
//
// One shell, two apps (android/app/build.gradle.kts): Remote for REAPER (app 'reaper') and the
// mixer remote (app 'mixers'). A tab registers itself before NullRemote.start():
//   NullRemote.register({
//     id: 'reaper', title: 'REAPER', order: 10,
//     app: 'reaper'  the app it belongs to; without it (Settings) it shows in both
//     mount(el)      build the tab into el (once, the first time it is shown)
//     show(), hide() optional, when the user switches to or away from it
//     back()         optional: handle the phone's Back button, return true if it did
//   })
// The mixers work registers id 'mixers', app 'mixers' (android/web/mixers/mixers.js); until it
// exists the mixer app shows a placeholder. Connectors may add any other members they need.
'use strict';
(function () {
  const all = new Map();   // every registered tab
  const tabs = new Map();  // the tabs of this app
  let current = null;
  const APP = (window.NullShell && NullShell.info().app) || 'reaper';

  const $ = id => document.getElementById(id);
  function el(tag, cls, text) {
    const e = document.createElement(tag);
    if (cls) e.className = cls;
    if (text != null) e.textContent = text;
    return e;
  }

  /** The NULL JSFX header: logo, "NULL JSFX" with a page name, and the slogan. */
  function header(page) {
    const h = el('header', 'top');
    h.innerHTML = '<svg viewBox="0 0 80 80" width="38" height="38" aria-hidden="true">' +
      '<defs><linearGradient id="ng" x1="0" y1="1" x2="0" y2="0"><stop offset="0" stop-color="#888"/><stop offset="1" stop-color="#aaa"/></linearGradient></defs>' +
      '<rect x=".5" y=".5" width="79" height="79" rx="12" fill="#2d2d2d" stroke="#4a4a4a"/>' +
      '<path d="M24 20v40l32-40v40" stroke="url(#ng)" stroke-width="6" stroke-linejoin="round" stroke-linecap="round" fill="none"/></svg>' +
      '<div><div class="name">NULL JSFX<span></span></div><div class="slogan">AUDIO WITHOUT NOISE</div></div>';
    h.querySelector('.name span').textContent = page;
    return h;
  }

  const PREFIX = 'null-remote-';
  const prefs = {
    get(key, dflt) {
      try {
        const v = localStorage.getItem(PREFIX + key);
        return v == null ? dflt : JSON.parse(v);
      } catch (e) { return dflt; }
    },
    set(key, value) {
      try { localStorage.setItem(PREFIX + key, JSON.stringify(value)); } catch (e) { /* private mode */ }
    },
  };

  function register(tab) {
    if (!tab || !tab.id) throw new Error('NullRemote.register: a tab needs an id');
    const t = Object.assign({ order: 50, title: tab.id }, tab, { el: null });
    all.set(t.id, t);
    if (t.app && t.app !== APP) return;
    tabs.set(t.id, t);
    if (document.readyState !== 'loading' && $('tabs').childElementCount) build();
  }

  function build() {
    const nav = $('tabs');
    nav.textContent = '';
    for (const t of [...tabs.values()].sort((a, b) => a.order - b.order)) {
      const b = el('button', t.id === current ? 'on' : '', t.title);
      b.dataset.tab = t.id;
      b.onclick = () => select(t.id);
      nav.appendChild(b);
    }
  }

  function select(id) {
    const t = tabs.get(id);
    if (!t) return;
    if (current && current !== id) {
      const old = tabs.get(current);
      if (old.el) old.el.hidden = true;
      if (old.hide) old.hide();
    }
    current = id;
    if (!t.el) {
      t.el = el('section', 'view');
      t.el.dataset.tab = id;
      $('views').appendChild(t.el);
      t.mount(t.el);
    }
    t.el.hidden = false;
    if (t.show) t.show();
    for (const b of $('tabs').children) b.classList.toggle('on', b.dataset.tab === id);
    prefs.set('tab', id);
  }

  /** The phone's Back button (MainActivity.onBackPressed): true when a page handled it. */
  function back() {
    const t = current && tabs.get(current);
    if (t && t.back && t.back()) return true;
    const first = [...tabs.values()].sort((a, b) => a.order - b.order)[0];
    if (t && first && t.id !== first.id) { select(first.id); return true; }
    return false;
  }

  // --- the update notice (the GitHub APK only; F-Droid and Droidify update it themselves) ---

  const RELEASES = 'https://api.github.com/repos/koulaxizis/null-jsfx/releases/latest';
  const APK = { reaper: 'NULL-JSFX-Remote-REAPER-Android.apk', mixers: 'NULL-JSFX-Remote-Mixers-Android.apk' }[APP];

  function newer(a, b) {
    const x = String(a).split('.').map(n => parseInt(n, 10) || 0);
    const y = String(b).split('.').map(n => parseInt(n, 10) || 0);
    for (let i = 0; i < Math.max(x.length, y.length); i++) {
      const d = (x[i] || 0) - (y[i] || 0);
      if (d) return d > 0;
    }
    return false;
  }

  async function checkUpdate(force) {
    const info = NullShell.info();
    if (!info.updateCheck || !prefs.get('updates', true)) return null;
    const last = prefs.get('update-checked', 0);
    if (!force && Date.now() - last < 12 * 3600e3) return showUpdate(prefs.get('update-found', null));
    try {
      const res = await fetch(RELEASES, { headers: { Accept: 'application/vnd.github+json' }, cache: 'no-store' });
      if (!res.ok) return null;
      const rel = await res.json();
      const asset = (rel.assets || []).find(a => a.name === APK);
      const found = asset && newer(rel.tag_name, info.version) ? { version: rel.tag_name, url: asset.browser_download_url } : null;
      prefs.set('update-checked', Date.now());
      prefs.set('update-found', found);
      return showUpdate(found);
    } catch (e) {
      return null;
    }
  }

  function showUpdate(found) {
    const info = NullShell.info();
    if (!found || !newer(found.version, info.version) || prefs.get('update-skip', '') === found.version) {
      $('update').hidden = true;
      return found;
    }
    $('update-text').textContent = `${document.title} ${found.version} is out (you have ${info.version}).`;
    $('update-get').onclick = () => NullShell.openUrl(found.url);
    $('update-later').onclick = () => { prefs.set('update-skip', found.version); $('update').hidden = true; };
    $('update').hidden = false;
    return found;
  }

  function start() {
    document.title = APP === 'mixers' ? 'Remote for Mixers' : 'Remote for REAPER';
    if (APP === 'mixers' && !tabs.has('mixers')) {
      register({
        id: 'mixers', title: 'Mixers', order: 20, app: 'mixers',
        mount(e) {
          e.appendChild(header('Mixers'));
          const m = el('div', 'empty');
          m.appendChild(el('p', '', 'Behringer X AIR comes first, then X32 and Midas M32.'));
          e.appendChild(m);
        },
      });
    }
    build();
    NullShell.keepAwake(prefs.get('awake', true));
    NullShell.fullscreen(prefs.get('fullscreen', false));
    const want = prefs.get('tab', null);
    select(tabs.has(want) ? want : [...tabs.values()].sort((a, b) => a.order - b.order)[0].id);
    checkUpdate(false);
  }

  window.NullRemote = { app: APP, register, select, back, start, header, el, prefs, checkUpdate, newer, get current() { return current; } };
})();
