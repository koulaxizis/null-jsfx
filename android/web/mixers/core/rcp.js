// NULL Remote, Mixers: a console that speaks Yamaha's remote control protocol (RCP): text lines
// over TCP port 49280 (Yamaha CL, QL, TF and DM7, yamaha/profile.js).
// The same interface as the OSC session in core/mixer.js and the MIDI one in core/nrpn.js
// (connect, disconnect, get, set, on, onAny, onMeters, onStatus, action, layout, info, profile,
// status), so the tab (mixers.js) does not care which one it drives.
//   Send:   set ADDRESS X Y VALUE      get ADDRESS X Y      (a line ends with LF; X, Y from 0;
//                                                           a string value is in double quotes)
//   Answer: OK get ADDRESS X Y VALUE   OK set ADDRESS X Y VALUE    a change from elsewhere (the
//           console's surface, another remote): NOTIFY set ADDRESS X Y VALUE; a level may carry
//           its text after it ("-10.00"); a request the console does not know: ERROR ...
//   Device: devinfo productname -> OK devinfo productname "CL5" (the model);
//           devinfo devicename -> its name; scpmode keepalive 10000 and devstatus runmode every
//           4 s keep the connection open (and show a dead one: core/rcp.js DEAD_MS).
// The profile maps the tab's paths to addresses (key: "ADDRESS X Y"), what a set sends (encode:
// lines) and the values (toValue, fromValue). Written from the open Companion module for Yamaha
// RCP (bitfocus/companion-module-yamaha-rcp, MIT License): it reads, not Yamaha's own
// document, which is not public (Bible C25).
// NULL JSFX • https://nulljsfx.tech • MIT License
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.NullRcp = api;
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  const SEND_GAP_MS = 20; // at most one line per parameter this often while it moves
  const QUERY_BATCH = 4; // get lines sent together (about 400 a second)
  const QUERY_TICK_MS = 10;
  const REDIAL_MS = 2000;
  const KEEPALIVE_MS = 10000; // the console closes a link quiet for this long (scpmode keepalive)
  const PING_MS = 4000;       // so devstatus runmode goes every 4 s: one late timer or a slow
                              // link (a VPN) no longer gets us dropped (it went every 10 s)
  const DEAD_MS = 12000;      // no byte for this long after the console has answered a ping on
                              // this connection: the link is dead though still open; redial

  /** Splits a line into words; a word in double quotes keeps its spaces (and \" inside it). */
  function words(line) {
    return (line.match(/"(?:[^"\\]|\\.)*"|\S+/g) || []);
  }
  const unquote = (w) => (w && w[0] === '"' ? w.slice(1, -1).replace(/\\(.)/g, '$1') : w);
  const quote = (s) => '"' + String(s).replace(/["\\]/g, '\\$&') + '"';

  /** Reads one answer line: {status, action, key, value} for get and set answers, {status,
   *  action, what, value} for devinfo and devstatus, or null. */
  function parse(line) {
    const w = words(line.trim());
    if (w.length < 2) return null;
    const status = w[0].toUpperCase();
    if (status !== 'OK' && status !== 'NOTIFY' && status !== 'ERROR' && status !== 'OKM') return null;
    const action = w[1];
    if (action === 'get' || action === 'set') {
      if (w.length < 5) return { status, action, key: null, value: null };
      const key = w[2] + ' ' + w[3] + ' ' + w[4];
      const v = w[5];
      let value = null;
      if (v != null) value = v[0] === '"' ? unquote(v) : /^-?\d+$/.test(v) ? Number(v) : v;
      return { status, action, key, value };
    }
    return { status, action, what: w[2], value: unquote(w[3]) };
  }

  class Session {
    // dial(host, port) -> (cb {onOpen, onData, onClose}) -> {send(bytes), close()}
    constructor(profile, dial) {
      this.profile = profile;
      this.dial = dial;
      this.values = new Map();
      this.params = new Map(); // path -> type
      this.byKey = new Map(); // "ADDRESS X Y" -> path
      this.keyOf = new Map(); // path -> "ADDRESS X Y", fixed when the layout is taken (the
                              // profile's own map follows whichever layout it made last)
      this.listeners = new Map();
      this.anyListeners = new Set();
      this.statusListeners = new Set();
      this.pending = new Map();
      this.answered = new Set(); // keys answered (a value or an error) in this read
      this.status = 'idle';
      this.info = null;
      this.layout = null;
      this.conn = null;
      this.closed = true;
      this.rest = '';
      this.decoder = new TextDecoder();
      this.encoder = new TextEncoder();
    }

    connect(host, port, timeoutMs = 3000) {
      this.host = host;
      this.port = port || this.profile.port;
      this.closed = false;
      this._useLayout(this.profile.layout());
      this._setStatus('connecting');
      return new Promise((resolve, reject) => {
        const t = setTimeout(() => { fail(new Error('No answer from ' + host)); }, timeoutMs);
        const fail = (e) => { clearTimeout(t); this.closed = true; if (this.conn) this.conn.close(); this.conn = null; this._setStatus('failed'); reject(e); };
        this._open(() => {
          clearTimeout(t);
          this.info = { ip: host, name: host, model: this.profile.family.replace(/^Yamaha /, '') };
          this._sync(timeoutMs).then(() => { this._setStatus('online'); resolve(this.info); }, fail);
        }, (why) => fail(new Error((why ? why + ': ' : '') + 'no connection to ' + host + ':' + this.port)));
      });
    }

    _open(onUp, onFail) {
      let up = false;
      this.rest = '';
      this.decoder = new TextDecoder();
      let ended = false;
      const end = (why) => {
        if (ended) return;
        ended = true;
        this.conn = null;
        clearInterval(this.alive);
        clearInterval(this.dog);
        if (!up) { if (onFail) onFail(why); return; }
        if (this.closed) return;
        // the link went away: try again until it is back, then read everything again
        this._setStatus('lost');
        this._redial();
      };
      const conn = this.dial(this.host, this.port)({
        onOpen: () => {
          up = true;
          this.lastRx = Date.now();
          this.pingAt = 0;
          this.pingOk = false;
          clearInterval(this.dog);
          this.dog = setInterval(() => {
            if (this.conn === conn && this.pingOk && Date.now() - this.lastRx > DEAD_MS) { conn.close(); end('no answer'); }
          }, 1000);
          onUp();
        },
        onData: (bytes) => {
          this.lastRx = Date.now();
          if (this.pingAt && this.lastRx - this.pingAt < 2000) this.pingOk = true;
          this._data(bytes);
        },
        onClose: end,
      });
      this.conn = conn;
    }
    _redial() {
      this.redial = setTimeout(() => {
        if (this.closed) return;
        this._open(() => this._sync(3000).then(() => this._setStatus('online'), () => { if (this.conn) this.conn.close(); }), () => { if (!this.closed) this._redial(); });
      }, REDIAL_MS);
    }

    disconnect() {
      this.closed = true;
      clearTimeout(this.redial);
      clearInterval(this.alive);
      clearInterval(this.dog);
      clearTimeout(this.queryTimer);
      for (const p of this.pending.values()) clearTimeout(p.timer);
      this.pending.clear();
      if (this.conn) { const c = this.conn; this.conn = null; c.close(); }
      this._setStatus('idle');
    }

    get(path) { return this.values.get(path); }

    set(path, value) {
      const type = this.params.get(path);
      if (!type) throw new Error('unknown parameter ' + path);
      value = type === 'i' ? value | 0 : type === 's' ? String(value) : Math.min(1, Math.max(0, value));
      this.values.set(path, value);
      this._emit(path, value, true);
      let p = this.pending.get(path);
      if (!p) { p = { value, timer: null, last: 0 }; this.pending.set(path, p); }
      p.value = value;
      const flush = () => {
        p.timer = null;
        p.last = Date.now();
        const raw = this.profile.toValue(path, p.value);
        this._lines(this.profile.encode(path, raw).map((l) => 'set ' + l));
      };
      if (p.timer) return;
      const now = Date.now();
      if (now - p.last >= SEND_GAP_MS) flush();
      else p.timer = setTimeout(flush, SEND_GAP_MS - (now - p.last));
    }

    action() {} // no one-shot actions here
    on(path, cb) {
      let s = this.listeners.get(path);
      if (!s) this.listeners.set(path, (s = new Set()));
      s.add(cb);
      return () => s.delete(cb);
    }
    onAny(cb) { this.anyListeners.add(cb); return () => this.anyListeners.delete(cb); }
    onMeters() { return () => {}; } // no meters yet (Bible C25)
    onStatus(cb) { this.statusListeners.add(cb); return () => this.statusListeners.delete(cb); }

    _lines(lines) { if (this.conn && lines.length) this.conn.send(this.encoder.encode(lines.join('\n') + '\n')); }

    _data(bytes) {
      const text = this.rest + this.decoder.decode(bytes, { stream: true });
      const lines = text.split('\n');
      this.rest = lines.pop();
      for (const line of lines) {
        const m = parse(line.replace(/\r$/, ''));
        if (m) this._rx(m);
      }
    }

    _rx(m) {
      if (m.action === 'devinfo') {
        if (m.what === 'productname' && this.helloWaiter && m.status === 'OK') this.helloWaiter(m.value);
        else if (m.what === 'devicename' && m.status === 'OK' && m.value && this.info) this.info.name = m.value;
        return;
      }
      if (m.action !== 'get' && m.action !== 'set') return;
      if (m.status === 'ERROR') { if (m.key) this._answered(m.key); return; }
      if (!m.key) return;
      if (m.status === 'OK' && m.action === 'set') return; // the echo of our own set
      this._answered(m.key);
      const path = this.byKey.get(m.key);
      if (!path || m.value == null) return;
      const p = this.pending.get(path);
      if (p && p.timer) return; // the user is moving it: the newest local value wins
      const v = this.profile.fromValue(path, m.value);
      if (this.values.get(path) === v) return;
      this.values.set(path, v);
      this._emit(path, v, false);
    }
    _answered(key) {
      if (!this.asking || !this.asking.has(key)) return;
      this.asking.delete(key);
      if (!this.asking.size && this.queryWaiter) this.queryWaiter();
    }

    _useLayout(layout) {
      this.layout = layout;
      this.params.clear();
      this.byKey.clear();
      this.keyOf.clear();
      for (const p of layout.params) {
        const k = this.profile.key(p.path);
        this.params.set(p.path, p.type);
        this.byKey.set(k, p.path);
        this.keyOf.set(p.path, k);
      }
    }

    // the model (devinfo productname), its layout, the keep-alive, then every value: the late
    // paths (sends) are still being asked when this resolves
    _sync(ms) {
      const P = this.profile;
      return new Promise((resolve, reject) => {
        const t = setTimeout(() => { this.helloWaiter = null; reject(new Error('No answer from the ' + P.family.replace(/^Yamaha /, '') + ' at ' + this.host)); }, ms);
        this.helloWaiter = (product) => {
          clearTimeout(t);
          this.helloWaiter = null;
          this.info.model = P.modelOf(product);
          this._useLayout(P.layoutFor(this.info.model));
          this._lines(['devinfo devicename', 'scpmode keepalive ' + KEEPALIVE_MS]);
          clearInterval(this.alive);
          this.alive = setInterval(() => { this.pingAt = Date.now(); this._lines(['devstatus runmode']); }, PING_MS);
          const all = this.layout.params.map((p) => p.path);
          const late = P.isLate ? all.filter((p) => P.isLate(p)) : [];
          this._queryAll(all.filter((p) => !P.isLate || !P.isLate(p)), Math.max(ms, 8000)).then(() => {
            resolve();
            if (late.length) this._queryAll(late, 20000);
          });
        };
        this._lines(['devinfo productname']);
      });
    }

    _emit(path, value, local) {
      const s = this.listeners.get(path);
      if (s) for (const cb of s) cb(value, local);
      for (const cb of this.anyListeners) cb(path, value, local);
    }

    // ask the paths, paced; resolves when all have answered (a value or an error) or after ms
    _queryAll(paths, ms) {
      if (this.closed) return Promise.resolve(); // disconnected meanwhile
      const keys = paths.map((p) => this.keyOf.get(p)).filter(Boolean);
      this.asking = new Set(keys);
      let i = 0;
      const tick = () => {
        if (this.closed || !this.conn) return; // disconnected meanwhile
        const out = [];
        for (let n = 0; n < QUERY_BATCH && i < keys.length; n++, i++) out.push('get ' + keys[i]);
        this._lines(out);
        if (i < keys.length) this.queryTimer = setTimeout(tick, QUERY_TICK_MS);
      };
      clearTimeout(this.queryTimer);
      tick();
      return new Promise((resolve) => {
        const t = setTimeout(() => { this.queryWaiter = null; resolve(); }, ms);
        this.queryWaiter = () => { clearTimeout(t); this.queryWaiter = null; resolve(); };
        if (!this.asking.size) this.queryWaiter();
      });
    }

    _setStatus(s) {
      if (this.status === s) return;
      this.status = s;
      for (const cb of this.statusListeners) cb(s);
    }
  }

  return { Session, parse, words, quote, unquote };
});
