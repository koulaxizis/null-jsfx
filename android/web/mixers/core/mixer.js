// NULL Remote, Mixers: one live session with an OSC console, independent of the model family.
// A profile (xair/, x32/, wing/profile.js) supplies the port, discovery and info messages,
// the keep-alive messages and the layout (strips and the typed OSC parameters); this file keeps
// the state, queries every parameter on connect (and again after a lost link comes back), applies
// updates the console pushes, sends changes (coalesced per parameter), renews subscriptions and
// watches the link. Made for lossy links too (a VPN, busy Wi-Fi): the query keeps a few questions
// in flight and asks each one again until it is answered, and a quiet link is pinged before it
// counts as lost.
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
  const QUERY_WINDOW = 64;    // questions in flight at once while loading the state: each answer
                              // lets the next one go, so the pace follows the link and the console
  const QUERY_RETRY_MS = 150; // a question unanswered for 3 round trips (at least this long) is
  const QUERY_TRIES = 12;     // asked again, up to this many times, then left empty
  const QUERY_TICK_MS = 50;
  const PING_AFTER_MS = 1500; // no packet for this long: ask the console who it is, once a second
  const LOST_AFTER_MS = 5000; // no packet for this long: the link is lost
  const WATCH_MS = 500;

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
      this.lastPing = 0;
      this.query = null;            // the state load in progress: {waiting, queue, finish}
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
      this.timers.push(setInterval(() => this._watch(), WATCH_MS));
      await this._queryAll();
      this._setStatus('online');
      return info;
    }

    disconnect() {
      for (const t of this.timers) clearInterval(t);
      this.timers = [];
      if (this.query) this.query.finish();
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
      const flush = () => { p.timer = null; p.last = Date.now(); this._tx(path, this.profile.toWire ? this.profile.toWire(path, p.value, type) : [{ t: type, v: p.value }]); };
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
      if (this.status === 'lost') {
        // back after a gap: renew the subscriptions now and read everything again, since the
        // console pushed nothing to us while the link was down
        this._setStatus('online');
        this._keepAlive();
        if (!this.query) this._queryAll();
      }
      const q = this.query;
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
        if (q) q.answer(m.address);
        // a value the user is dragging right now wins over an echo of an older one
        const p = this.pending.get(m.address);
        if (p && p.timer) continue;
        // a profile in other units (WING: dB, -100..100 pan, 1 = muted) converts to the tab's
        const v = this.profile.fromWire ? this.profile.fromWire(m.address, m.args) : m.args[0].v;
        if (v === undefined || this.values.get(m.address) === v) continue;
        this.values.set(m.address, v);
        this._emit(m.address, v, false);
      }
      if (q) q.pump();
    }

    _emit(path, value, local) {
      const s = this.listeners.get(path);
      if (s) for (const cb of s) cb(value, local);
      for (const cb of this.anyListeners) cb(path, value, local);
    }

    _keepAlive() { for (const [a, args] of this.profile.keepAlive()) this._tx(a, args); }

    // Ping a quiet link before calling it lost: a meter stream can stop for a moment when one
    // renewal is lost on the way, and a console with nothing changing sends nothing else. While
    // lost, the subscriptions are renewed with every ping so the stream starts again at once.
    _watch() {
      const now = Date.now();
      const quiet = now - this.lastRx;
      if (this.status === 'online' && quiet > LOST_AFTER_MS) this._setStatus('lost');
      if (quiet > PING_AFTER_MS && now - this.lastPing >= 1000) {
        this.lastPing = now;
        this._tx(this.profile.infoAddress, []);
        if (this.status === 'lost') this._keepAlive();
      }
    }

    // Load the value of every parameter. At most QUERY_WINDOW questions are in flight: each
    // answer lets the next one go, an unanswered one is asked again after three round trips (at
    // least QUERY_RETRY_MS), up to QUERY_TRIES times. Resolves when every parameter has answered
    // or given up, or when the link has been quiet for LOST_AFTER_MS (the watch then reports it
    // lost, and the load runs again when the console is back).
    _queryAll() {
      const queue = this.layout.params.map((p) => p.path);
      const waiting = new Map(); // path -> {tries, at}
      let rtt = 0;               // smoothed round trip of first answers, for the retry time
      return new Promise((resolve) => {
        let timer = null;
        const q = {
          answer: (path) => {
            const w = waiting.get(path);
            if (!w) return;
            waiting.delete(path);
            if (w.tries === 1) { const t = Date.now() - w.at; rtt = rtt ? 0.875 * rtt + 0.125 * t : t; }
          },
          finish: () => {
            if (this.query !== q) return;
            this.query = null;
            clearInterval(timer);
            resolve();
          },
          pump: () => {
            if (this.query !== q) return;
            const now = Date.now();
            if (now - this.lastRx > LOST_AFTER_MS) { q.finish(); return; }
            const retry = Math.max(QUERY_RETRY_MS, 3 * rtt);
            for (const [path, w] of waiting) {
              if (now - w.at < retry) continue;
              if (w.tries >= QUERY_TRIES) { waiting.delete(path); continue; }
              w.tries++;
              w.at = now;
              this._tx(path, []);
            }
            while (waiting.size < QUERY_WINDOW && queue.length) {
              const path = queue.shift();
              waiting.set(path, { tries: 1, at: now });
              this._tx(path, []);
            }
            if (!waiting.size && !queue.length) q.finish();
          },
        };
        if (this.query) this.query.finish();
        this.query = q;
        timer = setInterval(q.pump, QUERY_TICK_MS);
        q.pump();
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
