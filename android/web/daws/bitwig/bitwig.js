// Remote for DAWs, Bitwig Studio: the session. Talks over TCP to our controller script
// (remote/bitwig/NullJSFX/NullJSFX.control.js, port 8818), which runs inside Bitwig; the
// protocol is written out at the top of that script. The Ableton tab uses this session too:
// our control surface for Live (remote/ableton/NullJSFX, port 8819) speaks the same protocol. JSON both ways: to Bitwig every message
// has Bitwig's 4-byte big-endian length in front (RemoteConnection's framing); from Bitwig one
// object per line. The API documents only the framing towards Bitwig, so the reader also takes
// a 4-byte length in front of a send, should Bitwig add one (a JSON line never starts with 0).
//
// The script reports the plugin parameters through Bitwig's direct parameter API, normalized
// 0..1. The NULL JSFX plugins run in Bitwig as VST3 or CLAP with one linear parameter per
// slider, named like the slider, so a NULL plugin is found by name and drawn with its own
// interface (web/fx, NULLGfx.View) by the shared desk (../core/desk.js).
//
// Runs in the WebView and in Node (tests). opts.dial({onOpen, onData(bytes), onClose}) opens
// a connection and returns {send(bytes), close()}: shellDial() in the app, net in the tests.
// NULL JSFX • https://nulljsfx.tech • MIT License
(function (root, factory) {
  const api = factory(root);
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.NullBitwig = api;
})(typeof self !== 'undefined' ? self : this, function (root) {
  'use strict';

  const PORT = 8818;
  const PROTO = 1;
  const SEND_EVERY = 25; // ms: at most one message per control while a finger drags it
  const REDIAL = 2000; // ms between tries while Bitwig is not there
  const KEEP_LOCAL = 500; // ms a control the user moved ignores older values coming back
  const UNITY = 0.7937; // Bitwig's volume position for 0 dB (only until Bitwig reports it)
  const INPUT = 0x80; // the desk's parameter flag for a control input

  const enc = typeof TextEncoder !== 'undefined' ? new TextEncoder() : null;
  const dec = typeof TextDecoder !== 'undefined' ? new TextDecoder() : null;
  const utf8 = (s) => enc.encode(s);
  const text = (b) => dec.decode(b);

  /** One message to Bitwig: 4-byte big-endian length, then the JSON. */
  function frame(obj) {
    const body = utf8(JSON.stringify(obj));
    const out = new Uint8Array(4 + body.length);
    new DataView(out.buffer).setUint32(0, body.length, false);
    out.set(body, 4);
    return out;
  }

  /** Reads Bitwig's stream: lines of JSON, each send possibly behind a 4-byte length. */
  class Reader {
    constructor(onMessage) {
      this.buf = new Uint8Array(0);
      this.onMessage = onMessage;
    }
    push(bytes) {
      const b = new Uint8Array(this.buf.length + bytes.length);
      b.set(this.buf);
      b.set(bytes, this.buf.length);
      this.buf = b;
      for (;;) {
        if (!this.buf.length) return;
        if (this.buf[0] === 0) {
          if (this.buf.length < 4) return;
          const n = new DataView(this.buf.buffer, this.buf.byteOffset).getUint32(0, false);
          if (this.buf.length < 4 + n) return;
          this.lines(this.buf.subarray(4, 4 + n));
          this.buf = this.buf.slice(4 + n);
          continue;
        }
        const nl = this.buf.indexOf(10);
        if (nl < 0) return;
        this.lines(this.buf.subarray(0, nl));
        this.buf = this.buf.slice(nl + 1);
      }
    }
    lines(bytes) {
      for (const line of text(bytes).split('\n')) {
        if (!line.trim()) continue;
        let m;
        try { m = JSON.parse(line); } catch (e) { continue; }
        if (m && typeof m.t === 'string') this.onMessage(m);
      }
    }
  }

  /** A TCP connection through the app (window.NullShell, android/web/core/shell.js). */
  function shellDial(host, port) {
    const shell = root.NullShell;
    if (!shell || !shell.native) throw new Error('This tab needs the Remote for DAWs app (no network access in a browser).');
    const b64 = (bytes) => { let s = ''; for (let k = 0; k < bytes.length; k++) s += String.fromCharCode(bytes[k]); return btoa(s); };
    const unb64 = (str) => { const s = atob(str); const b = new Uint8Array(s.length); for (let k = 0; k < s.length; k++) b[k] = s.charCodeAt(k); return b; };
    return function dial(cb) {
      const id = shell.tcpOpen(host, port, 4000);
      const on = {
        tcpopen: (m) => { if (m.id === id) cb.onOpen(); },
        tcp: (m) => { if (m.id === id) cb.onData(unb64(m.data)); },
        tcpclose: (m) => { if (m.id === id) { off(); cb.onClose(m.error || ''); } },
      };
      const off = () => { for (const k in on) shell.off(k, on[k]); };
      for (const k in on) shell.on(k, on[k]);
      if (id == null || id === -1) setTimeout(() => { off(); cb.onClose('cannot open a connection'); }, 0);
      return {
        send: (bytes) => shell.tcpSend(id, b64(bytes)),
        close: () => { off(); shell.tcpClose(id); },
      };
    };
  }

  class Session {
    constructor(opts) {
      this.dial = opts.dial;
      this.now = opts.now || (() => Date.now());
      this.timers = opts.timers || { set: (fn, ms) => setInterval(fn, ms), clear: (h) => clearInterval(h) };
      this.handlers = new Map();
      this.strips = new Map(); // ssid (slot in the script's window) -> strip
      this.devices = new Map(); // ssid -> [{piid, name, enabled, plugin}]
      this.rate = 0; // not reported: the GFX uses 48 kHz
      this.transport = { play: false, stop: true, rec: false, loop: false };
      this.time = '';
      this.total = 0; // tracks in the document
      this.from = 0; // the first of them in the script's window
      this.size = 0; // tracks in the window (the script says it in hello)
      this.sendNames = []; // the targets of sends 0.. (Bitwig's effect tracks, Live's returns)
      this.conn = null;
      this.open = false; // TCP up
      this.up = false; // and Bitwig's script answered
      this.closed = false;
      this.retryAt = 0;
      this.pending = new Map(); // key -> message waiting for its slot
      this.lastSent = new Map();
      this.touched = new Map(); // key -> time of the last local change
      this.later = new Map(); // key -> value from Bitwig that came while the key was held
      this.watch = null; // {ssid, piid, ok, ids, names, vals, disps}
      this.paramsDue = false;
      this.reader = new Reader((m) => this.handle(m));
      this.tick = this.timers.set(() => this.poll(), 25);
    }

    on(type, fn) {
      if (!this.handlers.has(type)) this.handlers.set(type, new Set());
      this.handlers.get(type).add(fn);
      return () => this.handlers.get(type).delete(fn);
    }
    emit(type, ...args) {
      for (const fn of this.handlers.get(type) || []) {
        try { fn(...args); } catch (e) { if (root.console) console.error(e); }
      }
    }

    connect() {
      this.closed = false;
      if (this.conn) return;
      this.reader = new Reader((m) => this.handle(m));
      this.conn = this.dial({
        onOpen: () => { this.open = true; this.send({ t: 'hello' }); },
        onData: (bytes) => this.reader.push(bytes),
        onClose: () => this.dropped(),
      });
    }

    dropped() {
      this.conn = null;
      this.open = false;
      if (this.up) { this.up = false; this.emit('link', false); }
      this.retryAt = this.now() + REDIAL;
    }

    close() {
      this.closed = true;
      this.timers.clear(this.tick);
      if (this.conn) { const c = this.conn; this.conn = null; c.close(); }
    }

    send(obj) {
      if (this.conn && this.open) this.conn.send(frame(obj));
    }
    // a control a finger is dragging: the newest value goes out at most every SEND_EVERY ms
    sendSoon(key, obj) {
      this.touched.set(key, this.now());
      if (this.now() - (this.lastSent.get(key) || 0) >= SEND_EVERY) {
        this.send(obj);
        this.lastSent.set(key, this.now());
        this.pending.delete(key);
      } else {
        this.pending.set(key, obj);
      }
    }
    held(key) { return this.now() - (this.touched.get(key) || 0) < KEEP_LOCAL; }
    // a value from Bitwig for key: true when it may be shown now, else kept for later
    take(key, v) {
      if (!this.held(key)) { this.later.delete(key); return true; }
      this.later.set(key, v);
      return false;
    }

    poll() {
      const now = this.now();
      for (const [key, obj] of this.pending) {
        if (now - (this.lastSent.get(key) || 0) >= SEND_EVERY) {
          this.send(obj);
          this.lastSent.set(key, now);
          this.pending.delete(key);
        }
      }
      // values Bitwig sent while the user was moving the control: the newest one, once let go
      for (const [key, v] of this.later) {
        if (this.held(key)) continue;
        this.later.delete(key);
        const [kind, rest] = [key.slice(0, key.indexOf('/')), key.slice(key.indexOf('/') + 1)];
        if (kind === 'param') {
          if (this.watch && this.watch.ids.includes(rest)) { this.watch.vals[rest] = v; this.paramsDue = true; }
        } else if (kind === 'send') {
          const [ssid, k] = rest.split('/').map(Number);
          const st = this.strips.get(ssid);
          if (st && st.sends) { st.sends[k] = v; this.emit('strip', st); }
        } else {
          const st = this.strips.get(Number(rest));
          if (st) { st[kind === 'vol' ? 'fader' : 'pan'] = v; this.emit('strip', st); }
        }
      }
      if (!this.conn && !this.closed && now >= this.retryAt) this.connect();
      if (this.paramsDue) { this.paramsDue = false; this.emitParams(); }
    }

    // --- from Bitwig ------------------------------------------------------------------------

    handle(m) {
      // any message means the script is there (a flush can overtake the answer to hello)
      if (!this.up) { this.up = true; this.emit('link', true); }
      switch (m.t) {
        case 'hello':
          if (m.proto > PROTO && root.console) console.warn('the DAW script speaks protocol ' + m.proto);
          this.size = m.size | 0;
          return;
        case 'sends':
          this.sendNames = (m.list || []).map((n) => (n == null ? null : String(n)));
          this.emit('sends', this.sendNames);
          return;
        case 'tracks': return this.tracks(m);
        case 'track': return this.track(m);
        case 'devices': return this.deviceList(m);
        case 'meters':
          for (const s of this.strips.values()) {
            const v = m.v[s.ssid];
            if (v != null) { s.meter = v; this.emit('meter', s); }
          }
          return;
        case 'transport':
          this.transport = { play: !!m.play, stop: !m.play, rec: !!m.rec, loop: !!m.loop };
          this.emit('transport', this.transport);
          return;
        case 'time': this.time = String(m.s || ''); this.emit('time', this.time); return;
        case 'device': return this.device(m);
        case 'pids': return this.pids(m.ids || []);
        case 'pname': if (this.watch) { this.watch.names[m.id] = String(m.n); this.paramsDue = true; } return;
        case 'pval':
          if (this.watch && this.take('param/' + m.id, Number(m.v))) { this.watch.vals[m.id] = Number(m.v); this.paramsDue = true; }
          return;
        case 'pdisp': if (this.watch) { this.watch.disps[m.id] = String(m.s); this.paramsDue = true; } return;
        default:
      }
    }

    strip(t, old) {
      const s = old || { ssid: t.i, meter: 0 };
      s.name = String(t.name || '');
      s.type = String(t.type || '');
      s.canRec = /^(Audio|Instrument|Hybrid|MIDI)$/.test(s.type); // Bitwig's and Live's types
      this.fields(s, t);
      return s;
    }
    fields(s, t) {
      if ('vol' in t && this.take('vol/' + s.ssid, Number(t.vol))) s.fader = Number(t.vol);
      if ('volText' in t) s.volText = String(t.volText);
      if ('pan' in t && this.take('pan/' + s.ssid, Number(t.pan))) s.pan = Number(t.pan);
      if ('panText' in t) s.panText = String(t.panText);
      if ('mute' in t) s.mute = !!t.mute;
      if ('solo' in t) s.solo = !!t.solo;
      if ('arm' in t) s.rec = !!t.arm;
      if ('color' in t) s.color = String(t.color);
      if ('name' in t) s.name = String(t.name);
      if (Array.isArray(t.sends)) {
        // a send being dragged keeps the phone's value until it is let go
        const old = s.sends || [];
        s.sends = t.sends.map((v, k) => (v == null ? null : this.take('send/' + s.ssid + '/' + k, Number(v)) ? Number(v) : old[k]));
        s.sendTexts = (t.sendTexts || []).map(String);
      }
    }
    tracks(m) {
      this.total = m.total | 0;
      this.from = m.from | 0;
      const list = new Map();
      for (const t of m.list || []) {
        // a slot keeps its strip object (and meter) when its track changes name or moves
        list.set(t.i, this.strip(t, this.strips.get(t.i)));
      }
      this.strips = list;
      for (const k of [...this.devices.keys()]) if (!list.has(k)) this.devices.delete(k);
      this.emit('strips', [...list.values()]);
    }
    track(m) {
      const s = this.strips.get(m.i);
      if (!s) return;
      this.fields(s, m);
      this.emit('strip', s);
    }
    deviceList(m) {
      const list = (m.list || []).map((d) => ({ piid: d.d, name: String(d.name), enabled: !!d.on, plugin: !!d.plugin }));
      this.devices.set(m.i, list);
      this.emit('plugins', m.i, list);
    }

    device(m) {
      const w = this.watch;
      if (!w) return;
      const listed = (this.devices.get(w.ssid) || []).find((d) => d.piid === w.piid);
      w.ok = m.i === w.ssid && m.d === w.piid && !!m.exists && (!listed || listed.name === m.name);
      this.paramsDue = true;
    }
    pids(ids) {
      const w = this.watch;
      if (!w) return;
      w.ids = ids.map(String);
      w.names = {};
      w.vals = {};
      w.disps = {};
      this.paramsDue = true;
    }

    emitParams() {
      const w = this.watch;
      if (!w || !w.ok || !w.ids.length) return;
      const params = [];
      w.ids.forEach((id, k) => {
        if (w.names[id] == null) return;
        params.push({
          par: k + 1, id, name: w.names[id], flags: INPUT, min: 0, max: 1, points: [],
          value: w.vals[id] ?? 0, text: w.disps[id] || '',
        });
      });
      if (params.length) this.emit('params', w.ssid, w.piid, params);
    }

    // --- to Bitwig --------------------------------------------------------------------------

    listStrips() { this.send({ t: 'hello' }); }
    /** Moves the script's window so that track `from` of the document comes first (the last
     *  page may hold fewer tracks than the window). */
    scroll(from) {
      const last = Math.max(0, this.total - 1);
      this.send({ t: 'scroll', from: Math.max(0, Math.min(last, Math.round(from) || 0)) });
    }
    listPlugins(ssid) {
      const list = this.devices.get(ssid);
      if (list) setTimeout(() => this.emit('plugins', ssid, list), 0);
    }
    watchPlugin(ssid, piid) {
      if (ssid == null) {
        if (this.watch) this.send({ t: 'close' });
        this.watch = null;
        return;
      }
      this.watch = { ssid, piid, ok: false, ids: [], names: {}, vals: {}, disps: {} };
      this.send({ t: 'open', i: ssid, d: piid });
    }

    setFader(ssid, v) {
      const s = this.strips.get(ssid);
      v = Math.min(1, Math.max(0, v));
      if (s) s.fader = v;
      this.sendSoon('vol/' + ssid, { t: 'vol', i: ssid, v });
    }
    setPan(ssid, v) {
      const s = this.strips.get(ssid);
      v = Math.min(1, Math.max(0, v));
      if (s) s.pan = v;
      this.sendSoon('pan/' + ssid, { t: 'pan', i: ssid, v });
    }
    /** Send k of a strip, 0..1 (a script or surface that reports sends: Bitwig, Live). */
    setSend(ssid, k, v) {
      const s = this.strips.get(ssid);
      v = Math.min(1, Math.max(0, v));
      if (s && s.sends && s.sends[k] != null) s.sends[k] = v;
      this.sendSoon('send/' + ssid + '/' + k, { t: 'send', i: ssid, k, v });
    }
    resetSend(ssid, k) { this.touched.delete('send/' + ssid + '/' + k); this.send({ t: 'reset', i: ssid, what: 'send', k }); }
    resetFader(ssid) { this.touched.delete('vol/' + ssid); this.send({ t: 'reset', i: ssid, what: 'vol' }); }
    resetPan(ssid) { this.touched.delete('pan/' + ssid); this.send({ t: 'reset', i: ssid, what: 'pan' }); }
    setMute(ssid, on) { this.button(ssid, 'mute', on, 'mute'); }
    setSolo(ssid, on) { this.button(ssid, 'solo', on, 'solo'); }
    setRec(ssid, on) { this.button(ssid, 'arm', on, 'rec'); }
    button(ssid, t, on, field) {
      const s = this.strips.get(ssid);
      if (s) s[field] = !!on;
      this.send({ t, i: ssid, on: !!on });
    }

    /** value normalized 0..1, as the parameters are reported */
    setParam(ssid, piid, par, value) {
      const w = this.watch;
      if (!w || w.ssid !== ssid || w.piid !== piid) return;
      const id = w.ids[par - 1];
      if (id == null) return;
      value = Math.min(1, Math.max(0, value));
      w.vals[id] = value;
      this.sendSoon('param/' + id, { t: 'param', id, v: value });
    }
    setPluginActive(ssid, piid, on) {
      const p = (this.devices.get(ssid) || []).find((d) => d.piid === piid);
      if (p) p.enabled = !!on;
      this.send({ t: 'bypass', i: ssid, d: piid, on: !!on });
    }

    play() { this.send({ t: 'transport', do: 'play' }); }
    stop() { this.send({ t: 'transport', do: 'stop' }); }
    toStart() { this.send({ t: 'transport', do: 'start' }); }
    record() { this.send({ t: 'transport', do: 'rec' }); }
    loop() { this.send({ t: 'transport', do: 'loop' }); }
  }

  return { Session, Reader, frame, shellDial, PORT, PROTO, UNITY };
});
