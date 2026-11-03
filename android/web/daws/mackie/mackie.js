// Remote for DAWs, Mackie Control (beta): the session. The phone plays a Mackie Control
// Universal surface over network MIDI (../core/rtpmidi.js), so any DAW that drives a Mackie
// Control (Logic Pro, Studio One, Pro Tools, Cakewalk, Reason, Digital Performer and others)
// shows its mixer here without a script of ours: eight strips at a time, the master fader and
// the transport. The DAW decides what a strip is; there are no plugins in this mode (the other
// tabs reach the NULL JSFX plugins through our own scripts).
//
// The protocol, as published for the Logic Control / Mackie Control and as every DAW above
// reads it (MIDI channel 1 unless said):
//   Faders 1-8 and master: pitch bend on channels 1-8 and 9, 14 bits both ways; touch: note
//     0x68 + n, velocity 0x7F touched, 0 let go.
//   Buttons, notes 0x7F pressed then 0 released; the DAW lights them with the same note
//     (0x7F on, 1 flashing, 0 off): record arm 0x00 + n, solo 0x08 + n, mute 0x10 + n,
//     select 0x18 + n, V-pot push 0x20 + n, assign Pan 0x2A, Bank left/right 0x2E/0x2F,
//     Cycle 0x56, Rewind 0x5B, Fast forward 0x5C, Stop 0x5D, Play 0x5E, Record 0x5F.
//   V-pots (pan here): CC 0x10 + n relative, 0x01..0x0F clockwise, 0x41..0x4F anticlockwise;
//     their rings from the DAW: CC 0x30 + n, bits 0-3 the lit LED (0 none, 1..11), bits 4-5
//     the mode (0 dot), bit 6 the centre LED.
//   Meters: channel pressure, (n << 4) | level, level 0..0x0C, 0x0E overload, 0x0F clear; a
//     surface lets a meter fall by itself, about one LED every 300 ms.
//   Display: SysEx F0 00 00 66 14 12 <offset 0..111> <ASCII> F7, two rows of 56 characters,
//     7 per strip: the top row holds the names.
//   Time: CC 0x40..0x49, the ten digits from the right; value & 0x3F a character (0x00-0x1F
//     stand for 0x40-0x5F, 0x20-0x3F for themselves), 0x40 its dot.
//   Hello: the surface says F0 00 00 66 14 01 <serial 7> <challenge 4> F7 when it comes up or
//     when the DAW asks (F0 00 00 66 14 00 F7); the DAW answers 02 <serial> <response 4>, the
//     surface confirms 03 <serial>. Its firmware version: the DAW asks 13 00, the surface
//     answers 14 <ASCII>. The DAW then sends everything again.
// Not tried with a DAW yet (Bible C26): what each DAW does with Bank, a second Stop or a V-pot
// push is its own.
//
// Runs in the WebView and in Node (tests): opts {host, port, udp (rtpmidi.js's), now, timers}.
// NULL JSFX • https://nulljsfx.tech • MIT License
(function (root, factory) {
  const R = typeof module === 'object' && module.exports ? require('../core/rtpmidi.js') : root.NullRtpMidi;
  const api = factory(root, R);
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.NullMackie = api;
})(typeof self !== 'undefined' ? self : this, function (root, R) {
  'use strict';

  const PORT = 5004;
  const STRIPS = 8;
  const MASTER = 'master';
  const SEND_EVERY = 20; // ms between two moves of one fader
  const RELEASE = 300; // ms after the last move a fader is let go (touch off)
  const KEEP_LOCAL = 500; // ms a control the user moved ignores values coming back
  const REDIAL = 2000;
  const METER_FALL = 300; // ms per meter LED
  const PAN_TICKS = 64; // V-pot ticks for the whole pan range (the DAW decides one tick's size)
  const HEAD = [0xf0, 0x00, 0x00, 0x66, 0x14];
  const SERIAL = [0x4e, 0x55, 0x4c, 0x4c, 0x4a, 0x53, 0x58]; // "NULLJSX"
  const VERSION = 'V1.02';
  const N = { REC: 0x00, SOLO: 0x08, MUTE: 0x10, PUSH: 0x20, PAN: 0x2a, BANK_L: 0x2e, BANK_R: 0x2f,
    CYCLE: 0x56, REW: 0x5b, STOP: 0x5d, PLAY: 0x5e, RECORD: 0x5f, TOUCH: 0x68 };

  /** The 7-character name of strip n in the display's top row, without its spaces. */
  function nameOf(lcd, n) { return lcd.slice(7 * n, 7 * n + 7).join('').trim(); }

  /** The time display: ten characters, a dot after those that have one. */
  function timeText(digits) {
    let s = '';
    for (let i = 9; i >= 0; i--) {
      const v = digits[i];
      const c = v & 0x3f;
      s += String.fromCharCode(c < 0x20 ? c + 0x40 : c);
      if (v & 0x40) s += '.';
    }
    return s.trim();
  }

  class Session {
    constructor(opts) {
      this.opts = opts;
      this.now = opts.now || (() => Date.now());
      this.timers = opts.timers || { set: (fn, ms) => setInterval(fn, ms), clear: (h) => clearInterval(h) };
      this.handlers = new Map();
      this.strips = new Map();
      this.rate = 0;
      this.transport = { play: false, stop: true, rec: false, loop: false };
      this.time = '';
      this.from = 0; // tracks banked past, as counted here (the DAW does not say)
      this.size = STRIPS;
      this.total = 0;
      this.more = true; // the DAW never says how many tracks it has
      this.lcd = new Array(112).fill(' ');
      this.digits = new Array(10).fill(0x20);
      for (let n = 0; n < STRIPS; n++) this.strips.set(n, this._strip(n));
      this.strips.set(MASTER, { ssid: MASTER, name: 'Master', type: 'master', mute: false, solo: false, rec: false, canRec: false, fader: 0, pan: 0.5, meter: 0 });
      this.level = new Array(STRIPS).fill(0); // meter LEDs 0..12
      this.touched = new Map();
      this.touching = new Set(); // faders held down in the DAW (touch on sent)
      this.lastSent = new Map();
      this.pending = new Map();
      this.later = new Map();
      this.panSent = new Map(); // ssid -> the pan the ticks sent so far stand for
      this.midi = null;
      this.up = false;
      this.closed = true;
      this.heard = false; // the DAW has sent something since the link came up
      this.listed = '';
      this.stripsDue = false;
      this.retryAt = 0;
      this.lastFall = 0;
    }
    _strip(n) {
      return { ssid: n, name: '', type: '', mute: false, solo: false, rec: false, canRec: true, fader: 0, pan: 0.5, meter: 0 };
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
      if (!this.tick) this.tick = this.timers.set(() => this.poll(), 25);
      if (this.midi) return;
      const s = new R.Session({ udp: this.opts.udp, name: this.opts.name || 'NULL Remote' });
      this.midi = s;
      s.onMidi((m) => { if (this.midi === s) this.handle(m); });
      s.onClose(() => { if (this.midi === s) this.dropped(); });
      s.connect(this.opts.host, this.opts.port || PORT).then(() => {
        if (this.midi !== s) return;
        this.up = true;
        this.emit('link', true);
        // a surface that comes up says hello, puts its V-pots on pan, and the DAW sends it all
        this.send([this.hello()]);
        this.press(N.PAN);
      }, () => { if (this.midi === s) this.dropped(); });
    }
    dropped() {
      this.midi = null;
      if (this.up) { this.up = false; this.emit('link', false); }
      this.retryAt = this.now() + REDIAL;
    }
    close() {
      this.closed = true;
      if (this.tick) { this.timers.clear(this.tick); this.tick = null; }
      if (this.midi) { const s = this.midi; this.midi = null; s.close(); }
      this.up = false;
    }

    send(messages) { if (this.midi && this.up) this.midi.send(messages); }
    hello() { return HEAD.concat([0x01], SERIAL, [0, 0, 0, 0, 0xf7]); }
    press(note) { this.send([[0x90, note, 0x7f], [0x90, note, 0]]); }

    // --- from the DAW ---------------------------------------------------------------------

    handle(m) {
      if (!this.heard) { this.heard = true; this.stripsDue = true; }
      const st = m[0] & 0xf0;
      const ch = m[0] & 0x0f;
      if (m[0] === 0xf0) { this.sysex(m); return; }
      if (st === 0xe0) { // a fader
        const ssid = ch < STRIPS ? ch : ch === STRIPS ? MASTER : null;
        if (ssid == null) return;
        const v = ((m[2] << 7) | m[1]) / 16383;
        if (this.take('fader/' + ssid, v)) this.set(ssid, 'fader', v);
        return;
      }
      if (st === 0x90 && ch === 0) { this.led(m[1], m[2]); return; }
      if (st === 0xb0 && ch === 0) {
        const c = m[1];
        if (c >= 0x30 && c < 0x30 + STRIPS) { // a V-pot ring: the pan
          const pos = m[2] & 0x0f;
          if (!pos) return;
          const n = c - 0x30;
          const v = (pos - 1) / 10;
          if (this.take('pan/' + n, v)) { this.panSent.set(n, v); this.set(n, 'pan', v); }
        } else if (c >= 0x40 && c <= 0x49) {
          this.digits[c - 0x40] = m[2];
          const t = timeText(this.digits);
          if (t !== this.time) { this.time = t; this.emit('time', t); }
        }
        return;
      }
      if (st === 0xd0) { // a meter
        const n = m[1] >> 4;
        const lv = m[1] & 0x0f;
        if (n >= STRIPS) return;
        if (lv <= 0x0c) this.level[n] = lv;
        else if (lv === 0x0e) this.level[n] = 12;
        else return;
        this.meterOut(n);
      }
    }

    sysex(m) {
      if (m.length < 7 || HEAD.some((b, i) => m[i] !== b)) return;
      const cmd = m[5];
      if (cmd === 0x12) { // the display
        let off = m[6];
        for (let i = 7; i < m.length - 1 && off < 112; i++, off++) this.lcd[off] = String.fromCharCode(m[i] & 0x7f);
        this.stripsDue = true;
      } else if (cmd === 0x00) {
        this.send([this.hello()]); // the DAW asks who is there
      } else if (cmd === 0x02) {
        this.send([HEAD.concat([0x03], SERIAL, [0xf7])]); // its answer: we take it
      } else if (cmd === 0x13) {
        this.send([HEAD.concat([0x14], Array.from(VERSION, (c) => c.charCodeAt(0)), [0xf7])]);
      }
    }

    led(note, vel) {
      const on = vel > 0;
      const t = this.transport;
      const bank = (base, key) => {
        if (note >= base && note < base + STRIPS) { this.set(note - base, key, on); return true; }
        return false;
      };
      if (bank(N.REC, 'rec') || bank(N.SOLO, 'solo') || bank(N.MUTE, 'mute')) return;
      const k = { [N.PLAY]: 'play', [N.STOP]: 'stop', [N.RECORD]: 'rec', [N.CYCLE]: 'loop' }[note];
      if (k && t[k] !== on) { t[k] = on; this.emit('transport', t); }
    }

    set(ssid, key, v) {
      const s = this.strips.get(ssid);
      if (!s || s[key] === v) return;
      s[key] = v;
      this.emit('strip', s);
    }
    meterOut(n) {
      const s = this.strips.get(n);
      const v = this.level[n] / 12;
      if (s.meter === v) return;
      s.meter = v;
      this.emit('meter', s);
    }

    // the strips the DAW shows: those it named, then the master
    list() {
      const named = [];
      for (let n = 0; n < STRIPS; n++) {
        const s = this.strips.get(n);
        s.name = nameOf(this.lcd, n);
        if (s.name) named.push(s);
      }
      this.total = this.from + named.length + (named.length === STRIPS ? STRIPS : 0);
      this.more = named.length === STRIPS;
      return named.concat([this.strips.get(MASTER)]);
    }

    poll() {
      const now = this.now();
      for (const [key, msgs] of this.pending) {
        if (now - (this.lastSent.get(key) || 0) >= SEND_EVERY) { this.send(msgs); this.lastSent.set(key, now); this.pending.delete(key); }
      }
      // touch off for a fader let go
      for (const key of this.touching) {
        if (now - (this.touched.get(key) || 0) < RELEASE || this.pending.has(key)) continue;
        this.touching.delete(key);
        const ssid = key.slice(6);
        this.send([[0x90, N.TOUCH + (ssid === MASTER ? STRIPS : Number(ssid)), 0]]);
      }
      for (const [key, v] of this.later) {
        if (this.held(key)) continue;
        this.later.delete(key);
        const [kind, id] = key.split('/');
        const ssid = id === MASTER ? MASTER : Number(id);
        if (kind === 'pan') this.panSent.set(ssid, v);
        this.set(ssid, kind, v);
      }
      if (now - this.lastFall >= METER_FALL) {
        this.lastFall = now;
        for (let n = 0; n < STRIPS; n++) if (this.level[n] > 0) { this.level[n]--; this.meterOut(n); }
      }
      if (this.stripsDue && this.heard) {
        this.stripsDue = false;
        const l = this.list();
        const key = l.map((s) => s.name).join('|') + '|' + this.from;
        if (key !== this.listed) { this.listed = key; this.emit('strips', l); }
      }
      if (!this.midi && !this.closed && now >= this.retryAt) this.connect();
    }

    held(key) { return this.now() - (this.touched.get(key) || -1e9) < KEEP_LOCAL; }
    take(key, v) {
      if (!this.held(key)) { this.later.delete(key); return true; }
      this.later.set(key, v);
      return false;
    }

    // --- to the DAW -----------------------------------------------------------------------

    setFader(ssid, v) {
      const s = this.strips.get(ssid);
      if (!s) return;
      v = Math.max(0, Math.min(1, v));
      s.fader = v;
      const key = 'fader/' + ssid;
      const ch = ssid === MASTER ? STRIPS : ssid;
      const first = !this.touching.has(key);
      this.touching.add(key);
      this.touched.set(key, this.now());
      const raw = Math.round(v * 16383);
      const msgs = [[0xe0 | ch, raw & 0x7f, raw >> 7]];
      if (first) msgs.unshift([0x90, N.TOUCH + ch, 0x7f]);
      if (this.now() - (this.lastSent.get(key) || 0) >= SEND_EVERY) { this.send(msgs); this.lastSent.set(key, this.now()); this.pending.delete(key); }
      else this.pending.set(key, (this.pending.get(key) || []).filter((x) => x[0] === 0x90).concat(msgs));
    }
    resetFader() {} // Mackie Control has no "back to 0 dB"
    setPan(ssid, v) {
      if (ssid === MASTER) return;
      const s = this.strips.get(ssid);
      if (!s) return;
      v = Math.max(0, Math.min(1, v));
      s.pan = v;
      this.touched.set('pan/' + ssid, this.now());
      const was = this.panSent.has(ssid) ? this.panSent.get(ssid) : 0.5;
      let ticks = Math.round((v - was) * PAN_TICKS);
      if (!ticks) return;
      this.panSent.set(ssid, was + ticks / PAN_TICKS);
      const out = [];
      while (ticks) {
        const k = Math.max(-15, Math.min(15, ticks));
        out.push([0xb0, 0x10 + ssid, k > 0 ? k : 0x40 | -k]);
        ticks -= k;
      }
      this.send(out);
    }
    resetPan(ssid) { // most DAWs centre the pan when its V-pot is pushed in pan mode
      if (ssid === MASTER) return;
      this.touched.delete('pan/' + ssid);
      this.press(N.PUSH + ssid);
    }
    toggle(ssid, base, key, on) {
      if (ssid === MASTER) return;
      const s = this.strips.get(ssid);
      if (!s || s[key] === on) return;
      this.press(base + ssid); // a toggle in the DAW; its light comes back
    }
    setMute(ssid, on) { this.toggle(ssid, N.MUTE, 'mute', on); }
    setSolo(ssid, on) { this.toggle(ssid, N.SOLO, 'solo', on); }
    setRec(ssid, on) { this.toggle(ssid, N.REC, 'rec', on); }
    play() { this.press(N.PLAY); }
    stop() { this.press(N.STOP); }
    toStart() { this.press(N.STOP); this.press(N.STOP); } // a second Stop: back to the start in most DAWs
    record() { this.press(N.RECORD); }
    loop() { this.press(N.CYCLE); }

    /** Pages by a bank of eight: Bank left or right once per eight tracks. */
    scroll(from) {
      const steps = Math.round((Math.max(0, from) - this.from) / STRIPS);
      if (!steps) return;
      for (let i = 0; i < Math.abs(steps); i++) this.press(steps > 0 ? N.BANK_R : N.BANK_L);
      this.from = Math.max(0, this.from + steps * STRIPS);
      this.stripsDue = true;
    }

    // no plugins in this mode
    listPlugins(ssid) { setTimeout(() => this.emit('plugins', ssid, []), 0); }
    watchPlugin() {}
    setParam() {}
    setPluginActive() {}
  }

  return { PORT, STRIPS, MASTER, Session, timeText, nameOf, SERIAL };
});
