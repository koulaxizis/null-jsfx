// Remote for DAWs: the REAPER tab. NULL JSFX • https://nulljsfx.tech • MIT License
//
// Connects to REAPER's web interface and then shows the tablet page
// (remote/reaper_www_root/null_jsfx.html, copied into the app at build time as
// daws/reaper/null_jsfx.html) in a frame. The page talks to /_/ on its own origin; the app forwards
// those requests to the REAPER chosen here (NullShell.reaperSet), so the page is the same file
// that REAPER serves to a tablet browser and needs no change for the app.
'use strict';
(function () {
  const { el, prefs, header } = NullRemote;
  const PAGE = '../daws/reaper/null_jsfx.html';
  let root, top, form, frame, msg, found, conn = null;

  function saved() { return prefs.get('reaper-hosts', []); }
  function remember(c) {
    const list = saved().filter(x => !(x.host === c.host && x.port === c.port));
    list.unshift(c);
    prefs.set('reaper-hosts', list.slice(0, 8));
    prefs.set('reaper-last', c);
  }

  function say(text, err) {
    msg.textContent = text || '';
    msg.className = 'msg' + (err ? ' err' : '');
  }

  /** Points the app at [c] and checks that REAPER answers there. */
  async function connect(c) {
    c = { host: String(c.host || '').trim(), port: parseInt(c.port, 10) || 8080, user: c.user || '', pass: c.pass || '', slot: parseInt(c.slot, 10) || 1 };
    if (!c.host) return say('Type the address REAPER shows, or tap Find REAPER.', true);
    say(`Connecting to ${c.host}:${c.port}…`);
    NullShell.reaperSet(c);
    let res;
    const stop = new AbortController();
    const timer = setTimeout(() => stop.abort(), 6000);
    try {
      res = await fetch('/_/TRANSPORT', { cache: 'no-store', signal: stop.signal });
    } catch (e) {
      res = null;
    }
    clearTimeout(timer);
    if (res && res.status === 401) return say('REAPER asks for a user name and password (Preferences > Control/OSC/web).', true);
    const text = res && res.ok ? await res.text() : '';
    if (!text.startsWith('TRANSPORT')) {
      return say(`No answer from REAPER at ${c.host}:${c.port}. Is the web interface on, and are the phone and the computer on the same network?`, true);
    }
    say('');
    conn = c;
    remember(c);
    showPage();
    return true;
  }

  function showPage() {
    form.hidden = true;
    top.hidden = true; // the page has its own NULL JSFX header
    if (frame) frame.remove();
    frame = el('iframe');
    frame.title = 'REAPER';
    frame.src = PAGE + (conn.slot > 1 ? `?slot=${conn.slot}` : '');
    frame.onload = dressPage;
    root.appendChild(frame);
  }

  function disconnect() {
    conn = null;
    prefs.set('reaper-last', null);
    NullShell.reaperSet({});
    if (frame) frame.remove();
    frame = null;
    form.hidden = false;
    top.hidden = false;
    fillSaved();
  }

  /** The app's fonts and taps inside the page (same origin, so the frame is reachable). */
  function dressPage() {
    const d = frame && frame.contentDocument;
    if (!d) return;
    const st = d.createElement('style');
    st.textContent = '@font-face{font-family:"Noto Sans";src:url("../../core/fonts/NotoSans-Regular.ttf") format("truetype");font-weight:400}' +
      '@font-face{font-family:"Noto Sans";src:url("../../core/fonts/NotoSans-Bold.ttf") format("truetype");font-weight:700}';
    d.head.appendChild(st);
    d.addEventListener('pointerdown', e => {
      if (prefs.get('haptics', true) && e.target.closest && e.target.closest('button')) NullShell.haptic();
    }, true);
  }

  function fillSaved() {
    const box = form.querySelector('[data-saved]');
    const list = saved();
    box.parentElement.hidden = !list.length;
    box.textContent = '';
    for (const c of list) {
      const b = el('button');
      b.appendChild(el('span', '', c.host));
      b.appendChild(el('small', '', `port ${c.port}${c.user ? ' · ' + c.user : ''}`));
      b.onclick = () => connect(c);
      box.appendChild(b);
    }
  }

  function field(label, name, type, value, cls) {
    const i = el('input', cls);
    i.name = name;
    i.type = type;
    i.placeholder = label;
    i.setAttribute('aria-label', label);
    i.autocomplete = 'off';
    i.autocapitalize = 'off';
    i.spellcheck = false;
    if (value != null) i.value = value;
    return i;
  }

  async function scan() {
    const port = parseInt(form.querySelector('[name=port]').value, 10) || 8080;
    const b = form.querySelector('[data-scan]');
    b.disabled = true;
    found.textContent = '';
    say(`Looking for REAPER on port ${port}…`);
    const r = await NullShell.scan(port);
    b.disabled = false;
    if (!r.hosts.length) {
      say(`No REAPER found on port ${port}${r.self ? ` near ${r.self}` : ''}. Check the web interface port, or type the address.`, true);
      return;
    }
    say(r.hosts.length === 1 ? 'Found REAPER.' : `Found ${r.hosts.length} computers with REAPER.`);
    for (const h of r.hosts.sort((a, b) => a.host.localeCompare(b.host, 'en', { numeric: true }))) {
      const x = el('button');
      x.appendChild(el('span', '', h.host));
      x.appendChild(el('small', '', h.auth ? 'needs a password' : `port ${port}`));
      x.onclick = () => {
        form.querySelector('[name=host]').value = h.host;
        if (h.auth) { say('Type the user name and password, then Connect.'); form.querySelector('[name=user]').focus(); }
        else connect({ host: h.host, port });
      };
      found.appendChild(x);
    }
  }

  function build() {
    form = el('div', 'scroll');
    const page = el('div', 'page');
    form.appendChild(page);

    const c = el('div', 'card');
    c.appendChild(el('h2', '', 'Connect to REAPER'));
    const last = prefs.get('reaper-last', null) || saved()[0] || {};
    const host = field('Address, e.g. 192.168.1.20', 'host', 'text', last.host);
    host.inputMode = 'decimal';
    const port = field('Port', 'port', 'number', last.port || 8080, 'narrow');
    const r1 = el('div', 'row');
    r1.append(host, port);
    const r2 = el('div', 'row');
    r2.append(field('User (optional)', 'user', 'text', last.user), field('Password', 'pass', 'password', last.pass));
    const r3 = el('div', 'row');
    const go = el('button', 'primary', 'Connect');
    go.dataset.connect = '';
    const sc = el('button', '', 'Find REAPER');
    sc.dataset.scan = '';
    sc.onclick = scan;
    r3.append(sc, go);
    msg = el('div', 'msg');
    found = el('div', 'list');
    c.append(r1, r2, r3, msg, found);
    page.appendChild(c);

    const s = el('div', 'card');
    s.appendChild(el('h2', '', 'Recent'));
    const list = el('div', 'list');
    list.dataset.saved = '';
    s.appendChild(list);
    page.appendChild(s);

    const help = el('div', 'card');
    help.appendChild(el('h2', '', 'Set up REAPER once'));
    help.insertAdjacentHTML('beforeend',
      '<ol>' +
      '<li>In REAPER: <b>Options &gt; Preferences &gt; Control/OSC/web &gt; Add &gt; Web browser interface</b>. Any default interface works; note the port (8080) and the address REAPER shows.</li>' +
      '<li>For the FX: copy <b>null_jsfx_remote_bridge.lua</b> (in the NULL JSFX download, folder Remote/Scripts) into REAPER\'s Scripts folder, and run it from <b>Actions &gt; Show action list &gt; New action &gt; Load ReaScript</b>. The mixer and transport work without it.</li>' +
      '<li>Put the phone on the same Wi-Fi as the computer, and allow REAPER through the computer\'s firewall on private networks.</li>' +
      '</ol>' +
      '<p class="muted">Several phones or tablets at the same time: give each one its own slot (1 to 4).</p>');
    const slot = field('Slot (1 to 4)', 'slot', 'number', last.slot || 1, 'narrow');
    slot.min = 1;
    slot.max = 4;
    const sr = el('div', 'row');
    sr.append(el('span', 'muted', 'Slot for this device'), slot);
    help.appendChild(sr);
    go.onclick = () => connect({
      host: host.value, port: port.value, slot: slot.value,
      user: form.querySelector('[name=user]').value, pass: form.querySelector('[name=pass]').value,
    });
    page.appendChild(help);

    root.appendChild(form);
    fillSaved();
  }

  NullRemote.register({
    id: 'reaper', title: 'REAPER', order: 10, app: 'daws',
    mount(e) {
      root = e;
      top = header('Remote');
      root.appendChild(top);
      build();
      const last = prefs.get('reaper-last', null);
      if (last && last.host) connect(last);
    },
    back() {
      // inside the page: close its FX view first
      const d = frame && frame.contentDocument;
      const fx = d && d.getElementById('fxview');
      if (fx && !fx.hidden) { d.getElementById('fx-back').click(); return true; }
      if (frame) { disconnect(); return true; }
      return false;
    },
    disconnect,
    get connection() { return conn; },
  });
})();
