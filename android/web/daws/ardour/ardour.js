// Remote for DAWs, Ardour and Mixbus: the OSC session. Talks to Ardour's own OSC control surface
// (Preferences > Control Surfaces > Open Sound Control, UDP 3819), so nothing is installed on the
// computer. Harrison Mixbus is built on Ardour and has the same surface.
//
// Messages and their arguments follow Ardour's libs/surfaces/osc/osc.cc (checked against the
// source, not only the manual):
//   /set_surface bank strip_types feedback gainmode send_page plugin_page port   (all int32)
//   /strip/list -> one "#reply" (or "/reply" with feedback bit 16384) per strip:
//       type, name, inputs, outputs, mute, solo, ssid[, rec]; then "end_route_list", rate, end, monitor
//   /strip/plugin/list ssid -> /strip/plugin/list ssid (piid name enabled)*      piid from 1
//   /strip/plugin/descriptor ssid piid -> per parameter /strip/plugin/descriptor ssid piid par
//       name flags datatype min max print_fmt n (value label)*n current(double);
//       then /strip/plugin/descriptor_end ssid piid. par counts every parameter from 1;
//       flags: 1 enum, 2 integer, 4 log, 32 sample-rate, 64 toggle, 0x80 input, 0x100 hidden
//   /strip/plugin/parameter ssid piid par value   value in the plugin's own range [min, max]
//       (Ardour ignores a value outside it). Ardour sends no feedback for these, so an open
//       plugin is re-read with the descriptor while it is on screen.
//   /strip/fader ssid pos (0..1), /strip/mute|solo|recenable ssid 0/1, /strip/pan_stereo_position
//   feedback on the same paths; /strip/meter ssid v with v = (dB + 94) / 100 when gainmode > 0
//
// The NULL JSFX plugins run in Ardour as VST3 or LV2 under their bare name (Bible R2) with one
// parameter per slider, named like the slider and with the slider's range, so a NULL plugin is
// found by name and drawn with its own interface (web/fx, NULLGfx.View).
//
// Runs in the WebView and in Node (tests). The transport is { send(host, port, bytes),
// onPacket(cb(bytes, host, port)), close() }, as android/web/mixers/core/transport.js gives.
// NULL JSFX • https://nulljsfx.tech • MIT License
(function (root, factory) {
  const api = factory(root);
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.NullArdour = api;
})(typeof self !== 'undefined' ? self : this, function (root) {
  'use strict';

  const Osc = (typeof module === 'object' && module.exports) ? require('../../mixers/core/osc.js') : root.NullOsc;

  const PORT = 3819;
  // strip types: audio tracks 1, MIDI tracks 2, audio busses 4, MIDI busses 8, master 32
  const STRIP_TYPES = 1 | 2 | 4 | 8 | 32;
  // feedback: buttons 1, levels 2, heartbeat 8, master section and transport 16, meters 128,
  // time 2048, "/reply" instead of "#reply" 16384
  const FEEDBACK = 1 | 2 | 8 | 16 | 128 | 2048 | 16384;
  const GAINMODE = 2; // fader position 0..1 and gain in dB
  const SEND_EVERY = 25; // ms: at most one message per control while a finger drags it
  const LINK_TIMEOUT = 4000; // ms without any packet: Ardour is gone (the heartbeat is ~1 s)
  const POLL_PARAMS = 800; // ms between re-reads of the open plugin

  const FLAG = { ENUM: 1, INTEGER: 2, LOG: 4, TOGGLE: 64, INPUT: 0x80, HIDDEN: 0x100 };

  const num = (a) => (a == null ? 0 : typeof a.v === 'boolean' ? (a.v ? 1 : 0) : Number(a.v));
  const str = (a) => (a == null ? '' : String(a.v));

  /** The fader position (0..1) Ardour uses, as dB, and back (gain_to_slider_position). */
  function faderToDb(pos) {
    if (pos <= 0) return -Infinity;
    const g = Math.pow(2, (Math.sqrt(Math.sqrt(Math.sqrt(pos))) * 198.0 - 192.0) / 6.0);
    return 20 * Math.log10(g);
  }
  function dbToFader(db) {
    if (!(db > -200)) return 0;
    const g = Math.pow(10, db / 20);
    const p = (6.0 * Math.log(g) / Math.log(2.0) + 192.0) / 198.0;
    return Math.min(1, Math.max(0, Math.pow(p, 8.0)));
  }

  /** The NULL JSFX effect an Ardour plugin name stands for, from web/fx/index.json, or null. */
  function nullEffect(name, effects) {
    if (!name || !effects) return null;
    const bare = String(name).replace(/^NULL\s+/i, '').replace(/\s+GFX$/i, '').trim().toLowerCase();
    return effects.find((e) => e.name.toLowerCase() === bare) || null;
  }

  /**
   * Pairs the sliders of a NULL effect (FX.sliders: {i, name, min, max}) with the plugin's
   * Ardour parameters: by name first, then the remaining inputs in order. Returns a Map
   * slider index -> parameter.
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

  class Session {
    constructor(opts) {
      this.t = opts.transport;
      this.host = opts.host;
      this.port = opts.port || PORT;
      this.now = opts.now || (() => Date.now());
      this.timers = opts.timers || { set: (fn, ms) => setInterval(fn, ms), clear: (h) => clearInterval(h) };
      this.handlers = new Map();
      this.strips = new Map(); // ssid -> strip
      this.listing = null; // the strips of a /strip/list reply in progress
      this.early = new Map(); // feedback for strips not listed yet
      this.rate = 0;
      this.plugins = new Map(); // ssid -> [{piid, name, enabled}]
      this.params = new Map(); // "ssid/piid" -> [{par, name, flags, type, min, max, fmt, points, value}]
      this.reading = new Map(); // "ssid/piid" -> params being received
      this.transport = { play: false, stop: true, rec: false, loop: false, time: '' };
      this.heartbeat = 0;
      this.lastPacket = 0;
      this.up = false;
      this.pending = new Map(); // path key -> {bytes, at}
      this.lastSent = new Map();
      this.touched = new Map(); // "ssid/piid/par" -> time of the last local change
      this.open = null; // {ssid, piid} re-read while shown
      this.unhook = this.t.onPacket((bytes) => this.receive(bytes));
      this.tick = this.timers.set(() => this.poll(), 50);
      this.lastPoll = 0;
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

    send(address, args) {
      this.t.send(this.host, this.port, Osc.encode(address, args || []));
    }
    // a control a finger is dragging: the newest value goes out at most every SEND_EVERY ms
    sendSoon(key, address, args) {
      const bytes = Osc.encode(address, args);
      const last = this.lastSent.get(key) || 0;
      if (this.now() - last >= SEND_EVERY) {
        this.t.send(this.host, this.port, bytes);
        this.lastSent.set(key, this.now());
        this.pending.delete(key);
      } else {
        this.pending.set(key, bytes);
      }
    }

    /** Introduces this device to Ardour and asks for the strips. */
    connect() {
      this.send('/set_surface', [0, STRIP_TYPES, FEEDBACK, GAINMODE, 0, 0, 0].map((v) => ({ t: 'i', v })));
      this.send('/strip/list');
      this.asked = this.now();
    }

    close() {
      this.timers.clear(this.tick);
      if (this.unhook) this.unhook();
      this.t.close && this.t.close();
    }

    poll() {
      const now = this.now();
      for (const [key, bytes] of this.pending) {
        if (now - (this.lastSent.get(key) || 0) >= SEND_EVERY) {
          this.t.send(this.host, this.port, bytes);
          this.lastSent.set(key, now);
          this.pending.delete(key);
        }
      }
      const alive = this.lastPacket && now - this.lastPacket < LINK_TIMEOUT;
      if (alive !== this.up) {
        this.up = alive;
        this.emit('link', alive);
      }
      // not answered yet (Ardour started later, or the surface is off): ask again
      if (!alive && this.asked && now - this.asked > 2000) this.connect();
      if (this.open && now - this.lastPoll >= POLL_PARAMS && !this.reading.has(this.open.ssid + '/' + this.open.piid)) {
        this.lastPoll = now;
        this.send('/strip/plugin/descriptor', [{ t: 'i', v: this.open.ssid }, { t: 'i', v: this.open.piid }]);
      }
    }

    receive(bytes) {
      let msgs;
      try { msgs = Osc.decode(bytes); } catch (e) { return; }
      this.lastPacket = this.now();
      for (const m of msgs) this.handle(m.address, m.args);
    }

    handle(addr, a) {
      if (addr === '/reply' || addr === '#reply') return this.reply(a);
      if (addr === '/heartbeat') { this.heartbeat = num(a[0]); return; }
      if (addr.startsWith('/strip/plugin/')) return this.pluginMessage(addr, a);
      if (addr.startsWith('/strip/')) return this.stripFeedback(addr.slice(7), a);
      const tr = { '/transport_play': 'play', '/transport_stop': 'stop', '/rec_enable_toggle': 'rec', '/loop_toggle': 'loop' }[addr];
      if (tr) { this.transport[tr] = num(a[0]) > 0.5; this.emit('transport', this.transport); return; }
      if (addr === '/position/time') { this.transport.time = str(a[0]).trim(); this.emit('time', this.transport.time); }
    }

    reply(a) {
      if (str(a[0]) === 'end_route_list') {
        this.rate = num(a[1]);
        const list = this.listing || new Map();
        this.listing = null;
        // keep what feedback already told us (fader, pan, meter) for strips that stayed
        for (const [ssid, s] of list) {
          const old = this.strips.get(ssid);
          if (old && old.name === s.name) Object.assign(s, { fader: old.fader, gain: old.gain, pan: old.pan, meter: old.meter });
        }
        for (const [ssid, e] of this.early) {
          const st = list.get(ssid);
          if (st) for (const k of ['fader', 'gain', 'pan', 'meter']) if (e[k] != null) st[k] = e[k];
        }
        this.early.clear();
        this.strips = list;
        this.emit('strips', [...list.values()]);
        return;
      }
      if (a.length < 7) return;
      if (!this.listing) this.listing = new Map();
      const s = {
        ssid: num(a[6]), type: str(a[0]), name: str(a[1]), ins: num(a[2]), outs: num(a[3]),
        mute: num(a[4]) > 0.5, solo: num(a[5]) > 0.5, rec: a.length > 7 ? num(a[7]) > 0.5 : null,
        canRec: a.length > 7, fader: null, gain: null, pan: null, meter: 0,
      };
      this.listing.set(s.ssid, s);
    }

    stripFeedback(what, a) {
      const ssid = num(a[0]);
      let s = this.strips.get(ssid);
      if (!s && this.listing) s = this.listing.get(ssid);
      // Ardour sends every strip's state right after /set_surface, before the list arrives:
      // keep it for the strip it belongs to
      if (!s) {
        if (!this.early.has(ssid)) this.early.set(ssid, {});
        s = this.early.get(ssid);
      }
      const v = a[1];
      switch (what) {
        case 'name': s.name = str(v); break;
        case 'mute': s.mute = num(v) > 0.5; break;
        case 'solo': s.solo = num(v) > 0.5; break;
        case 'recenable': s.rec = num(v) > 0.5; break;
        case 'fader': if (!this.dragging(s.ssid, 'fader')) s.fader = num(v); break;
        case 'gain': s.gain = num(v); break;
        case 'pan_stereo_position': if (!this.dragging(s.ssid, 'pan')) s.pan = num(v); break;
        case 'meter': s.meter = Math.max(0, num(v)); if (s.ssid) this.emit('meter', s); return;
        default: return;
      }
      if (s.ssid) this.emit('strip', s);
    }

    dragging(ssid, what) {
      return this.now() - (this.touched.get(ssid + '/' + what) || 0) < 400;
    }

    pluginMessage(addr, a) {
      const ssid = num(a[0]);
      if (addr === '/strip/plugin/list') {
        const list = [];
        for (let i = 1; i + 1 < a.length; i += 3) {
          list.push({ piid: num(a[i]), name: str(a[i + 1]), enabled: a[i + 2] == null ? true : num(a[i + 2]) > 0.5 });
        }
        this.plugins.set(ssid, list);
        this.emit('plugins', ssid, list);
        return;
      }
      const key = ssid + '/' + num(a[1]);
      if (addr === '/strip/plugin/descriptor') {
        const n = num(a[9]);
        const points = [];
        for (let k = 0; k < n; k++) points.push([num(a[10 + 2 * k]), str(a[11 + 2 * k])]);
        const p = {
          par: num(a[2]), name: str(a[3]), flags: num(a[4]), type: str(a[5]),
          min: num(a[6]), max: num(a[7]), fmt: str(a[8]), points, value: num(a[10 + 2 * n]),
        };
        if (!this.reading.has(key)) this.reading.set(key, []);
        this.reading.get(key).push(p);
        return;
      }
      if (addr === '/strip/plugin/descriptor_end') {
        const fresh = this.reading.get(key) || [];
        this.reading.delete(key);
        const old = this.params.get(key);
        // a parameter the user is moving keeps the value we sent until Ardour has it
        if (old) {
          for (const p of fresh) {
            const o = old.find((q) => q.par === p.par);
            if (o && this.now() - (this.touched.get(key + '/' + p.par) || 0) < 1500) p.value = o.value;
          }
        }
        this.params.set(key, fresh);
        this.emit('params', ssid, num(a[1]), fresh);
      }
    }

    // --- requests ---------------------------------------------------------------------------

    listStrips() { this.send('/strip/list'); }
    listPlugins(ssid) { this.send('/strip/plugin/list', [{ t: 'i', v: ssid }]); }
    readPlugin(ssid, piid) {
      this.send('/strip/plugin/descriptor', [{ t: 'i', v: ssid }, { t: 'i', v: piid }]);
      this.lastPoll = this.now();
    }
    /** Keeps re-reading this plugin while it is on screen (Ardour sends no feedback for it). */
    watchPlugin(ssid, piid) {
      this.open = ssid == null ? null : { ssid, piid };
      if (this.open) this.readPlugin(ssid, piid);
    }

    setFader(ssid, pos) {
      const s = this.strips.get(ssid);
      pos = Math.min(1, Math.max(0, pos));
      if (s) s.fader = pos;
      this.touched.set(ssid + '/fader', this.now());
      this.sendSoon('f' + ssid, '/strip/fader', [{ t: 'i', v: ssid }, { t: 'f', v: pos }]);
    }
    setPan(ssid, pos) {
      const s = this.strips.get(ssid);
      pos = Math.min(1, Math.max(0, pos));
      if (s) s.pan = pos;
      this.touched.set(ssid + '/pan', this.now());
      this.sendSoon('p' + ssid, '/strip/pan_stereo_position', [{ t: 'i', v: ssid }, { t: 'f', v: pos }]);
    }
    resetFader(ssid) { this.setFader(ssid, dbToFader(0)); }
    resetPan(ssid) { this.setPan(ssid, 0.5); }
    setMute(ssid, on) { this.button(ssid, 'mute', on); }
    setSolo(ssid, on) { this.button(ssid, 'solo', on); }
    setRec(ssid, on) { this.button(ssid, 'recenable', on, 'rec'); }
    button(ssid, path, on, field) {
      const s = this.strips.get(ssid);
      if (s) s[field || path] = !!on;
      this.send('/strip/' + path, [{ t: 'i', v: ssid }, { t: 'i', v: on ? 1 : 0 }]);
    }

    /** value in the parameter's own range */
    setParam(ssid, piid, par, value) {
      const key = ssid + '/' + piid;
      const p = (this.params.get(key) || []).find((q) => q.par === par);
      if (p) {
        value = clamp(value, p.min, p.max);
        p.value = value;
      }
      this.touched.set(key + '/' + par, this.now());
      this.sendSoon('x' + key + '/' + par, '/strip/plugin/parameter',
        [{ t: 'i', v: ssid }, { t: 'i', v: piid }, { t: 'i', v: par }, { t: 'f', v: value }]);
    }
    setPluginActive(ssid, piid, on) {
      const list = this.plugins.get(ssid) || [];
      const p = list.find((q) => q.piid === piid);
      if (p) p.enabled = !!on;
      this.send(on ? '/strip/plugin/activate' : '/strip/plugin/deactivate', [{ t: 'i', v: ssid }, { t: 'i', v: piid }]);
    }

    play() { this.send('/transport_play'); }
    stop() { this.send('/transport_stop'); }
    toStart() { this.send('/goto_start'); }
    record() { this.send('/rec_enable_toggle'); }
    loop() { this.send('/loop_toggle'); }
  }

  return { Session, PORT, FLAG, nullEffect, mapSliders, toParam, fromParam, faderToDb, dbToFader };
});
