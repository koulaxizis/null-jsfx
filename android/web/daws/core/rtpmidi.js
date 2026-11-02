// Remote for DAWs: network MIDI (RTP-MIDI, Apple's session protocol as in RFC 6295 and Apple's
// "RTP-MIDI" network driver), for DAWs whose remote scripts can only speak MIDI (Cubase's MIDI
// Remote, FL Studio's MIDI scripts). The phone is the initiator; on the computer a network MIDI
// session listens: macOS has one built in (Audio MIDI Setup > MIDI Network Setup), Windows takes
// Tobias Erichsen's free rtpMIDI. Port N is the control port, N + 1 the data port (5004 by
// default). Our DAW scripts speak the Bitwig script's protocol v1 (../bitwig/bitwig.js), one
// JSON object per System Exclusive message:
//     F0 7D 4E 4A <the JSON, ASCII only: other characters as \uXXXX> F7
// (7D is MIDI's ID for non-commercial use; 4E 4A is "NJ"). dial() turns that into the byte
// stream the Bitwig session reads, so that session runs unchanged over network MIDI.
//
//   Session control (big endian): FF FF, command ('IN' invitation, 'OK', 'NO', 'BY', 'CK'
//   clock sync), then for IN/OK/NO/BY: version 2, initiator token, SSRC, name (NUL ended);
//   for CK: SSRC, count 0..2, 3 bytes padding, three 64-bit times in 100 µs units.
//   MIDI (data port): RTP header (80 61, sequence, 32-bit time at 10 kHz, SSRC), then the
//   MIDI command section: B J Z P LEN (B = 12-bit length), commands with delta times before
//   every command but the first (Z = 0), running status; a journal after it (J) is skipped,
//   since this session sends none and needs none on a LAN. A System Exclusive message longer
//   than one packet goes in segments: F0 .. F0, F7 .. F0, F7 .. F7.
//
// Runs in the WebView and in Node (tests). opts.udp(localPort) opens a UDP socket and resolves
// to {send(host, port, bytes), onPacket(cb(bytes, host, port)), close()} or null when the port
// is taken: shellUdp() in the app, dgram in the tests.
// NULL JSFX • https://nulljsfx.tech • MIT License
(function (root, factory) {
  const api = factory(root);
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.NullRtpMidi = api;
})(typeof self !== 'undefined' ? self : this, function (root) {
  'use strict';

  const PORT = 5004;
  const VERSION = 2;
  const MAX_SYSEX = 1000; // bytes of one System Exclusive segment in a packet
  const CK_EVERY = [500, 1000, 2000, 5000, 10000]; // ms between clock syncs, then 10 s
  const LOST_MS = 25000; // no answer to the clock sync for this long: the session is gone
  const INVITE_TRIES = 6;
  const INVITE_MS = 1000;
  const HEADER = [0xf0, 0x7d, 0x4e, 0x4a];
  const cmd = (s) => (s.charCodeAt(0) << 8) | s.charCodeAt(1);
  const IN = cmd('IN'), OK = cmd('OK'), NO = cmd('NO'), BY = cmd('BY'), CK = cmd('CK');

  // ---- packets ------------------------------------------------------------------------------

  function control(command, token, ssrc, name) {
    const nm = command === BY ? [] : Array.from(name || '', (c) => c.charCodeAt(0) & 0x7f).concat([0]);
    const b = new Uint8Array(16 + nm.length);
    const v = new DataView(b.buffer);
    v.setUint16(0, 0xffff); v.setUint16(2, command); v.setUint32(4, VERSION);
    v.setUint32(8, token >>> 0); v.setUint32(12, ssrc >>> 0);
    b.set(nm, 16);
    return b;
  }
  function clock(ssrc, count, t) {
    const b = new Uint8Array(36);
    const v = new DataView(b.buffer);
    v.setUint16(0, 0xffff); v.setUint16(2, CK); v.setUint32(4, ssrc >>> 0); b[8] = count;
    for (let i = 0; i < 3; i++) setU64(v, 12 + 8 * i, t[i] || 0);
    return b;
  }
  const setU64 = (v, o, x) => { v.setUint32(o, Math.floor(x / 4294967296)); v.setUint32(o + 4, x >>> 0); };
  const getU64 = (v, o) => v.getUint32(o) * 4294967296 + v.getUint32(o + 4);

  function parseControl(b) {
    if (b.length < 4 || b[0] !== 0xff || b[1] !== 0xff) return null;
    const v = new DataView(b.buffer, b.byteOffset, b.length);
    const command = v.getUint16(2);
    if (command === CK) {
      if (b.length < 36) return null;
      return { command, ssrc: v.getUint32(4), count: b[8], t: [getU64(v, 12), getU64(v, 20), getU64(v, 28)] };
    }
    if (b.length < 16) return { command };
    let name = '';
    for (let i = 16; i < b.length && b[i]; i++) name += String.fromCharCode(b[i]);
    return { command, version: v.getUint32(4), token: v.getUint32(8), ssrc: v.getUint32(12), name };
  }

  /** One RTP-MIDI packet holding a list of MIDI messages (arrays of bytes), deltas 0. */
  function rtp(seq, time, ssrc, messages) {
    const list = [];
    let status = 0;
    messages.forEach((m, i) => {
      if (i) list.push(0); // delta time 0
      let from = 0;
      // running status for channel messages only
      if (m[0] < 0xf0 && m[0] === status) from = 1;
      status = m[0] < 0xf0 ? m[0] : m[0] < 0xf8 ? 0 : status;
      for (let k = from; k < m.length; k++) list.push(m[k]);
    });
    const long = list.length > 15;
    const head = long ? [0x80 | ((list.length >> 8) & 0x0f), list.length & 0xff] : [list.length];
    const b = new Uint8Array(12 + head.length + list.length);
    const v = new DataView(b.buffer);
    b[0] = 0x80; b[1] = 0x61; v.setUint16(2, seq & 0xffff); v.setUint32(4, time >>> 0); v.setUint32(8, ssrc >>> 0);
    b.set(head, 12);
    b.set(list, 12 + head.length);
    return b;
  }

  const DATA_LEN = (s) => { // data bytes after a status byte; -1 = until the next status byte
    if (s < 0xf0) return [2, 2, 2, 2, 1, 1, 2][(s >> 4) - 8];
    return { 0xf1: 1, 0xf2: 2, 0xf3: 1 }[s] || 0;
  };

  /** Reads the MIDI command list of one RTP-MIDI packet. Calls onMessage(bytes) for every
   *  complete message; System Exclusive segments collect in state.sysex until the end. */
  function parseRtp(b, state, onMessage) {
    if (b.length < 13 || (b[0] & 0xc0) !== 0x80 || (b[1] & 0x7f) !== 0x61) return;
    let p = 12 + ((b[0] & 0x0f) * 4); // CSRC list, never seen
    if (b[0] & 0x10) return; // RTP header extension: not ours
    const flags = b[p];
    let len = flags & 0x0f;
    p++;
    if (flags & 0x80) { len = (len << 8) | b[p]; p++; }
    const end = Math.min(b.length, p + len);
    let first = !(flags & 0x20); // Z = 0: no delta time before the first command
    let status = 0;
    while (p < end) {
      if (!first) { // delta time, up to 4 bytes
        for (let k = 0; k < 4 && p < end; k++) { const d = b[p++]; if (!(d & 0x80)) break; }
        if (p >= end) break;
      }
      first = false;
      let s = b[p];
      if (s === 0xf0 || (s === 0xf7 && state.sysex)) {
        // System Exclusive, or a later segment of one: up to F7 (end) or F0 (more follows)
        const start = p + 1;
        let q = start;
        while (q < end && b[q] !== 0xf7 && b[q] !== 0xf0) q++;
        const body = Array.from(b.subarray(start, q));
        if (s === 0xf0) state.sysex = [0xf0];
        state.sysex.push(...body);
        if (q < end && b[q] === 0xf7) { state.sysex.push(0xf7); onMessage(state.sysex); state.sysex = null; }
        else if (q >= end) state.sysex = null; // cut off: drop it
        p = q + 1;
        status = 0;
        continue;
      }
      if (s >= 0xf8) { onMessage([s]); p++; continue; }
      if (s === 0xf7 || s === 0xf4 || s === 0xf5) { p++; status = 0; continue; } // a stray end, undefined
      if (s & 0x80) { status = s < 0xf0 ? s : 0; p++; } else s = status;
      if (!s) { p++; continue; } // a data byte without status: skip it
      const n = DATA_LEN(s);
      const m = [s];
      for (let k = 0; k < n && p < end; k++) m.push(b[p++]);
      if (m.length === n + 1) onMessage(m);
    }
  }

  /** Our JSON in a System Exclusive message, and back. */
  function toSysex(obj) {
    const s = (typeof obj === 'string' ? obj : JSON.stringify(obj)).replace(/[^\x20-\x7e]/g,
      (c) => '\\u' + ('000' + c.charCodeAt(0).toString(16)).slice(-4));
    const out = HEADER.slice();
    for (let i = 0; i < s.length; i++) out.push(s.charCodeAt(i));
    out.push(0xf7);
    return out;
  }
  function fromSysex(m) {
    if (m.length < 6 || m[0] !== 0xf0 || m[1] !== 0x7d || m[2] !== 0x4e || m[3] !== 0x4a) return null;
    let s = '';
    for (let i = 4; i < m.length - 1; i++) s += String.fromCharCode(m[i]);
    return s;
  }

  // ---- the session ---------------------------------------------------------------------------

  class Session {
    // opts: {udp(localPort) -> Promise<socket|null>, name, now(), random(); for the tests:
    //   lostMs, syncEvery[], inviteMs}
    constructor(opts) {
      this.udp = opts.udp;
      this.name = opts.name || 'NULL Remote';
      this.now = opts.now || (() => Date.now());
      this.random = opts.random || Math.random;
      this.lostMs = opts.lostMs || LOST_MS;
      this.syncEvery = opts.syncEvery || CK_EVERY;
      this.inviteMs = opts.inviteMs || INVITE_MS;
      this.ssrc = (this.random() * 4294967296) >>> 0;
      this.token = (this.random() * 4294967296) >>> 0;
      this.seq = (this.random() * 65536) | 0;
      this.t0 = this.now();
      this.state = {};
      this.listeners = new Set();
      this.closeListeners = new Set();
      this.up = false;
      this.peer = null; // {host, port, ssrc, name}
    }
    time() { return Math.round((this.now() - this.t0) * 10); } // 100 µs units

    onMidi(cb) { this.listeners.add(cb); return () => this.listeners.delete(cb); }
    onClose(cb) { this.closeListeners.add(cb); return () => this.closeListeners.delete(cb); }

    async _sockets() {
      for (let i = 0; i < 8; i++) {
        const p = 2 * (20000 + Math.floor(this.random() * 12000)); // even, 40000..63998
        const c = await this.udp(p);
        if (!c) continue;
        const d = await this.udp(p + 1);
        if (d) return [c, d];
        c.close();
      }
      const c = await this.udp(0); // any two ports, then
      const d = await this.udp(0);
      if (!c || !d) throw new Error('Cannot open a network socket on this device.');
      return [c, d];
    }

    /** Invites the computer's session at host:port (control) and port + 1 (data). */
    async connect(host, port = PORT) {
      this.host = host;
      this.port = port;
      [this.ctl, this.data] = await this._sockets();
      this.ctl.onPacket((b) => this._control(b, 'ctl'));
      this.data.onPacket((b) => {
        if (b[0] === 0xff && b[1] === 0xff) this._control(b, 'data');
        else parseRtp(b, this.state, (m) => { for (const cb of this.listeners) cb(m); });
      });
      const invite = (sock, p, which) => new Promise((resolve, reject) => {
        let tries = 0;
        const ask = () => {
          if (this.closed) { reject(new Error('closed')); return; }
          if (tries++ >= INVITE_TRIES) { this.waiting = null; reject(new Error('No answer from ' + host + ':' + p + '. Is a network MIDI session open on the computer?')); return; }
          sock.send(host, p, control(IN, this.token, this.ssrc, this.name));
          this.inviteTimer = setTimeout(ask, this.inviteMs);
        };
        this.waiting = { which, ok: (m) => { clearTimeout(this.inviteTimer); this.waiting = null; resolve(m); },
          no: () => { clearTimeout(this.inviteTimer); this.waiting = null; reject(new Error('The computer refused the session (' + host + ').')); } };
        ask();
      });
      try {
        const ok = await invite(this.ctl, port, 'ctl');
        this.peer = { host, port, ssrc: ok.ssrc, name: ok.name };
        await invite(this.data, port + 1, 'data');
      } catch (e) { this._shut(); throw e; }
      this.up = true;
      this.heard = this.now();
      this.syncs = 0;
      this._sync();
      return this.peer;
    }

    _sync() {
      if (!this.up) return;
      if (this.now() - this.heard > this.lostMs) { this._gone('the computer stopped answering'); return; }
      this.data.send(this.host, this.port + 1, clock(this.ssrc, 0, [this.time(), 0, 0]));
      this.syncTimer = setTimeout(() => this._sync(), this.syncEvery[Math.min(this.syncs++, this.syncEvery.length - 1)]);
    }

    _control(b, which) {
      const m = parseControl(b);
      if (!m) return;
      if (m.command === OK || m.command === NO) {
        if (this.waiting && this.waiting.which === which && m.token === this.token) (m.command === OK ? this.waiting.ok : this.waiting.no)(m);
        return;
      }
      if (m.command === BY) { if (this.up) this._gone('the computer ended the session'); return; }
      if (m.command === CK && which === 'data' && this.up) {
        this.heard = this.now();
        if (m.count === 1) this.data.send(this.host, this.port + 1, clock(this.ssrc, 2, [m.t[0], m.t[1], this.time()]));
        else if (m.count === 0) this.data.send(this.host, this.port + 1, clock(this.ssrc, 1, [m.t[0], this.time(), 0]));
      }
      if (m.command === IN && this.up) { // the computer invites back after a restart: say no, we invite
        (which === 'ctl' ? this.ctl : this.data).send(this.host, which === 'ctl' ? this.port : this.port + 1, control(NO, m.token, this.ssrc, this.name));
      }
    }

    /** Sends MIDI messages (arrays of bytes). A long System Exclusive message is split. */
    send(messages) {
      if (!this.up) return;
      const out = [];
      for (const m of messages) {
        if (m[0] !== 0xf0 || m.length <= MAX_SYSEX) { out.push(m); continue; }
        // segments: F0 .. F0, F7 .. F0, ..., F7 .. F7, each in its own packet
        this._flush(out.splice(0));
        const body = m.slice(1, m.length - 1);
        for (let i = 0; i < body.length; i += MAX_SYSEX - 2) {
          const last = i + MAX_SYSEX - 2 >= body.length;
          const seg = [i ? 0xf7 : 0xf0].concat(body.slice(i, i + MAX_SYSEX - 2), [last ? 0xf7 : 0xf0]);
          this._flush([seg]);
        }
      }
      this._flush(out);
    }
    _flush(msgs) {
      // keep packets under about 1200 bytes
      let batch = [];
      let size = 0;
      const go = () => { if (batch.length) this.data.send(this.host, this.port + 1, rtp(this.seq++, this.time(), this.ssrc, batch)); batch = []; size = 0; };
      for (const m of msgs) {
        if (size + m.length > 1200) go();
        batch.push(m);
        size += m.length + 1;
      }
      go();
    }

    _gone(why) {
      if (!this.up) return;
      this._shut();
      for (const cb of this.closeListeners) cb(why);
    }
    _shut() {
      this.up = false;
      clearTimeout(this.syncTimer);
      clearTimeout(this.inviteTimer);
      if (this.ctl) this.ctl.close();
      if (this.data) this.data.close();
      this.ctl = this.data = null;
    }
    close() {
      this.closed = true;
      if (this.up && this.ctl) {
        // say BY, and close the sockets once it has gone out
        this.ctl.send(this.host, this.port, control(BY, this.token, this.ssrc));
        const [c, d] = [this.ctl, this.data];
        this.ctl = this.data = null;
        setTimeout(() => { c.close(); d.close(); }, 200);
      }
      this._shut();
    }
  }

  /** A connection carrying our JSON lines over network MIDI, shaped like bitwig.js's dial:
   *  dial(host, port, opts)({onOpen, onData(bytes), onClose}) -> {send(bytes), close()}.
   *  What the Bitwig session sends (a 4-byte length, then the JSON) goes out as one System
   *  Exclusive message; what the DAW sends comes back as JSON lines. */
  function dial(host, port, opts) {
    return function (cb) {
      const s = new Session(opts);
      let closed = false;
      const enc = new TextEncoder();
      s.onMidi((m) => { const t = fromSysex(m); if (t != null) cb.onData(enc.encode(t + '\n')); });
      s.onClose((why) => { if (!closed) { closed = true; cb.onClose(why); } });
      s.connect(host, port).then(() => { if (!closed) cb.onOpen(); },
        (e) => { if (!closed) { closed = true; cb.onClose(e.message); } });
      return {
        session: s,
        send(bytes) {
          let from = 0;
          if (bytes.length >= 4 && bytes[0] === 0) from = 4; // the Bitwig framing's length
          // the bytes are UTF-8: decode them, toSysex escapes what is not ASCII
          s.send([toSysex(new TextDecoder().decode(bytes.subarray(from)))]);
        },
        close() { closed = true; s.close(); },
      };
    };
  }

  /** UDP through the app (window.NullShell, android/web/core/shell.js). */
  function shellUdp(localPort) {
    const shell = root.NullShell;
    if (!shell || !shell.native || !shell.udpOpen) throw new Error('This tab needs the Remote for DAWs app (no network access in a browser).');
    const b64 = (bytes) => { let s = ''; for (let k = 0; k < bytes.length; k++) s += String.fromCharCode(bytes[k]); return btoa(s); };
    const unb64 = (str) => { const s = atob(str); const b = new Uint8Array(s.length); for (let k = 0; k < s.length; k++) b[k] = s.charCodeAt(k); return b; };
    return Promise.resolve(shell.udpOpen(localPort)).then((id) => {
      if (id == null || id < 0) return null;
      const listeners = new Set();
      const deliver = (m) => { if (m && m.id === id) { const b = unb64(m.data); for (const cb of listeners) cb(b, m.host, m.port); } };
      shell.on('udp', deliver);
      return {
        send: (host, port, bytes) => shell.udpSend(id, host, port, b64(bytes)),
        onPacket(cb) { listeners.add(cb); return () => listeners.delete(cb); },
        close() { listeners.clear(); shell.off('udp', deliver); shell.udpClose(id); },
      };
    });
  }

  return { PORT, Session, dial, shellUdp, toSysex, fromSysex, rtp, parseRtp, control, parseControl, clock };
});
