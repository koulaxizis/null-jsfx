// NULL Remote, Mixers: one live session with an OSC console, independent of the model family.
// A profile (xair/profile.js, x32/profile.js) supplies the port, discovery and info messages,
// the keep-alive messages and the layout (strips and the typed OSC parameters); this file keeps
// the state, queries every parameter on connect, applies updates the console pushes, sends
// changes (coalesced per parameter), renews subscriptions and watches the link.
// NULL JSFX • https://nulljsfx.tech • MIT License
(function (root, factory) {
  const api = factory(root);
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.NullMixer = api;
})(typeof self !== 'undefined' ? self : this, function (root) {
  'use strict';

  const Osc = typeof module === 'object' && module.exports ? require('./osc.js') : root.NullOsc;
  const Levels = typeof module === 'object' && module.exports ? require('./levels.js') : root.NullLevels;

  const SEND_GAP_MS = 20;     // at most one message per parameter every 20 ms while dragging
  const QUERY_BATCH = 24;     // parameters queried per tick on connect
  const QUERY_TICK_MS = 8;
  const LOST_AFTER_MS = 5000; // no packet for this long: the link is lost

  // Ask every console on the LAN who it is. Resolves to [{host, port, name, model, version}].
  function discover(profile, transport, ms = 1500) {
    return new Promise((resolve) => {
      const found = new Map();
      const off = transport.onPacket((bytes, host) => {
        let msgs;
        try { msgs = Osc.decode(bytes); } catch (_) { return; }
        for (const m of msgs) {
          if (m.address !== profile.infoAddress) continue;
          const info = profile.parseInfo(m.args);
          if (info) found.set(host, Object.assign({ host, port: profile.port }, info));
        }
      });
      transport.broadcast(profile.port, Osc.encode(profile.infoAddress));
      setTimeout(() => { off(); resolve([...found.values()]); }, ms);
    });
  }

  // Ask one address which console it is, on the port of every profile at once (the user typed
  // an IP and did not say X AIR or X32). Resolves to {profile, info} from the first answer.
  function identify(profiles, transport, host, ms = 2500) {
    return new Promise((resolve, reject) => {
      let done = false;
      let tries = 0;
      let timer = null;
      const off = transport.onPacket((bytes, from) => {
        if (done || from !== host) return;
        let msgs;
        try { msgs = Osc.decode(bytes); } catch (_) { return; }
        for (const m of msgs) {
          for (const p of profiles) {
            if (m.address !== p.infoAddress) continue;
            const info = p.parseInfo(m.args);
            if (!info) continue;
            done = true;
            clearTimeout(timer);
            off();
            resolve({ profile: p, info });
            return;
          }
        }
      });
      const ask = () => {
        if (done) return;
        if (tries++ >= 3) { done = true; off(); reject(new Error('No answer from ' + host)); return; }
        for (const p of profiles) transport.send(host, p.port, Osc.encode(p.infoAddress));
        timer = setTimeout(ask, ms / 3);
      };
      ask();
    });
  }

  class Session {
    constructor(profile, transport) {
      this.profile = profile;
      this.transport = transport;
      this.values = new Map();      // path -> value
      this.params = new Map();      // path -> type tag
      this.listeners = new Map();   // path -> Set(cb)
      this.anyListeners = new Set();
      this.meterListeners = new Set();
      this.statusListeners = new Set();
      this.pending = new Map();     // path -> {value, timer, last}
      this.status = 'idle';
      this.info = null;
      this.layout = null;
      this.lastRx = 0;
      this.timers = [];
      this.offPacket = transport.onPacket((bytes, host) => this._rx(bytes, host));
    }

    // Connect to a console: identify it, build its layout, then load every parameter.
    async connect(host, port, timeoutMs = 2500) {
      this.host = host;
      this.port = port || this.profile.port;
      this._setStatus('connecting');
      const info = await new Promise((resolve, reject) => {
        let tries = 0;
        const ask = () => {
          if (this.info) return;
          if (tries++ >= 3) { this.infoWaiter = null; reject(new Error('No answer from ' + host)); return; }
          this._tx(this.profile.infoAddress, []);
          this.infoTimer = setTimeout(ask, timeoutMs / 3);
        };
        this.infoWaiter = (inf) => { clearTimeout(this.infoTimer); resolve(inf); };
        ask();
      }).catch((e) => { this._setStatus('failed'); throw e; });
      this.info = info;
      this.layout = this.profile.layout(info.model);
      for (const p of this.layout.params) this.params.set(p.path, p.type);
      this._keepAlive();
      this.timers.push(setInterval(() => this._keepAlive(), this.profile.keepAliveMs));
      this.timers.push(setInterval(() => this._watch(), 1000));
      await this._queryAll();
      this._setStatus('online');
      return info;
    }

    disconnect() {
      for (const t of this.timers) clearInterval(t);
      this.timers = [];
      for (const p of this.pending.values()) clearTimeout(p.timer);
      this.pending.clear();
      if (this.offPacket) this.offPacket();
      this.offPacket = null;
      this._setStatus('idle');
    }

    get(path) { return this.values.get(path); }

    // Set a parameter: the local state changes at once, the console gets at most one message per
    // parameter every SEND_GAP_MS, always ending with the latest value.
    set(path, value) {
      const type = this.params.get(path);
      if (!type) throw new Error('unknown parameter ' + path);
      if (type === 'f') value = Levels.quantise(value);
      if (type === 'i') value = value | 0;
      this.values.set(path, value);
      this._emit(path, value, true);
      let p = this.pending.get(path);
      if (!p) { p = { value, timer: null, last: 0 }; this.pending.set(path, p); }
      p.value = value;
      const now = Date.now();
      const flush = () => { p.timer = null; p.last = Date.now(); this._tx(path, [{ t: type, v: p.value }]); };
      if (p.timer) return;
      if (now - p.last >= SEND_GAP_MS) flush();
      else p.timer = setTimeout(flush, SEND_GAP_MS - (now - p.last));
    }

    // Send a one-shot action that is not part of the state (e.g. clear solo).
    action(path, args) { this._tx(path, args || []); }

    on(path, cb) {
      let s = this.listeners.get(path);
      if (!s) this.listeners.set(path, (s = new Set()));
      s.add(cb);
      return () => s.delete(cb);
    }
    onAny(cb) { this.anyListeners.add(cb); return () => this.anyListeners.delete(cb); }
    onMeters(cb) { this.meterListeners.add(cb); return () => this.meterListeners.delete(cb); }
    onStatus(cb) { this.statusListeners.add(cb); return () => this.statusListeners.delete(cb); }

    _tx(address, args) { this.transport.send(this.host, this.port, Osc.encode(address, args)); }

    _rx(bytes, host) {
      if (host !== this.host) return;
      let msgs;
      try { msgs = Osc.decode(bytes); } catch (_) { return; }
      this.lastRx = Date.now();
      if (this.status === 'lost') this._setStatus('online');
      for (const m of msgs) {
        if (m.address === this.profile.infoAddress) {
          const inf = this.profile.parseInfo(m.args);
          if (inf && this.infoWaiter) { const w = this.infoWaiter; this.infoWaiter = null; w(inf); }
          continue;
        }
        if (this.profile.isMeter(m.address)) {
          const blob = m.args[0] && m.args[0].t === 'b' ? m.args[0].v : null;
          // a profile with its own blob format parses it (X32: float32), else X AIR's int16 dB
          const db = this.profile.parseMeter ? this.profile.parseMeter(blob) : Levels.parseMeterBlob(blob);
          if (db) for (const cb of this.meterListeners) cb(m.address, db);
          continue;
        }
        if (!this.params.has(m.address) || !m.args.length) continue;
        // a value the user is dragging right now wins over an echo of an older one
        const p = this.pending.get(m.address);
        if (p && p.timer) continue;
        const v = m.args[0].v;
        if (this.values.get(m.address) === v) continue;
        this.values.set(m.address, v);
        this._emit(m.address, v, false);
      }
      if (this.queryWaiter && this.layout && this.values.size >= this.layout.params.length) this.queryWaiter();
    }

    _emit(path, value, local) {
      const s = this.listeners.get(path);
      if (s) for (const cb of s) cb(value, local);
      for (const cb of this.anyListeners) cb(path, value, local);
    }

    _keepAlive() { for (const [a, args] of this.profile.keepAlive()) this._tx(a, args); }

    _watch() {
      if (this.status === 'online' && Date.now() - this.lastRx > LOST_AFTER_MS) {
        this._setStatus('lost');
        this._tx(this.profile.infoAddress, []);
      }
    }

    // Query every parameter, paced so the console's UDP buffer is not flooded; resolves when all
    // have answered, or after a timeout (missing ones are asked once more, then left empty).
    _queryAll() {
      const paths = this.layout.params.map((p) => p.path);
      const sendSome = (list) => new Promise((resolve) => {
        let i = 0;
        const tick = () => {
          for (let n = 0; n < QUERY_BATCH && i < list.length; n++, i++) this._tx(list[i], []);
          if (i < list.length) setTimeout(tick, QUERY_TICK_MS); else resolve();
        };
        tick();
      });
      const wait = (ms) => new Promise((resolve) => {
        if (this.values.size >= paths.length) { resolve(); return; }
        const t = setTimeout(() => { this.queryWaiter = null; resolve(); }, ms);
        this.queryWaiter = () => { clearTimeout(t); this.queryWaiter = null; resolve(); };
      });
      return sendSome(paths).then(() => wait(1500)).then(() => {
        const missing = paths.filter((p) => !this.values.has(p));
        if (!missing.length) return;
        return sendSome(missing).then(() => wait(1000));
      });
    }

    _setStatus(s) {
      if (this.status === s) return;
      this.status = s;
      for (const cb of this.statusListeners) cb(s);
    }
  }

  return { discover, identify, Session };
});
