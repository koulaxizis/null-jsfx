// NULL Remote, Mixers tab: remote control for digital mixers (X AIR first; X32/M32 later as
// profiles). Loads its own files next to it, then registers with the app's tab shell:
//   NullRemote.register({id:'mixers', title:'Mixers', app:'mixers', mount(el), unmount()}).
// Views: Connect (scan, manual IP, how to connect) and Desk (layers of strips, the mix selector
// "Main LR / Bus n / FX n" with sends on faders, mute groups, clear solo, My mix, screen lock).
// NULL JSFX • https://nulljsfx.tech • MIT License
(function () {
  'use strict';

  const here = (document.currentScript && document.currentScript.src) || location.href;
  const FILES = ['core/osc.js', 'core/levels.js', 'core/transport.js', 'core/mixer.js', 'xair/profile.js'];
  const HOLD_MS = 1500;
  const STORE = 'null-remote-mixers';

  function load(src) {
    return new Promise((resolve, reject) => {
      const s = document.createElement('script');
      s.src = new URL(src, here).href;
      s.onload = resolve;
      s.onerror = () => reject(new Error('cannot load ' + src));
      document.head.appendChild(s);
    });
  }
  function css() {
    if (document.querySelector('link[data-nm]')) return;
    const l = document.createElement('link');
    l.rel = 'stylesheet';
    l.href = new URL('mixers.css', here).href;
    l.dataset.nm = '1';
    document.head.appendChild(l);
  }
  const ready = FILES.reduce((p, f) => p.then(() => load(f)), Promise.resolve());

  const store = {
    get() { try { return JSON.parse(localStorage.getItem(STORE)) || {}; } catch (_) { return {}; } },
    set(patch) { try { localStorage.setItem(STORE, JSON.stringify(Object.assign(this.get(), patch))); } catch (_) { /* private mode */ } },
  };
  const shell = () => window.NullShell || {};
  const haptic = () => { try { shell().haptic && shell().haptic(); } catch (_) { /* optional */ } };
  const keepAwake = (on) => { try { shell().keepAwake && shell().keepAwake(on); } catch (_) { /* optional */ } };

  function h(tag, attrs, ...kids) {
    const el = document.createElement(tag);
    for (const [k, v] of Object.entries(attrs || {})) {
      if (k === 'class') el.className = v;
      else if (k.startsWith('on')) el.addEventListener(k.slice(2), v);
      else if (v !== false && v != null) el.setAttribute(k, v === true ? '' : v);
    }
    for (const k of kids.flat()) if (k != null) el.append(k.nodeType ? k : document.createTextNode(k));
    return el;
  }

  // A button that acts only after being held, so a stray touch on stage cannot trigger it.
  function holdButton(label, cls, onHeld) {
    const b = h('button', { class: 'nm-hold ' + (cls || '') }, h('span', null, label));
    let t = null;
    const stop = () => { clearTimeout(t); t = null; b.classList.remove('holding'); };
    b.addEventListener('pointerdown', () => {
      b.classList.add('holding');
      t = setTimeout(() => { stop(); haptic(); onHeld(); }, HOLD_MS);
    });
    ['pointerup', 'pointerleave', 'pointercancel'].forEach((e) => b.addEventListener(e, stop));
    b.setLabel = (s) => { b.firstChild.textContent = s; };
    return b;
  }

  // Relative drag: the value moves with the finger from where it was, never jumps to the touch.
  function dragControl(el, { vertical, get, set, span, onDouble }) {
    let start = null;
    let lastTap = 0;
    el.addEventListener('pointerdown', (e) => {
      if (onDouble && e.timeStamp - lastTap < 300) { onDouble(); lastTap = 0; return; }
      lastTap = e.timeStamp;
      el.setPointerCapture(e.pointerId);
      start = { x: e.clientX, y: e.clientY, v: get() };
      el.classList.add('dragging');
    });
    el.addEventListener('pointermove', (e) => {
      if (!start) return;
      const px = span();
      const d = vertical ? (start.y - e.clientY) / px : (e.clientX - start.x) / px;
      set(Math.min(1, Math.max(0, start.v + d)));
    });
    const end = () => { start = null; el.classList.remove('dragging'); };
    el.addEventListener('pointerup', end);
    el.addEventListener('pointercancel', end);
  }

  class MixersTab {
    constructor(root) {
      this.root = root;
      this.session = null;
      this.transport = null;
      this.offs = [];
      this.view = store.get();
      this.view.layer = this.view.layer || 'inputs';
      this.view.mix = this.view.mix || 'main';
      this.meters = null;
      this.raf = 0;
    }

    get profile() { return window.NullMixerProfiles.xair; }

    // ---- connect view -------------------------------------------------------------------------
    showConnect(error) {
      this.teardown();
      const P = this.profile;
      const found = h('div', { class: 'nm-found' });
      const err = h('div', { class: 'nm-error' }, error || '');
      const ip = h('input', { type: 'text', inputmode: 'decimal', placeholder: 'Mixer IP address, e.g. 192.168.1.1', value: this.view.host || '' });
      const scan = h('button', { onclick: () => this.scan(found, err, scan) }, 'Scan');
      const go = h('button', { onclick: () => this.connect(ip.value.trim(), err) }, 'Connect');
      const ui = h('div', { class: 'nm-connect' },
        h('h2', null, 'MIXERS'),
        h('div', { class: 'nm-sub' }, 'Works with ' + P.family + ' mixers: ' + P.models.filter((m) => !m.startsWith('MR')).join(', ') + '.'),
        h('div', { class: 'nm-row' }, scan),
        found,
        h('div', { class: 'nm-row' }, ip, go),
        err,
        h('h2', { style: 'margin-top:16px' }, 'HOW TO CONNECT'),
        h('ol', { class: 'nm-help' }, P.connectHelp.map((t) => h('li', null, t))),
        h('div', { class: 'nm-note' }, 'X AIR, X32 and M32 are trademarks of their owners. Remote for Mixers is an independent free app from NULL JSFX and is not made or endorsed by them.'));
      this.root.replaceChildren(ui);
      this.root.className = 'nm';
      if (this.transportFactory || window.NullMixerTransport.available()) this.scan(found, err, scan);
      else err.textContent = 'Open this in the Remote for Mixers app: a browser cannot reach the mixer.';
    }

    async ensureTransport() {
      if (!this.transport) this.transport = await (this.transportFactory || window.NullMixerTransport.openShellUdp)(0);
      return this.transport;
    }

    async scan(list, err, btn) {
      btn.disabled = true;
      btn.textContent = 'Scanning…';
      try {
        const tr = await this.ensureTransport();
        const consoles = await window.NullMixer.discover(this.profile, tr, 1500);
        list.replaceChildren(...consoles.map((c) => h('button', { onclick: () => this.connect(c.host, err) },
          h('b', null, c.name || c.model), h('span', null, c.model + ' • ' + c.host + (c.version ? ' • firmware ' + c.version : '')))));
        if (!consoles.length) err.textContent = 'No mixer answered. Check the network, or type its IP address.';
        else err.textContent = '';
      } catch (e) {
        err.textContent = e.message;
      }
      btn.disabled = false;
      btn.textContent = 'Scan';
    }

    async connect(host, err) {
      if (!/^[\w.-]+$/.test(host)) { err.textContent = 'Type the mixer’s IP address.'; return; }
      err.textContent = 'Connecting to ' + host + '…';
      try {
        const tr = await this.ensureTransport();
        const s = new window.NullMixer.Session(this.profile, tr);
        await s.connect(host, this.profile.port);
        this.session = s;
        this.view.host = host;
        store.set({ host });
        keepAwake(true);
        this.showDesk();
      } catch (e) {
        err.textContent = e.message + '. Is the phone on the mixer’s network?';
      }
    }

    teardown() {
      for (const off of this.offs) off();
      this.offs = [];
      cancelAnimationFrame(this.raf);
      this.raf = 0;
    }

    disconnect() {
      this.teardown();
      if (this.session) this.session.disconnect();
      this.session = null;
      keepAwake(false);
      this.showConnect();
    }

    // ---- desk view ----------------------------------------------------------------------------
    showDesk() {
      this.teardown();
      const s = this.session;
      const L = s.layout;
      const v = this.view;
      if (v.mymix && !L.targets.some((t) => t.id === v.mymix)) v.mymix = null;
      if (!L.targets.some((t) => t.id === v.mix)) v.mix = 'main';
      this.root.className = 'nm' + (v.mymix ? ' mymix' : '') + (v.locked ? ' locked' : '');
      const bar = h('div', { class: 'nm-bar' });
      const banner = h('div', { class: 'nm-banner' }, 'Connection lost. Trying again…');
      const strips = h('div', { class: 'nm-strips' });
      const master = h('div', { class: 'nm-master' });
      this.root.replaceChildren(bar, banner, h('div', { class: 'nm-desk' }, strips, master), h('div', { class: 'nm-lockveil' }));

      // console status
      const dot = h('span', { class: 'nm-dot ok' });
      bar.append(h('div', { class: 'nm-console' }, dot, (s.info.name || L.model) + ' • ' + L.model));
      this.offs.push(s.onStatus((st) => {
        dot.className = 'nm-dot' + (st === 'online' ? ' ok' : st === 'lost' ? ' lost' : '');
        banner.classList.toggle('show', st === 'lost');
      }));

      const lock = holdButton(v.locked ? 'Unlock (hold)' : 'Lock', 'locked', () => { v.locked = !v.locked; store.set({ locked: v.locked }); this.showDesk(); });
      lock.style.position = 'relative';
      lock.style.zIndex = 6;

      if (v.mymix) {
        const t = L.targets.find((x) => x.id === v.mymix);
        const exit = holdButton('Leave My mix (hold)', '', () => { v.mymix = null; store.set({ mymix: null }); this.showDesk(); });
        bar.append(lock, exit);
        this.renderStrips(strips, master, L.inputs.concat(L.returns), t, true);
      } else {
        bar.append(lock, h('button', { onclick: () => this.pickMyMix() }, 'My mix'));
        // layers
        const layers = [['inputs', 'Inputs'], ['returns', 'FX Returns'], ['masters', 'Buses'], ['dca', 'DCA']];
        bar.append(h('span', { class: 'nm-sep' }));
        for (const [id, label] of layers) {
          bar.append(h('button', { class: v.layer === id ? 'on' : '', onclick: () => { v.layer = id; store.set({ layer: id }); this.showDesk(); } }, label));
        }
        // mix selector: what the faders control
        bar.append(h('span', { class: 'nm-sep' }));
        const mixes = [{ id: 'main', label: 'Main LR' }].concat(L.targets.map((t) => ({ id: t.id, label: this.targetName(t) })));
        for (const m of mixes) {
          bar.append(h('button', { class: v.mix === m.id ? 'on' : '', onclick: () => { v.mix = m.id; store.set({ mix: m.id }); this.showDesk(); } }, m.label));
        }
        // mute groups and clear solo
        bar.append(h('span', { class: 'nm-sep' }));
        for (const g of L.muteGroups) {
          const b = h('button', { class: 'mg', onclick: () => { haptic(); s.set(g.path, s.get(g.path) ? 0 : 1); } }, g.label);
          const paint = () => b.classList.toggle('on', !!s.get(g.path));
          paint();
          this.offs.push(s.on(g.path, () => { paint(); this.paintGroupMutes(); }));
          bar.append(b);
        }
        const clear = h('button', { class: 'clear-solo', onclick: () => { haptic(); s.action(...L.clearSolo); } }, 'Clear solo');
        const soloPaths = L.inputs.concat(L.returns, L.targets.map((t) => t.master), [L.main]).map((x) => x.solo).filter(Boolean);
        const paintClear = () => { clear.style.display = soloPaths.some((p) => s.get(p)) ? '' : 'none'; };
        paintClear();
        for (const p of soloPaths) this.offs.push(s.on(p, paintClear));
        bar.append(clear);
        bar.append(h('span', { class: 'nm-sep' }), h('button', { onclick: () => this.disconnect() }, 'Disconnect'));

        const target = v.mix === 'main' ? null : L.targets.find((t) => t.id === v.mix);
        let list;
        if (v.layer === 'inputs') list = L.inputs;
        else if (v.layer === 'returns') list = L.returns;
        else if (v.layer === 'masters') list = L.targets.map((t) => t.master);
        else list = L.dcas;
        const sendable = v.layer === 'inputs' || v.layer === 'returns';
        this.renderStrips(strips, master, list, sendable ? target : null, false, target);
      }

      // meters
      this.offs.push(s.onMeters((addr, db) => { if (addr === L.meterAddress) this.meters = db; }));
      const tick = () => {
        this.raf = requestAnimationFrame(tick);
        const m = this.meters;
        if (!m) return;
        for (const el of this.root.querySelectorAll('.nm-meter[data-i]')) {
          const db = m[+el.dataset.i];
          const pos = db == null ? 0 : window.NullLevels.meterPos(db);
          const bar = el.firstChild;
          const prev = +bar.dataset.p || 0;
          const shown = pos >= prev ? pos : Math.max(pos, prev - 0.02); // fall back gently
          bar.dataset.p = shown;
          bar.style.height = (shown * 100).toFixed(1) + '%';
          bar.style.setProperty('--h', el.clientHeight + 'px');
        }
      };
      this.raf = requestAnimationFrame(tick);
      this.paintGroupMutes();
    }

    targetName(t) {
      const n = this.session.get(t.master.name);
      return n ? t.label + ' ' + n : t.label;
    }

    // strips: the faders of `list`; with `sendTo`, each fader is that strip's send to the target.
    // The master section shows the target's master (or Main LR).
    renderStrips(strips, master, list, sendTo, mymix, shownTarget) {
      for (const st of list) strips.append(this.strip(st, sendTo));
      const t = sendTo || shownTarget;
      master.append(this.strip(t ? t.master : this.session.layout.main, null, true));
      if (mymix) master.firstChild.querySelector('.nm-btns').remove();
    }

    strip(st, sendTo, isMaster) {
      const s = this.session;
      const L = window.NullLevels;
      const P = this.profile;
      const faderPath = sendTo ? st.sends && st.sends[sendTo.id] : st.fader;
      const el = h('div', { class: 'nm-strip' + (sendTo ? ' sends' : '') + (isMaster ? ' master' : '') });
      if (!faderPath) return el;

      // name and colour
      const name = h('div', { class: 'nm-name' });
      const paintName = () => {
        name.textContent = s.get(st.name) || st.label;
        const c = s.get(st.color);
        name.style.setProperty('--strip-color', P.colorOf(c));
        name.classList.toggle('inv', P.colorInverted(c));
      };
      paintName();
      this.offs.push(s.on(st.name, paintName), s.on(st.color, paintName));

      // pan (hidden on sends and where the strip has none)
      const pan = h('div', { class: 'nm-pan' + (st.pan && !sendTo ? '' : ' none') }, h('i'), h('b'));
      if (st.pan && !sendTo) {
        const paintPan = () => {
          const p = s.get(st.pan) ?? 0.5;
          const i = pan.firstChild;
          i.style.left = (Math.min(p, 0.5) * 100) + '%';
          i.style.width = Math.abs(p - 0.5) * 100 + '%';
          pan.lastChild.textContent = L.formatPan(p);
        };
        paintPan();
        this.offs.push(s.on(st.pan, paintPan));
        dragControl(pan, { vertical: false, get: () => s.get(st.pan) ?? 0.5, set: (x) => s.set(st.pan, x), span: () => pan.clientWidth * 1.5, onDouble: () => s.set(st.pan, 0.5) });
      }

      // fader with meter
      const meter = h('div', { class: 'nm-meter' }, h('i'));
      if (st.meter != null && !sendTo) meter.dataset.i = st.meter;
      const cap = h('div', { class: 'cap' });
      const fader = h('div', { class: 'nm-fader' }, h('div', { class: 'track' }), h('div', { class: 'unity' }), cap);
      const db = h('div', { class: 'nm-db' });
      const travel = () => Math.max(1, fader.clientHeight - 24);
      const paintFader = () => {
        const f = s.get(faderPath) ?? 0;
        cap.style.top = (12 + (1 - f) * travel()) + 'px';
        fader.children[1].style.top = (12 + 0.25 * travel()) + 'px';
        db.textContent = L.formatDb(L.faderToDb(f));
      };
      this.offs.push(s.on(faderPath, paintFader));
      requestAnimationFrame(paintFader);
      const ro = new ResizeObserver(paintFader);
      ro.observe(fader);
      this.offs.push(() => ro.disconnect());
      dragControl(fader, { vertical: true, get: () => s.get(faderPath) ?? 0, set: (x) => s.set(faderPath, x), span: travel });

      // mute and solo
      const btns = h('div', { class: 'nm-btns' });
      if (st.on) {
        const mute = h('button', { class: 'mute', onclick: () => { haptic(); s.set(st.on, s.get(st.on) ? 0 : 1); } }, 'M');
        mute.dataset.groups = st.muteGroups || '';
        const paintMute = () => mute.classList.toggle('on', s.get(st.on) === 0); // on = 1 means unmuted
        paintMute();
        this.offs.push(s.on(st.on, paintMute));
        if (st.muteGroups) this.offs.push(s.on(st.muteGroups, () => this.paintGroupMutes()));
        btns.append(mute);
      }
      if (st.solo) {
        const solo = h('button', { class: 'solo', onclick: () => { haptic(); s.set(st.solo, s.get(st.solo) ? 0 : 1); } }, 'S');
        const paintSolo = () => solo.classList.toggle('on', !!s.get(st.solo));
        paintSolo();
        this.offs.push(s.on(st.solo, paintSolo));
        btns.append(solo);
      }

      el.append(name, pan, h('div', { class: 'nm-fadebox' }, meter, fader), db, btns);
      return el;
    }

    // A strip in an active mute group shows a red outline on its M button (bit n-1 = group n,
    // UNVERIFIED on hardware).
    paintGroupMutes() {
      const s = this.session;
      if (!s) return;
      let active = 0;
      for (const g of s.layout.muteGroups) if (s.get(g.path)) active |= g.bit;
      for (const b of this.root.querySelectorAll('.nm-btns .mute')) {
        const p = b.dataset.groups;
        b.classList.toggle('grp', !!(p && (s.get(p) & active)));
      }
    }

    pickMyMix() {
      const L = this.session.layout;
      const sheet = h('div', { class: 'nm-sheet' });
      const close = () => sheet.remove();
      sheet.append(h('div', null,
        h('h2', null, 'MY MIX'),
        h('div', { class: 'nm-sub' }, 'For a musician: this device then shows only the sends to one monitor bus and its master. Hold the button at the top to leave.'),
        h('div', { class: 'nm-grid' }, L.targets.filter((t) => t.master.kind === 'bus').map((t) => h('button', {
          onclick: () => { this.view.mymix = t.id; store.set({ mymix: t.id }); close(); this.showDesk(); },
        }, this.targetName(t)))),
        h('button', { onclick: close }, 'Cancel')));
      this.root.append(sheet);
    }

    mount() {
      css();
      return ready.then(() => { if (this.session) this.showDesk(); else this.showConnect(); });
    }

    unmount() {
      this.teardown();
      keepAwake(false);
    }
  }

  let tab = null;
  const api = {
    id: 'mixers',
    title: 'Mixers',
    app: 'mixers', // shown only in the mixers app of the shell (flavor "mixers")
    mount(el) { tab = tab && tab.root === el ? tab : new MixersTab(el); return tab.mount(); },
    unmount() { if (tab) tab.unmount(); },
    // for tests and the desktop harness: a transport factory replacing the app's UDP bridge
    useTransport(factory) { MixersTab.prototype.transportFactory = factory; },
  };
  window.NullMixersTab = api;
  if (window.NullRemote && window.NullRemote.register) window.NullRemote.register(api);
})();
