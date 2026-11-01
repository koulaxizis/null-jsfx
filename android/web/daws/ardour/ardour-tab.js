// Remote for DAWs: the Ardour tab (Ardour and Harrison Mixbus). NULL JSFX • https://nulljsfx.tech • MIT License
//
// Connect (address of the computer, OSC port 3819), then the desk: transport and clock, one
// strip per track and bus (meter, fader, mute, solo, record, pan) and the plugins of each strip.
// A NULL JSFX plugin opens with its own interface, drawn by the web version of its GFX
// (web/fx/<id>.js and web/runtime, copied into the app as nullfx/ at build time); any other
// plugin gets a list of its parameters. The protocol is in ardour.js.
'use strict';
(function () {
  const { el, prefs, header } = NullRemote;
  const BASE = '../daws/ardour/';
  const NULLFX = '../nullfx/';
  const FILES = ['../mixers/core/osc.js', '../mixers/core/transport.js', BASE + 'ardour.js'];
  const RUNTIME = ['runtime/eel-rt.js', 'runtime/null-fx.js', 'runtime/null-gfx.js'];

  let root, connectView, desk, msg, session = null, conn = null;
  let strips, clock, transportBtns, sheet, sheetBody, sheetTitle;
  let effects = null; // web/fx/index.json effects
  let view = null; // the open plugin {ssid, piid, kind, ...}
  const loaded = new Map();

  function load(src) {
    if (!loaded.has(src)) {
      loaded.set(src, new Promise((resolve, reject) => {
        const s = document.createElement('script');
        s.src = src;
        s.onload = resolve;
        s.onerror = () => { loaded.delete(src); reject(new Error('cannot load ' + src)); };
        document.head.appendChild(s);
      }));
    }
    return loaded.get(src);
  }
  async function loadAll(list) { for (const f of list) await load(f); }

  async function nullIndex() {
    if (effects) return effects;
    try {
      const res = await fetch(NULLFX + 'fx/index.json');
      effects = res.ok ? (await res.json()).effects : [];
    } catch (e) { effects = []; }
    return effects;
  }

  function say(text, err) {
    msg.textContent = text || '';
    msg.className = 'msg' + (err ? ' err' : '');
  }

  // --- connect -------------------------------------------------------------------------------

  function saved() { return prefs.get('ardour-hosts', []); }
  function remember(c) {
    const list = saved().filter((x) => !(x.host === c.host && x.port === c.port));
    list.unshift(c);
    prefs.set('ardour-hosts', list.slice(0, 8));
  }

  function buildConnect() {
    connectView = el('div', 'scroll');
    const page = el('div', 'page');
    const card = el('div', 'card');
    card.appendChild(el('h2', '', 'Connect to Ardour or Mixbus'));
    const last = saved()[0] || {};
    const row = el('div', 'row');
    const host = el('input');
    host.placeholder = 'Computer address, e.g. 192.168.1.20';
    host.inputMode = 'decimal';
    host.value = last.host || '';
    host.setAttribute('aria-label', 'Computer address');
    const port = el('input', 'narrow');
    port.inputMode = 'numeric';
    port.value = last.port || 3819;
    port.setAttribute('aria-label', 'OSC port');
    row.append(host, port);
    const go = el('button', 'primary', 'Connect');
    go.onclick = () => connect({ host: host.value, port: port.value });
    msg = el('div', 'msg');
    card.append(row, go, msg);
    const list = el('div', 'list');
    for (const c of saved()) {
      const b = el('button');
      b.append(el('span', '', c.host), el('small', '', 'port ' + c.port));
      b.onclick = () => connect(c);
      list.appendChild(b);
    }
    if (list.childElementCount) card.appendChild(list);

    const help = el('div', 'card');
    help.appendChild(el('h2', '', 'Set up Ardour or Mixbus (once)'));
    const ol = el('ol');
    for (const step of [
      ['Edit > Preferences > Control Surfaces: tick ', 'Open Sound Control (OSC)', '.'],
      ['Double-click it (or Show Protocol Settings): ', 'Port Mode Auto', ', port 3819.'],
      ['Load the NULL JSFX plugins as ', 'VST3 or LV2', '; they open here with their own interface.'],
      ['Put the phone on the same network and type the computer\'s address above. Allow ', 'UDP 3819', ' through its firewall.'],
    ]) {
      const li = el('li');
      li.append(step[0], el('b', '', step[1]), step[2]);
      ol.appendChild(li);
    }
    help.appendChild(ol);
    page.append(card, help);
    connectView.appendChild(page);
    return connectView;
  }

  async function connect(c) {
    c = { host: String(c.host || '').trim(), port: parseInt(c.port, 10) || 3819 };
    if (!c.host) return say('Type the address of the computer that runs Ardour.', true);
    say(`Connecting to ${c.host}:${c.port}…`);
    try {
      await loadAll(FILES);
    } catch (e) {
      return say(e.message, true);
    }
    let transport;
    try {
      transport = await NullMixerTransport.openShellUdp(0);
    } catch (e) {
      return say(NullShell.native ? e.message : 'This tab needs the Remote for DAWs app (no network access in a browser).', true);
    }
    if (session) session.close();
    session = new NullArdour.Session({ transport, host: c.host, port: c.port });
    let answered = false;
    session.on('strips', (list) => {
      if (!answered) {
        answered = true;
        conn = c;
        remember(c);
        say('');
        showDesk();
      }
      drawStrips(list);
    });
    session.on('strip', updateStrip);
    session.on('meter', updateMeter);
    session.on('transport', updateTransport);
    session.on('time', (t) => { if (clock) clock.textContent = t || '--:--:--'; });
    session.on('link', (up) => { if (desk) desk.classList.toggle('da-down', !up); });
    session.on('plugins', (ssid, list) => { if (view && view.kind === 'list' && view.ssid === ssid) drawPluginList(ssid, list); });
    session.on('params', onParams);
    session.connect();
    setTimeout(() => {
      if (!answered && session) {
        say(`No answer from ${c.host}:${c.port}. Is OSC on in Ardour (Preferences > Control Surfaces), and are the phone and the computer on the same network?`, true);
      }
    }, 4000);
    return true;
  }

  function disconnect() {
    closeSheet();
    if (session) session.close();
    session = null;
    conn = null;
    desk.hidden = true;
    connectView.hidden = false;
  }

  // --- desk ----------------------------------------------------------------------------------

  function buildDesk() {
    desk = el('div', 'da-desk');
    desk.hidden = true;
    const bar = el('div', 'da-bar');
    clock = el('div', 'da-clock', '--:--:--');
    transportBtns = {};
    const t = el('div', 'da-transport');
    for (const [k, label, fn] of [
      ['start', '⏮', () => session.toStart()], ['stop', '■', () => session.stop()],
      ['play', '▶', () => session.play()], ['rec', '●', () => session.record()], ['loop', '⟲', () => session.loop()],
    ]) {
      const b = el('button', 'da-t da-' + k, label);
      b.setAttribute('aria-label', { start: 'To start', stop: 'Stop', play: 'Play', rec: 'Record', loop: 'Loop' }[k]);
      b.onclick = () => { NullShell.haptic(); fn(); };
      transportBtns[k] = b;
      t.appendChild(b);
    }
    const leave = el('button', 'da-leave', '⏏');
    leave.setAttribute('aria-label', 'Disconnect');
    leave.onclick = disconnect;
    bar.append(clock, t, leave);
    strips = el('div', 'da-strips');
    sheet = el('div', 'da-sheet');
    sheet.hidden = true;
    const sh = el('div', 'da-sheet-top');
    const back = el('button', 'da-back', '‹');
    back.setAttribute('aria-label', 'Back');
    back.onclick = () => goBack();
    sheetTitle = el('div', 'da-sheet-title');
    sh.append(back, sheetTitle);
    sheetBody = el('div', 'da-sheet-body');
    sheet.append(sh, sheetBody);
    desk.append(bar, strips, sheet);
    return desk;
  }

  function showDesk() {
    connectView.hidden = true;
    desk.hidden = false;
  }

  const typeName = { AT: 'Audio', MT: 'MIDI', B: 'Bus', MB: 'MIDI bus', FB: 'Foldback', V: 'VCA', MA: 'Master', MO: 'Monitor' };

  function drawStrips(list) {
    strips.textContent = '';
    for (const s of list) {
      const st = el('div', 'da-strip' + (s.type === 'MA' ? ' da-master' : ''));
      st.dataset.ssid = s.ssid;
      const name = el('div', 'da-name', s.name);
      const kind = el('div', 'da-kind', typeName[s.type] || s.type);
      const fx = el('button', 'da-fx', 'FX');
      fx.onclick = () => openPlugins(s.ssid);
      const pan = el('div', 'da-pan');
      pan.appendChild(el('div', 'da-pan-dot'));
      relativeDrag(pan, 'x', () => s.pan ?? 0.5, (v) => { session.setPan(s.ssid, v); updateStrip(s); }, () => { session.setPan(s.ssid, 0.5); updateStrip(s); });
      const lane = el('div', 'da-lane');
      const meter = el('div', 'da-meter');
      meter.appendChild(el('div', 'da-meter-fill'));
      const fader = el('div', 'da-fader');
      fader.appendChild(el('div', 'da-cap'));
      relativeDrag(fader, 'y', () => s.fader ?? NullArdour.dbToFader(0), (v) => { session.setFader(s.ssid, v); updateStrip(s); },
        () => { session.setFader(s.ssid, NullArdour.dbToFader(0)); updateStrip(s); });
      lane.append(meter, fader);
      const db = el('div', 'da-db');
      const btns = el('div', 'da-btns');
      const mute = el('button', 'da-m', 'M');
      mute.onclick = () => { session.setMute(s.ssid, !s.mute); updateStrip(s); };
      const solo = el('button', 'da-s', 'S');
      solo.onclick = () => { session.setSolo(s.ssid, !s.solo); updateStrip(s); };
      btns.append(mute, solo);
      if (s.canRec) {
        const rec = el('button', 'da-r', 'R');
        rec.onclick = () => { session.setRec(s.ssid, !s.rec); updateStrip(s); };
        btns.appendChild(rec);
      }
      if (s.type === 'MA' || s.type === 'V') fx.hidden = s.type === 'V';
      if (s.type === 'V') pan.style.visibility = 'hidden';
      st.append(name, kind, fx, pan, lane, db, btns);
      strips.appendChild(st);
      updateStrip(s);
    }
    if (!list.length) strips.appendChild(el('div', 'empty', 'The session has no tracks yet.'));
  }

  function stripEl(ssid) { return strips.querySelector(`.da-strip[data-ssid="${ssid}"]`); }

  function updateStrip(s) {
    const st = stripEl(s.ssid);
    if (!st) return;
    st.querySelector('.da-name').textContent = s.name;
    const pos = s.fader ?? NullArdour.dbToFader(0);
    st.querySelector('.da-cap').style.bottom = `calc(${(pos * 100).toFixed(2)}% - ${(pos * 24).toFixed(1)}px)`;
    const db = s.gain != null && !(Date.now() - (session.touched.get(s.ssid + '/fader') || 0) < 500) ? s.gain : NullArdour.faderToDb(pos);
    st.querySelector('.da-db').textContent = db > -120 ? (db > 0 ? '+' : '') + db.toFixed(1) + ' dB' : '-∞';
    const pan = s.pan ?? 0.5;
    st.querySelector('.da-pan-dot').style.left = (pan * 100).toFixed(1) + '%';
    st.querySelector('.da-m').classList.toggle('on', !!s.mute);
    st.querySelector('.da-s').classList.toggle('on', !!s.solo);
    const r = st.querySelector('.da-r');
    if (r) r.classList.toggle('on', !!s.rec);
  }

  function updateMeter(s) {
    const st = stripEl(s.ssid);
    if (!st) return;
    // v = (dB + 94) / 100; show -60..+6 dB
    const db = s.meter * 100 - 94;
    const h = Math.max(0, Math.min(1, (db + 60) / 66));
    const fill = st.querySelector('.da-meter-fill');
    fill.style.height = (h * 100).toFixed(1) + '%';
    fill.classList.toggle('hot', db > -0.5);
  }

  function updateTransport(t) {
    transportBtns.play.classList.toggle('on', t.play);
    transportBtns.stop.classList.toggle('on', t.stop);
    transportBtns.rec.classList.toggle('on', t.rec);
    transportBtns.loop.classList.toggle('on', t.loop);
  }

  /**
   * Moves a control relative to the finger, so touching it never makes it jump; a double tap
   * resets it. axis 'y' for faders (up = more), 'x' for pan.
   */
  function relativeDrag(target, axis, get, set, reset) {
    let start = null;
    let lastTap = 0;
    target.style.touchAction = 'none';
    target.addEventListener('pointerdown', (e) => {
      const now = Date.now();
      if (now - lastTap < 300) { reset(); lastTap = 0; NullShell.haptic(); return; }
      lastTap = now;
      try { target.setPointerCapture(e.pointerId); } catch (x) { /* old WebView */ }
      const r = target.getBoundingClientRect();
      start = { p: axis === 'y' ? e.clientY : e.clientX, v: get(), size: axis === 'y' ? r.height : r.width };
      e.preventDefault();
    });
    target.addEventListener('pointermove', (e) => {
      if (!start) return;
      const d = (axis === 'y' ? start.p - e.clientY : e.clientX - start.p) / Math.max(40, start.size);
      set(Math.min(1, Math.max(0, start.v + d)));
    });
    const end = () => { start = null; };
    target.addEventListener('pointerup', end);
    target.addEventListener('pointercancel', end);
  }

  // --- plugins -------------------------------------------------------------------------------

  function openSheet(title) {
    sheetTitle.textContent = title;
    sheetBody.textContent = '';
    sheet.hidden = false;
  }
  function closeSheet() {
    closePlugin();
    view = null;
    if (sheet) sheet.hidden = true;
  }
  function goBack() {
    if (view && view.kind === 'plugin') { const ssid = view.ssid; closePlugin(); openPlugins(ssid); return true; }
    if (view) { closeSheet(); return true; }
    return false;
  }

  function stripName(ssid) { const s = session && session.strips.get(ssid); return s ? s.name : 'Track'; }

  async function openPlugins(ssid) {
    await nullIndex();
    view = { kind: 'list', ssid };
    openSheet(stripName(ssid) + ' · Plugins');
    sheetBody.appendChild(el('div', 'empty', 'Reading the plugins…'));
    session.listPlugins(ssid);
  }

  function drawPluginList(ssid, list) {
    sheetBody.textContent = '';
    if (!list.length) {
      sheetBody.appendChild(el('div', 'empty', 'No plugins on this track. Add the NULL JSFX plugins in Ardour (VST3 or LV2) and they show here.'));
      return;
    }
    const box = el('div', 'list da-plugins');
    for (const p of list) {
      const fx = NullArdour.nullEffect(p.name, effects);
      const row = el('div', 'da-plugin');
      const open = el('button', 'da-plugin-open');
      const label = el('span', '', p.name);
      open.append(label, el('small', '', fx ? 'NULL JSFX' : 'parameters'));
      open.onclick = () => openPlugin(ssid, p, fx);
      const on = el('button', 'da-onoff' + (p.enabled ? ' on' : ''), p.enabled ? 'ON' : 'OFF');
      on.setAttribute('aria-label', (p.enabled ? 'Bypass ' : 'Activate ') + p.name);
      on.onclick = () => {
        session.setPluginActive(ssid, p.piid, !p.enabled);
        on.classList.toggle('on', p.enabled);
        on.textContent = p.enabled ? 'ON' : 'OFF';
      };
      row.append(open, on);
      box.appendChild(row);
    }
    sheetBody.appendChild(box);
  }

  async function openPlugin(ssid, p, fx) {
    view = { kind: 'plugin', ssid, piid: p.piid, name: p.name, fx, params: null, map: null, gfx: null, controls: new Map() };
    openSheet(stripName(ssid) + ' · ' + p.name);
    sheetBody.appendChild(el('div', 'empty', 'Reading the parameters…'));
    if (fx) {
      try {
        await loadAll(RUNTIME.map((f) => NULLFX + f));
        await load(NULLFX + 'fx/' + fx.id + '.js');
        view.FX = globalThis.NULLFX && globalThis.NULLFX[fx.id];
      } catch (e) {
        view.FX = null; // no web version: the parameter list instead
      }
    }
    session.watchPlugin(ssid, p.piid);
  }

  function closePlugin() {
    if (session) session.watchPlugin(null);
    if (view && view.gfx) {
      cancelAnimationFrame(view.frame);
      view.gfx.destroy();
    }
    if (view && view.kind === 'plugin') view.gfx = null;
  }

  function onParams(ssid, piid, params) {
    if (!view || view.kind !== 'plugin' || view.ssid !== ssid || view.piid !== piid) return;
    const first = !view.params;
    view.params = params;
    if (view.FX) {
      if (first) {
        view.map = NullArdour.mapSliders(view.FX.sliders, params);
        drawNull();
      } else {
        followNull();
      }
    } else if (first) {
      drawGeneric();
    } else {
      followGeneric();
    }
  }

  // a NULL JSFX plugin: its own interface, from the web version of its GFX
  // the newest read of the parameter a slider is paired with (each re-read brings fresh objects)
  function paramOf(i) {
    const first = view.map.get(i);
    return first && (view.params.find((q) => q.par === first.par) || first);
  }

  function sliderValues() {
    const out = {};
    for (const s of view.FX.sliders) {
      const p = paramOf(s.i);
      if (p) out[s.i] = NullArdour.fromParam(p.value, s.min, s.max, p);
    }
    return out;
  }

  function drawNull() {
    sheetBody.textContent = '';
    const FX = view.FX;
    const wrap = el('div', 'da-gfx');
    const cv = document.createElement('canvas');
    const [w, h] = [FX.gfx ? FX.gfx.w : 360, FX.gfx ? FX.gfx.h : 240];
    wrap.style.aspectRatio = `${w} / ${h}`;
    wrap.style.maxWidth = `${Math.round(w * 2)}px`;
    wrap.appendChild(cv);
    sheetBody.appendChild(wrap);
    const vals = sliderValues();
    view.shown = vals;
    const v = view;
    view.gfx = new NULLGfx.View(FX, cv, {
      srate: session.rate || 48000,
      sliders: vals,
      onSlider(i, value) {
        const s = FX.sliders.find((x) => x.i === i);
        const p = view === v ? paramOf(i) : null;
        if (!s || !p) return;
        v.shown[i] = value;
        session.setParam(v.ssid, v.piid, p.par, NullArdour.toParam(value, s.min, s.max, p));
      },
    });
    const loop = () => {
      if (view !== v || !v.gfx) return;
      v.gfx.draw();
      v.frame = requestAnimationFrame(loop);
    };
    v.frame = requestAnimationFrame(loop);
    const note = el('div', 'muted da-note', 'Drag the controls as in the plugin. Changes made in Ardour show here within a second.');
    sheetBody.appendChild(note);
  }

  function followNull() {
    if (!view.gfx) return;
    const vals = sliderValues();
    for (const s of view.FX.sliders) {
      const v = vals[s.i];
      if (v == null || Math.abs(v - (view.shown[s.i] ?? NaN)) <= 1e-6 * Math.max(1, Math.abs(s.max - s.min))) continue;
      view.shown[s.i] = v;
      view.gfx.setSlider(s.i, v);
    }
  }

  // any other plugin: one row per input parameter
  function fmt(p, v) {
    const pt = p.points.find((x) => Math.abs(x[0] - v) < 1e-6);
    if (pt) return pt[1];
    if (p.flags & NullArdour.FLAG.TOGGLE) return v > 0.5 ? 'On' : 'Off';
    if (p.flags & NullArdour.FLAG.INTEGER) return String(Math.round(v));
    return Math.abs(v) >= 100 ? v.toFixed(0) : Math.abs(v) >= 10 ? v.toFixed(1) : v.toFixed(2);
  }

  function drawGeneric() {
    sheetBody.textContent = '';
    const list = el('div', 'da-params');
    const shown = view.params.filter((p) => (p.flags & NullArdour.FLAG.INPUT) && !(p.flags & NullArdour.FLAG.HIDDEN));
    if (!shown.length) list.appendChild(el('div', 'empty', 'This plugin has no parameters to control.'));
    for (const first of shown) {
      const par = first.par;
      const v0 = view;
      // the newest read of this parameter (each re-read brings fresh objects)
      const cur = () => v0.params.find((q) => q.par === par) || first;
      const row = el('div', 'da-param');
      const val = el('div', 'da-param-val');
      const bar = el('div', 'da-param-bar');
      bar.appendChild(el('div', 'da-param-fill'));
      bar.setAttribute('aria-label', first.name);
      const span = first.max - first.min || 1;
      const isEnum = (first.flags & (NullArdour.FLAG.ENUM | NullArdour.FLAG.TOGGLE)) || first.points.length;
      const set = (v) => { session.setParam(v0.ssid, v0.piid, par, v); paint(); };
      const paint = () => {
        const p = cur();
        val.textContent = fmt(p, p.value);
        bar.firstChild.style.width = (((p.value - p.min) / span) * 100).toFixed(1) + '%';
      };
      if (isEnum) {
        bar.classList.add('da-step');
        bar.onclick = () => {
          const p = cur();
          const vals = p.points.length ? p.points.map((x) => x[0]) : [p.min, p.max];
          const i = vals.findIndex((x) => Math.abs(x - p.value) < 1e-6);
          set(vals[(i + 1) % vals.length]);
        };
      } else {
        relativeDrag(bar, 'x', () => (cur().value - first.min) / span, (u) => {
          let v = first.min + u * span;
          if (first.flags & NullArdour.FLAG.INTEGER) v = Math.round(v);
          set(v);
        }, () => {});
      }
      view.controls.set(par, paint);
      paint();
      row.append(el('div', 'da-param-name', first.name), val, bar);
      list.appendChild(row);
    }
    sheetBody.appendChild(list);
  }

  function followGeneric() {
    for (const paint of view.controls.values()) paint();
  }

  // --- tab -----------------------------------------------------------------------------------

  NullRemote.register({
    id: 'ardour', title: 'Ardour', order: 20, app: 'daws',
    mount(e) {
      root = e;
      const css = document.createElement('link');
      css.rel = 'stylesheet';
      css.href = BASE + 'ardour.css';
      document.head.appendChild(css);
      root.appendChild(header('Ardour · Mixbus'));
      root.appendChild(buildConnect());
      root.appendChild(buildDesk());
    },
    show() { if (conn && session) session.listStrips(); },
    hide() { closePlugin(); },
    back() { return goBack(); },
  });

  // for the tests
  NullRemote.ardour = { connect, get session() { return session; }, get view() { return view; } };
})();
