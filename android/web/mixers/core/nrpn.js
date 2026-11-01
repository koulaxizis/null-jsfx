// NULL Remote, Mixers: a console that speaks MIDI NRPN over TCP (Allen & Heath SQ, sq/profile.js).
// The same interface as the OSC session in core/mixer.js (connect, disconnect, get, set, on,
// onAny, onMeters, onStatus, action, layout, info, profile, status), so the tab (mixers.js) does
// not care which one it drives.
//   Set:  BN 63 MSB  BN 62 LSB  BN 06 VC  BN 26 VF      (CC 99, 98, 6, 38; N = MIDI channel - 1)
//   Get:  BN 63 MSB  BN 62 LSB  BN 60 7F                (CC 96 with 7F); the console answers with
//         a set message of the current value (UNVERIFIED: the protocol document gives the get
//         message and not the answer's format; the set format is the natural one)
// The parameter address is MSB·128 + LSB and the value VC·128 + VF (14 bits). The profile maps
// the tab's paths to addresses and values (address, toValue, fromValue).
// NULL JSFX • https://nulljsfx.tech • MIT License
(function (root, factory) {
  const api = factory(root);
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.NullNrpn = api;
})(typeof self !== 'undefined' ? self : this, function (root) {
  'use strict';

  const SEND_GAP_MS = 20; // at most one message per parameter this often while it moves
  const QUERY_BATCH = 24; // get requests sent together
  const QUERY_TICK_MS = 15;
  const REDIAL_MS = 2000;

  function setMessage(ch, addr, value) {
    const b = 0xb0 | (ch & 15);
    return [b, 0x63, (addr >> 7) & 127, b, 0x62, addr & 127, b, 0x06, (value >> 7) & 127, b, 0x26, value & 127];
  }
  function getMessage(ch, addr) {
    const b = 0xb0 | (ch & 15);
    return [b, 0x63, (addr >> 7) & 127, b, 0x62, addr & 127, b, 0x60, 0x7f];
  }

  /** Reads a MIDI byte stream (running status too) and calls onValue(channel, address, value)
   *  for every complete NRPN value (CC 38 after 99, 98 and 6). */
  class Parser {
    constructor(onValue) {
      this.onValue = onValue;
      this.status = 0;
      this.data = [];
      this.ch = new Map(); // channel -> {msb, lsb, vc}
    }
    push(bytes) {
      for (const x of bytes) {
        if (x >= 0xf8) continue; // real time
        if (x >= 0x80) { this.status = x < 0xf0 ? x : 0; this.data = []; continue; }
        if (!this.status) continue;
        this.data.push(x);
        const type = this.status & 0xf0;
        const need = type === 0xc0 || type === 0xd0 ? 1 : 2;
        if (this.data.length < need) continue;
        const [a, b] = this.data;
        this.data = [];
        if (type === 0xb0) this.cc(this.status & 15, a, b);
      }
    }
    cc(ch, num, v) {
      let s = this.ch.get(ch);
      if (!s) this.ch.set(ch, (s = { msb: -1, lsb: -1, vc: -1 }));
      if (num === 0x63) { s.msb = v; s.vc = -1; } else if (num === 0x62) { s.lsb = v; s.vc = -1; } else if (num === 0x06) s.vc = v;
      else if (num === 0x26 && s.msb >= 0 && s.lsb >= 0 && s.vc >= 0) this.onValue(ch, s.msb * 128 + s.lsb, s.vc * 128 + v);
    }
  }

  class Session {
    // dial(host, port) -> (cb {onOpen, onData, onClose}) -> {send(bytes), close()}
    constructor(profile, dial) {
      this.profile = profile;
      this.dial = dial;
      this.channel = profile.midiChannel || 0;
      this.values = new Map();
      this.params = new Map(); // path -> type
      this.byAddr = new Map(); // address -> path
      this.listeners = new Map();
      this.anyListeners = new Set();
      this.statusListeners = new Set();
      this.pending = new Map();
      this.status = 'idle';
      this.info = null;
      this.layout = null;
      this.conn = null;
      this.closed = true;
      this.parser = new Parser((ch, addr, v) => this._rx(ch, addr, v));
    }

    connect(host, port, timeoutMs = 3000) {
      this.host = host;
      this.port = port || this.profile.port;
      this.closed = false;
      this.layout = this.profile.layout();
      for (const p of this.layout.params) {
        this.params.set(p.path, p.type);
        this.byAddr.set(this.profile.address(p.path), p.path);
      }
      this._setStatus('connecting');
      return new Promise((resolve, reject) => {
        const t = setTimeout(() => { fail(new Error('No answer from ' + host)); }, timeoutMs);
        const fail = (e) => { clearTimeout(t); this.closed = true; if (this.conn) this.conn.close(); this.conn = null; this._setStatus('failed'); reject(e); };
        this._open(() => {
          clearTimeout(t);
          // no console name on this protocol: the address stands in for it
          this.info = { ip: host, name: host, model: this.profile.family.replace(/^Allen & Heath /, '') };
          this._queryAll().then(() => { this._setStatus('online'); resolve(this.info); });
        }, (why) => fail(new Error((why ? why + ': ' : '') + 'no connection to ' + host + ':' + this.port)));
      });
    }

    _open(onUp, onFail) {
      let up = false;
      this.conn = this.dial(this.host, this.port)({
        onOpen: () => { up = true; onUp(); },
        onData: (bytes) => this.parser.push(bytes),
        onClose: (why) => {
          this.conn = null;
          if (!up) { if (onFail) onFail(why); return; }
          if (this.closed) return;
          // the link went away: try again until it is back, then read everything again
          this._setStatus('lost');
          this._redial();
        },
      });
    }
    _redial() {
      this.redial = setTimeout(() => {
        if (this.closed) return;
        this._open(() => this._queryAll().then(() => this._setStatus('online')), () => { if (!this.closed) this._redial(); });
      }, REDIAL_MS);
    }

    disconnect() {
      this.closed = true;
      clearTimeout(this.redial);
      for (const p of this.pending.values()) clearTimeout(p.timer);
      this.pending.clear();
      if (this.conn) { const c = this.conn; this.conn = null; c.close(); }
      this._setStatus('idle');
    }

    get(path) { return this.values.get(path); }

    set(path, value) {
      const type = this.params.get(path);
      if (!type) throw new Error('unknown parameter ' + path);
      value = type === 'i' ? value | 0 : Math.min(1, Math.max(0, value));
      this.values.set(path, value);
      this._emit(path, value, true);
      let p = this.pending.get(path);
      if (!p) { p = { value, timer: null, last: 0 }; this.pending.set(path, p); }
      p.value = value;
      const flush = () => { p.timer = null; p.last = Date.now(); this._tx(setMessage(this.channel, this.profile.address(path), this.profile.toValue(path, p.value))); };
      if (p.timer) return;
      const now = Date.now();
      if (now - p.last >= SEND_GAP_MS) flush();
      else p.timer = setTimeout(flush, SEND_GAP_MS - (now - p.last));
    }

    action() {} // no one-shot actions on this protocol
    on(path, cb) {
      let s = this.listeners.get(path);
      if (!s) this.listeners.set(path, (s = new Set()));
      s.add(cb);
      return () => s.delete(cb);
    }
    onAny(cb) { this.anyListeners.add(cb); return () => this.anyListeners.delete(cb); }
    onMeters() { return () => {}; } // no meters on this protocol
    onStatus(cb) { this.statusListeners.add(cb); return () => this.statusListeners.delete(cb); }

    _tx(bytes) { if (this.conn) this.conn.send(Uint8Array.from(bytes)); }

    _rx(ch, addr, raw) {
      if (ch !== this.channel) return;
      const path = this.byAddr.get(addr);
      if (!path) return;
      const p = this.pending.get(path);
      if (p && p.timer) return; // the user is moving it: the newest local value wins
      const v = this.profile.fromValue(path, raw);
      if (this.values.get(path) === v) return;
      this.values.set(path, v);
      this._emit(path, v, false);
      if (this.queryWaiter && this.values.size >= this.layout.params.length) this.queryWaiter();
    }

    _emit(path, value, local) {
      const s = this.listeners.get(path);
      if (s) for (const cb of s) cb(value, local);
      for (const cb of this.anyListeners) cb(path, value, local);
    }

    // ask every parameter, paced; resolves when all have answered or after a timeout
    _queryAll(ms = 2500) {
      const paths = this.layout.params.map((p) => p.path);
      let i = 0;
      const tick = () => {
        const out = [];
        for (let n = 0; n < QUERY_BATCH && i < paths.length; n++, i++) out.push(...getMessage(this.channel, this.profile.address(paths[i])));
        if (out.length) this._tx(out);
        if (i < paths.length) setTimeout(tick, QUERY_TICK_MS);
      };
      tick();
      return new Promise((resolve) => {
        if (this.values.size >= paths.length) { resolve(); return; }
        const t = setTimeout(() => { this.queryWaiter = null; resolve(); }, ms);
        this.queryWaiter = () => { clearTimeout(t); this.queryWaiter = null; resolve(); };
      });
    }

    _setStatus(s) {
      if (this.status === s) return;
      this.status = s;
      for (const cb of this.statusListeners) cb(s);
    }
  }

  return { Session, Parser, setMessage, getMessage };
});
