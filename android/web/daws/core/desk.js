// Remote for DAWs: the desk every DAW tab other than REAPER shares (Ardour/Mixbus, Bitwig).
// NULL JSFX • https://nulljsfx.tech • MIT License
//
// Connect (address and port, Recent, how to set the DAW up), then the desk: transport and
// clock, one strip per track (meter, fader, mute, solo, record, pan, FX) and the plugins of
// each strip. A NULL JSFX plugin opens with its own interface, drawn by the web version of its
// GFX (web/fx/<id>.js and web/runtime, copied into the app as nullfx/ at build time); any other
// plugin gets a list of its parameters.
//
// NullDawDesk.register(profile) makes one tab. The profile names the tab and its texts and
// opens the DAW's session (ardour/ardour-tab.js, bitwig/bitwig-tab.js). Every session has
// the same shape:
//   on(type, fn): 'strips' (list), 'strip' (s), 'meter' (s), 'transport' ({play, stop, rec, loop}),
//     'time' (text), 'link' (up), 'plugins' (ssid, list), 'params' (ssid, piid, params)
//   connect(), close(), strips (Map ssid -> strip), rate
//   strip: {ssid, name, type, mute, solo, rec, canRec, fader (0..1), pan (0..1), meter}
//   listStrips(), listPlugins(ssid) -> 'plugins' with [{piid, name, enabled}]
//   watchPlugin(ssid, piid) / watchPlugin(null) -> 'params' while it is open, with
//     [{par, name, flags, min, max, points [[value, label]], value, text?}]
//   setFader, setPan (ssid, 0..1), resetFader, resetPan (ssid), setMute, setSolo, setRec (ssid, on)
//   setParam(ssid, piid, par, value in the parameter's range), setPluginActive(ssid, piid, on)
//   play(), stop(), toStart(), record(), loop()
'use strict';
(function () {
  const NULLFX = '../nullfx/';
  const RUNTIME = ['runtime/eel-rt.js', 'runtime/null-fx.js', 'runtime/null-gfx.js'];
  const CSS = '../daws/core/desk.css';
  // parameter flags, as Ardour numbers them; the other DAWs fill in the same bits
  const FLAG = { ENUM: 1, INTEGER: 2, LOG: 4, TOGGLE: 64, INPUT: 0x80, HIDDEN: 0x100 };

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

  let effects = null; // web/fx/index.json effects
  async function nullIndex() {
    if (effects) return effects;
    try {
      const res = await fetch(NULLFX + 'fx/index.json');
      effects = res.ok ? (await res.json()).effects : [];
    } catch (e) { effects = []; }
    return effects;
  }

  /** The NULL JSFX effect a plugin name stands for ("NULL Tilt", "Tilt GFX"), or null. */
  function nullEffect(name, list) {
    if (!name || !list) return null;
    const bare = String(name).replace(/^NULL\s+/i, '').replace(/\s+GFX$/i, '').trim().toLowerCase();
    return list.find((e) => e.name.toLowerCase() === bare) || null;
  }

  /**
   * Pairs the sliders of a NULL effect (FX.sliders: {i, name, min, max}) with the plugin's
   * parameters: by name first, then the remaining inputs in order. Map slider index -> parameter.
   */
  function mapSliders(sliders, params) {
    const inputs = params.filter((p) => p.flags & FLAG.INPUT);
    const map = new Map();
    const used = new Set();
    // the JSFX label carries its unit ("Time (ms)"), the native parameter does not ("Time")
    const bare = (n) => String(n).replace(/\s*\([^)]*\)\s*$/, '').trim().toLowerCase();
    for (const s of sliders) {
      const p = inputs.find((q) => !used.has(q) && bare(q.name) === bare(s.name));
      if (p) { map.set(s.i, p); used.add(p); }
    }
    const rest = inputs.filter((q) => !used.has(q) && !(q.flags & FLAG.HIDDEN));
    for (const s of sliders) if (!map.has(s.i) && rest.length) map.set(s.i, rest.shift());
    return map;
  }

  /** A slider value (range lo..hi) as the parameter's value (p.min..p.max), and back. */
  function toParam(v, lo, hi, p) {
    if (hi === lo || (p.min === lo && p.max === hi)) return clamp(v, p.min, p.max);
    return clamp(p.min + ((v - lo) / (hi - lo)) * (p.max - p.min), p.min, p.max);
  }
  function fromParam(v, lo, hi, p) {
    if (p.max === p.min || (p.min === lo && p.max === hi)) return v;
    return lo + ((v - p.min) / (p.max - p.min)) * (hi - lo);
  }
  function clamp(v, lo, hi) {
    // Ardour compares in float32: keep the value inside the range after rounding too
    const f = Math.fround(Math.min(hi, Math.max(lo, v)));
    return f > hi ? Math.fround(hi) : f < lo ? Math.fround(lo) : f;
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

  let cssAdded = false;

  /**
   * profile: {
   *   id, title, order, header, prefsKey, port, portLabel, files (scripts to load first),
   *   connectTitle, setupTitle, setup [[text, bold, text]...], noAnswer(c), noTracks, noPlugins,
   *   followNote, needsApp,
   *   open(c) -> session (after files are loaded; may throw with a message for the user),
   *   kind(s) -> label under the name, isMaster(s), hasFx(s), hasPan(s),
   *   meter(s) -> {h (0..1), hot}, dbText(s, session), unity (fader position of 0 dB)
   * }
   */
  function register(P) {
    const { el, prefs, header } = NullRemote;
    let root, connectView, desk, msg, session = null, conn = null;
    let strips, clock, transportBtns, sheet, sheetBody, sheetTitle;
    let view = null; // the open sheet: {kind: 'list' | 'plugin', ssid, ...}

    function say(text, err) {
      msg.textContent = text || '';
      msg.className = 'msg' + (err ? ' err' : '');
    }

    // --- connect -----------------------------------------------------------------------------

    function saved() { return prefs.get(P.prefsKey, []); }
    function remember(c) {
      const list = saved().filter((x) => !(x.host === c.host && x.port === c.port));
      list.unshift(c);
      prefs.set(P.prefsKey, list.slice(0, 8));
    }

    function buildConnect() {
      connectView = el('div', 'scroll');
      const page = el('div', 'page');
      const card = el('div', 'card');
      card.appendChild(el('h2', '', P.connectTitle));
      const last = saved()[0] || {};
      const row = el('div', 'row');
      const host = el('input');
      host.placeholder = 'Computer address, e.g. 192.168.1.20';
      host.inputMode = 'decimal';
      host.value = last.host || '';
      host.setAttribute('aria-label', 'Computer address');
      const port = el('input', 'narrow');
      port.inputMode = 'numeric';
      port.value = last.port || P.port;
      port.setAttribute('aria-label', P.portLabel);
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
      help.appendChild(el('h2', '', P.setupTitle));
      const ol = el('ol');
      for (const step of P.setup) {
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
      c = { host: String(c.host || '').trim(), port: parseInt(c.port, 10) || P.port };
      if (!c.host) return say(`Type the address of the computer that runs ${P.title}.`, true);
      say(`Connecting to ${c.host}:${c.port}…`);
      let s;
      try {
        await loadAll(P.files);
        s = await P.open(c);
      } catch (e) {
        return say(NullShell.native ? e.message : 'This tab needs the Remote for DAWs app (no network access in a browser).', true);
      }
      if (session) session.close();
      session = s;
      let answered = false;
      session.on('strips', (list) => {
        if (session !== s) return;
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
        if (!answered && session === s) say(P.noAnswer(c), true);
      }, 4000);
      return true;
    }

    function disconnect() {
      closeSheet();
      if (session) session.close();
      session = null;
      conn = null;
      drawn = '';
      desk.hidden = true;
      connectView.hidden = false;
    }

    // --- desk --------------------------------------------------------------------------------

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

    // the list is drawn again only when its tracks change, so a strip under a finger stays put
    // while the DAW sends the list again; the controls always act on the newest strip object
    let drawn = '';
    function drawStrips(list) {
      const key = list.map((s) => [s.ssid, s.name, s.type, !!s.canRec].join('|')).join('\n');
      if (key === drawn && strips.childElementCount) {
        for (const s of list) updateStrip(s);
        return;
      }
      drawn = key;
      strips.textContent = '';
      for (const first of list) {
        const ssid = first.ssid;
        const cur = () => (session && session.strips.get(ssid)) || first;
        const st = el('div', 'da-strip' + (P.isMaster(first) ? ' da-master' : ''));
        st.dataset.ssid = ssid;
        const name = el('div', 'da-name', first.name);
        const kind = el('div', 'da-kind', P.kind(first));
        const fx = el('button', 'da-fx', 'FX');
        fx.onclick = () => openPlugins(ssid);
        fx.hidden = !P.hasFx(first);
        const pan = el('div', 'da-pan');
        pan.appendChild(el('div', 'da-pan-dot'));
        relativeDrag(pan, 'x', () => cur().pan ?? 0.5, (v) => { session.setPan(ssid, v); updateStrip(cur()); },
          () => { session.resetPan(ssid); updateStrip(cur()); });
        if (!P.hasPan(first)) pan.style.visibility = 'hidden';
        const lane = el('div', 'da-lane');
        const meter = el('div', 'da-meter');
        meter.appendChild(el('div', 'da-meter-fill'));
        const fader = el('div', 'da-fader');
        fader.appendChild(el('div', 'da-cap'));
        relativeDrag(fader, 'y', () => cur().fader ?? P.unity, (v) => { session.setFader(ssid, v); updateStrip(cur()); },
          () => { session.resetFader(ssid); updateStrip(cur()); });
        lane.append(meter, fader);
        const db = el('div', 'da-db');
        const btns = el('div', 'da-btns');
        const mute = el('button', 'da-m', 'M');
        mute.onclick = () => { session.setMute(ssid, !cur().mute); updateStrip(cur()); };
        const solo = el('button', 'da-s', 'S');
        solo.onclick = () => { session.setSolo(ssid, !cur().solo); updateStrip(cur()); };
        btns.append(mute, solo);
        if (first.canRec) {
          const rec = el('button', 'da-r', 'R');
          rec.onclick = () => { session.setRec(ssid, !cur().rec); updateStrip(cur()); };
          btns.appendChild(rec);
        }
        st.append(name, kind, fx, pan, lane, db, btns);
        strips.appendChild(st);
        updateStrip(first);
      }
      if (!list.length) strips.appendChild(el('div', 'empty', P.noTracks));
    }

    function stripEl(ssid) { return strips.querySelector(`.da-strip[data-ssid="${ssid}"]`); }

    function updateStrip(s) {
      const st = stripEl(s.ssid);
      if (!st) return;
      st.querySelector('.da-name').textContent = s.name;
      const pos = s.fader ?? P.unity;
      st.querySelector('.da-cap').style.bottom = `calc(${(pos * 100).toFixed(2)}% - ${(pos * 24).toFixed(1)}px)`;
      st.querySelector('.da-db').textContent = P.dbText(s, session);
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
      const m = P.meter(s);
      const fill = st.querySelector('.da-meter-fill');
      fill.style.height = (Math.max(0, Math.min(1, m.h)) * 100).toFixed(1) + '%';
      fill.classList.toggle('hot', !!m.hot);
    }

    function updateTransport(t) {
      transportBtns.play.classList.toggle('on', !!t.play);
      transportBtns.stop.classList.toggle('on', !!t.stop);
      transportBtns.rec.classList.toggle('on', !!t.rec);
      transportBtns.loop.classList.toggle('on', !!t.loop);
    }

    // --- plugins -----------------------------------------------------------------------------

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
        sheetBody.appendChild(el('div', 'empty', P.noPlugins));
        return;
      }
      const box = el('div', 'list da-plugins');
      for (const p of list) {
        const fx = nullEffect(p.name, effects);
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
          view.map = mapSliders(view.FX.sliders, params);
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
    // the newest read of the parameter a slider is paired with (a re-read brings fresh objects)
    function paramOf(i) {
      const first = view.map.get(i);
      return first && (view.params.find((q) => q.par === first.par) || first);
    }

    function sliderValues() {
      const out = {};
      for (const s of view.FX.sliders) {
        const p = paramOf(s.i);
        if (p) out[s.i] = fromParam(p.value, s.min, s.max, p);
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
          session.setParam(v.ssid, v.piid, p.par, toParam(value, s.min, s.max, p));
        },
      });
      const loop = () => {
        if (view !== v || !v.gfx) return;
        v.gfx.draw();
        v.frame = requestAnimationFrame(loop);
      };
      v.frame = requestAnimationFrame(loop);
      sheetBody.appendChild(el('div', 'muted da-note', P.followNote));
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
      if (p.text) return p.text;
      const pt = p.points.find((x) => Math.abs(x[0] - v) < 1e-6);
      if (pt) return pt[1];
      if (p.flags & FLAG.TOGGLE) return v > 0.5 ? 'On' : 'Off';
      if (p.flags & FLAG.INTEGER) return String(Math.round(v));
      return Math.abs(v) >= 100 ? v.toFixed(0) : Math.abs(v) >= 10 ? v.toFixed(1) : v.toFixed(2);
    }

    function drawGeneric() {
      sheetBody.textContent = '';
      const list = el('div', 'da-params');
      const shown = view.params.filter((p) => (p.flags & FLAG.INPUT) && !(p.flags & FLAG.HIDDEN));
      if (!shown.length) list.appendChild(el('div', 'empty', 'This plugin has no parameters to control.'));
      for (const first of shown) {
        const par = first.par;
        const v0 = view;
        // the newest read of this parameter (a re-read brings fresh objects)
        const cur = () => v0.params.find((q) => q.par === par) || first;
        const row = el('div', 'da-param');
        const val = el('div', 'da-param-val');
        const bar = el('div', 'da-param-bar');
        bar.appendChild(el('div', 'da-param-fill'));
        bar.setAttribute('aria-label', first.name);
        const span = first.max - first.min || 1;
        const isEnum = (first.flags & (FLAG.ENUM | FLAG.TOGGLE)) || first.points.length;
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
            if (first.flags & FLAG.INTEGER) v = Math.round(v);
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

    // --- tab ---------------------------------------------------------------------------------

    NullRemote.register({
      id: P.id, title: P.title, order: P.order, app: 'daws',
      mount(e) {
        root = e;
        if (!cssAdded) {
          cssAdded = true;
          const css = document.createElement('link');
          css.rel = 'stylesheet';
          css.href = CSS;
          document.head.appendChild(css);
        }
        root.appendChild(header(P.header));
        root.appendChild(buildConnect());
        root.appendChild(buildDesk());
      },
      show() { if (conn && session) session.listStrips(); },
      hide() { closePlugin(); },
      back() { return goBack(); },
    });

    // for the tests
    NullRemote[P.id] = { connect, get session() { return session; }, get view() { return view; } };
  }

  window.NullDawDesk = { register, FLAG, nullEffect, mapSliders, toParam, fromParam, load };
})();
