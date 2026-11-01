// NULL JSFX Android remotes: the Settings tab. NULL JSFX • https://nulljsfx.tech • MIT License
'use strict';
(function () {
  const { el, prefs, header } = NullRemote;

  function toggle(card, key, label, dflt, apply) {
    const l = el('label', 'switch');
    l.appendChild(el('span', '', label));
    const i = el('input');
    i.type = 'checkbox';
    i.checked = prefs.get(key, dflt);
    i.dataset.pref = key;
    i.onchange = () => { prefs.set(key, i.checked); if (apply) apply(i.checked); };
    l.appendChild(i);
    card.appendChild(l);
  }

  NullRemote.register({
    id: 'settings', title: 'Settings', order: 90,
    mount(root) {
      root.appendChild(header('Settings'));
      const s = el('div', 'scroll');
      const page = el('div', 'page');
      s.appendChild(page);
      root.appendChild(s);

      const screen = el('div', 'card');
      screen.appendChild(el('h2', '', 'Screen'));
      toggle(screen, 'awake', 'Keep the screen on', true, on => NullShell.keepAwake(on));
      toggle(screen, 'fullscreen', 'Full screen', false, on => NullShell.fullscreen(on));
      toggle(screen, 'haptics', 'Vibrate on taps', true);
      page.appendChild(screen);

      const info = NullShell.info();
      const about = el('div', 'card');
      about.appendChild(el('h2', '', 'About'));
      about.appendChild(el('div', '', `${document.title} ${info.version || ''}`));
      about.appendChild(el('div', 'muted', 'Part of NULL JSFX, free and MIT licensed. English only for now.'));
      if (info.updateCheck) {
        toggle(about, 'updates', 'Tell me about new versions', true);
        const st = el('div', 'muted');
        const b = el('button', '', 'Check for updates');
        b.onclick = async () => {
          st.textContent = 'Checking…';
          const f = await NullRemote.checkUpdate(true);
          st.textContent = f && NullRemote.newer(f.version, info.version) ? `${f.version} is available.` : 'You have the latest version.';
        };
        about.appendChild(b);
        about.appendChild(st);
      }
      const site = el('button', '', 'nulljsfx.tech');
      site.onclick = () => NullShell.openUrl('https://nulljsfx.tech/');
      about.appendChild(site);
      page.appendChild(about);
    },
  });
})();
