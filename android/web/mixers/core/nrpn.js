// NULL Remote, Mixers: a console that speaks MIDI NRPN over TCP (Allen & Heath SQ, sq/profile.js;
// Allen & Heath Qu, qu/profile.js; Allen & Heath CQ, cq/profile.js).
// The same interface as the OSC session in core/mixer.js (connect, disconnect, get, set, on,
// onAny, onMeters, onStatus, action, layout, info, profile, status), so the tab (mixers.js) does
// not care which one it drives.
//   Set:  BN 63 MSB  BN 62 LSB  BN 06 VC  BN 26 VF      (CC 99, 98, 6, 38; N = MIDI channel - 1)
//   Get:  BN 63 MSB  BN 62 LSB  BN 60 7F                (CC 96 with 7F); the console answers with
//         a set message of the current value (UNVERIFIED: the protocol document gives the get
//         message and not the answer's format; the set format is the natural one)
// The parameter address is MSB·128 + LSB and the value VC·128 + VF (14 bits). The profile maps
// the tab's paths to addresses and values (address, toValue, fromValue).
// A profile may change how the session talks (the Qu does; every hook is optional, the SQ has none):
//   key(msb, lsb, vc, vf) -> {addr, raw}     an NRPN message's address and value (default: MSB·128
//                                            + LSB, VC·128 + VF)
//   noteKey(note, velocity) -> {addr, raw}   a note on (velocity > 0) as a value (Qu: mutes)
//   encode(channel, path, raw) -> bytes      what a set sends (default: the NRPN set above)
//   hello() -> bytes, helloReply(sysex) -> {channel, model} or null, syncEnd(sysex) -> bool:
//                                            the console pushes its whole state after a hello, so
//                                            nothing is asked one by one; connect waits for the
//                                            reply, then for the end of the push
//   query(path) -> bytes or null             ask one value (default: the NRPN get above; Qu: names)
//   fromSysex(sysex) -> [{addr, raw}]        values in System Exclusive messages (Qu: names)
//   layoutFor(model)                         the layout once the console has said what it is
//   keepAliveMs                              send Active Sensing (FE) this often
//   meterRequest(channel, on) -> bytes, parseMeters(sysex, model) -> {address, db} or null,
//   meterGapMs                               meters (Qu): asked while someone listens; asked
//                                            again meterGapMs after each answer unless the next
//                                            comes first, and after 2 s without one
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
   *  for every complete NRPN value (CC 38 after 99, 98 and 6), with MSB, LSB, VC and VF after
   *  them for a profile that reads them its own way. */
  class Parser {
    // onNote(channel, note, velocity) for a note on, onSysex(bytes F0..F7): both optional
    constructor(onValue, onNote, onSysex) {
      this.onValue = onValue;
      this.onNote = onNote || null;
      this.onSysex = onSysex || null;
      this.sx = null; // a System Exclusive message being read
      this.status = 0;
      this.data = [];
      this.ch = new Map(); // channel -> {msb, lsb, vc}
    }
    push(bytes) {
      for (const x of bytes) {
        if (x >= 0xf8) continue; // real time (the Qu's Active Sensing too)
        if (x === 0xf0) { this.sx = [x]; this.status = 0; continue; }
        if (x === 0xf7) { if (this.sx && this.onSysex) { this.sx.push(x); this.onSysex(this.sx); } this.sx = null; continue; }
        if (this.sx) { if (x < 0x80) { this.sx.push(x); continue; } this.sx = null; }
        if (x >= 0x80) { this.status = x < 0xf0 ? x : 0; this.data = []; continue; }
        if (!this.status) continue;
        this.data.push(x);
        const type = this.status & 0xf0;
        const need = type === 0xc0 || type === 0xd0 ? 1 : 2;
        if (this.data.length < need) continue;
        const [a, b] = this.data;
        this.data = [];
        if (type === 0xb0) this.cc(this.status & 15, a, b);
        else if (type === 0x90 && b > 0 && this.onNote) this.onNote(this.status & 15, a, b);
      }
    }
    cc(ch, num, v) {
      let s = this.ch.get(ch);
      if (!s) this.ch.set(ch, (s = { msb: -1, lsb: -1, vc: -1 }));
      if (num === 0x63) { s.msb = v; s.vc = -1; } else if (num === 0x62) { s.lsb = v; s.vc = -1; } else if (num === 0x06) s.vc = v;
      else if (num === 0x26 && s.msb >= 0 && s.lsb >= 0 && s.vc >= 0) this.onValue(ch, s.msb * 128 + s.lsb, s.vc * 128 + v, s.msb, s.lsb, s.vc, v);
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
      this.meterListeners = new Set();
      this.statusListeners = new Set();
      this.pending = new Map();
      this.status = 'idle';
      this.info = null;
      this.layout = null;
      this.conn = null;
      this.closed = true;
      const P = profile;
      this.parser = new Parser(
        (ch, addr, raw, msb, lsb, vc, vf) => {
          const k = P.key ? P.key(msb, lsb, vc, vf) : { addr, raw };
          if (k) this._rx(ch, k.addr, k.raw);
        },
        P.noteKey ? (ch, note, vel) => { const k = P.noteKey(note, vel); if (k) this._rx(ch, k.addr, k.raw); } : null,
        (sx) => this._sysex(sx));
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
          // no console name on this protocol: the address stands in for it
          this.info = { ip: host, name: host, model: this.profile.family.replace(/^Allen & Heath /, '') };
          this._sync(timeoutMs).then(() => { this._setStatus('online'); resolve(this.info); }, fail);
        }, (why) => fail(new Error((why ? why + ': ' : '') + 'no connection to ' + host + ':' + this.port)));
      });
    }

    _open(onUp, onFail) {
      let up = false;
      this.conn = this.dial(this.host, this.port)({
        onOpen: () => {
          up = true;
          clearInterval(this.alive);
          if (this.profile.keepAliveMs) this.alive = setInterval(() => this._tx([0xfe]), this.profile.keepAliveMs);
          onUp();
        },
        onData: (bytes) => this.parser.push(bytes),
        onClose: (why) => {
          this.conn = null;
          clearInterval(this.alive);
          this._stopMeters();
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
        this._open(() => this._sync(3000).then(() => this._setStatus('online'), () => { if (this.conn) this.conn.close(); }), () => { if (!this.closed) this._redial(); });
      }, REDIAL_MS);
    }

    disconnect() {
      if (this.metering && this.conn) this._tx(this.profile.meterRequest(this.channel, false));
      this._stopMeters();
      this.closed = true;
      clearTimeout(this.redial);
      clearInterval(this.alive);
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
      const flush = () => {
        p.timer = null;
        p.last = Date.now();
        const raw = this.profile.toValue(path, p.value);
        this._tx(this.profile.encode ? this.profile.encode(this.channel, path, raw) : setMessage(this.channel, this.profile.address(path), raw));
      };
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
    onMeters(cb) {
      if (!this.profile.meterRequest) return () => {}; // no meters on this protocol (SQ)
      this.meterListeners.add(cb);
      this._meters();
      return () => { this.meterListeners.delete(cb); this._meters(); };
    }
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
      if (this.queryWaiter && this.values.size >= this.want) this.queryWaiter();
    }

    _useLayout(layout) {
      this.layout = layout;
      this.params.clear();
      this.byAddr.clear();
      for (const p of layout.params) {
        this.params.set(p.path, p.type);
        this.byAddr.set(this.profile.address(p.path), p.path);
      }
    }

    // meters: on while someone listens and the console is read (fresh: after a new hello)
    _meters(fresh) {
      const P = this.profile;
      if (!P.meterRequest) return;
      const want = this.meterListeners.size > 0 && !!this.conn && !this.helloWaiter && !this.syncWaiter;
      if (!want) {
        if (this.metering && this.conn) this._tx(P.meterRequest(this.channel, false));
        this._stopMeters();
        return;
      }
      if (this.metering && !fresh) return;
      this._stopMeters();
      this.metering = true;
      const ask = () => { this.meterAsked = Date.now(); this._tx(P.meterRequest(this.channel, true)); };
      ask();
      // the watchdog: nothing for 2 s (an answer lost, or the console stopped): ask again
      this.meterDog = setInterval(() => { if (Date.now() - Math.max(this.meterAsked, this.meterAt || 0) > 2000) ask(); }, 500);
      this.meterAgain = ask;
    }
    _stopMeters() {
      this.metering = false;
      clearInterval(this.meterDog);
      clearTimeout(this.meterNext);
    }
    _meterData(m) {
      this.meterAt = Date.now();
      for (const cb of this.meterListeners) cb(m.address, m.db);
      clearTimeout(this.meterNext);
      if (this.metering) this.meterNext = setTimeout(this.meterAgain, this.profile.meterGapMs || 100);
    }

    // a System Exclusive message: the hello's answer, the end of the push, meters, or values (names)
    _sysex(sx) {
      const P = this.profile;
      if (P.parseMeters) {
        const m = P.parseMeters(sx, this.info && this.info.model);
        if (m) { this._meterData(m); return; }
      }
      if (this.helloWaiter && P.helloReply) {
        const r = P.helloReply(sx);
        if (r) { this.helloWaiter(r); return; }
      }
      if (this.syncWaiter && P.syncEnd && P.syncEnd(sx)) { this.syncWaiter(); return; }
      if (P.fromSysex) for (const k of P.fromSysex(sx) || []) this._rx(this.channel, k.addr, k.raw);
    }

    // read the console's state: a hello and its push (Qu), or every parameter asked (SQ)
    _sync(ms) {
      const P = this.profile;
      if (!P.hello) return this._queryAll();
      return new Promise((resolve, reject) => {
        const t = setTimeout(() => { this.helloWaiter = null; reject(new Error('No answer from the ' + P.family.replace(/^Allen & Heath /, '') + ' at ' + this.host)); }, ms);
        this.helloWaiter = (r) => {
          clearTimeout(t);
          this.helloWaiter = null;
          this.channel = r.channel;
          if (r.model) this.info.model = r.model;
          if (P.layoutFor) this._useLayout(P.layoutFor(r.model));
          // the push of every value follows; then ask what only a query gives (names)
          const done = () => {
            this.syncWaiter = null;
            clearTimeout(this.syncTimer);
            this._meters(true);
            this._queryAll(1500).then(resolve);
          };
          this.syncWaiter = done;
          this.syncTimer = setTimeout(done, 4000);
        };
        this._tx(P.hello());
      });
    }

    _emit(path, value, local) {
      const s = this.listeners.get(path);
      if (s) for (const cb of s) cb(value, local);
      for (const cb of this.anyListeners) cb(path, value, local);
    }

    // ask every parameter (the profile's query: only those it can ask), paced; resolves when all
    // have answered or after a timeout
    _queryAll(ms = 2500) {
      const P = this.profile;
      const ask = (path) => (P.query ? P.query(this.channel, path) : getMessage(this.channel, P.address(path)));
      const paths = this.layout.params.map((p) => p.path).filter((path) => ask(path));
      const want = this.values.size + paths.filter((path) => !this.values.has(path)).length;
      let i = 0;
      const tick = () => {
        const out = [];
        for (let n = 0; n < QUERY_BATCH && i < paths.length; n++, i++) out.push(...ask(paths[i]));
        if (out.length) this._tx(out);
        if (i < paths.length) setTimeout(tick, QUERY_TICK_MS);
      };
      tick();
      this.want = want;
      return new Promise((resolve) => {
        if (this.values.size >= want) { resolve(); return; }
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
