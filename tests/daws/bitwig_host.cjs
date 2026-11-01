// A software Bitwig Studio for the tests of the Bitwig connector: it runs the real controller
// script (remote/bitwig/NullJSFX/NullJSFX.control.js) in a Node VM against a stand-in for the
// parts of Bitwig's controller API the script uses (typed-bitwig-api 25 for the signatures),
// and serves its RemoteSocket on real TCP. Inbound messages carry Bitwig's 4-byte big-endian
// length; outbound sends get the same header only with { headerOut: true }, because the API
// documents the inbound framing only, and the phone has to read both.
// setDirectParameterValueNormalized follows the documented [0, resolution - 1] form.
// NULL JSFX • https://nulljsfx.tech • MIT License
'use strict';
const fs = require('fs');
const net = require('net');
const path = require('path');
const vm = require('vm');

const SCRIPT = path.join(__dirname, '../../remote/bitwig/NullJSFX/NullJSFX.control.js');
const UNITY = 0.7937; // Bitwig's volume position for 0 dB

// a NULL plugin as Bitwig lists its direct parameters: one per slider (native name, no unit),
// normalized 0..1, plus a parameter of the host's own
function nullPlugin(id) {
  require(path.join(__dirname, '../../web/fx', id + '.js'));
  const FX = globalThis.NULLFX[id];
  const params = FX.sliders.map((s, k) => ({
    id: 'CONTENTS/PID' + (k + 1).toString(16),
    name: s.name.replace(/\s*\([^)]*\)\s*$/, ''),
    value: (s.def - s.min) / (s.max - s.min || 1),
    lo: s.min, hi: s.max,
  }));
  params.push({ id: 'CONTENTS/PIDbypass', name: 'Bypass', value: 0, lo: 0, hi: 1 });
  return { name: 'NULL ' + FX.name, enabled: true, plugin: true, params };
}

function defaultDocument() {
  return {
    tracks: [
      { name: 'Vocal', type: 'Audio', vol: UNITY, pan: 0.5, mute: false, solo: false, arm: false, color: [0.8, 0.3, 0.2], sends: [0.25],
        devices: [nullPlugin('tilt'), nullPlugin('delay'),
          { name: 'EQ+', enabled: true, plugin: false, params: [
            { id: 'CONTENTS/ROOT_GENERIC_MODULE/GAIN1', name: 'Gain 1', value: 0.5, lo: -24, hi: 24 },
            { id: 'CONTENTS/ROOT_GENERIC_MODULE/FREQ1', name: 'Freq 1', value: 0.25, lo: 20, hi: 20000 },
          ] }] },
      { name: 'Synth', type: 'Instrument', vol: 0.6, pan: 0.3, mute: true, solo: false, arm: true, color: [0.2, 0.5, 0.8], devices: [] },
      { name: 'FX 1', type: 'Effect', vol: UNITY, pan: 0.5, mute: false, solo: false, arm: false, color: [0.5, 0.5, 0.5], devices: [] },
      { name: 'Master', type: 'Master', vol: UNITY, pan: 0.5, mute: false, solo: false, arm: false, color: [0.3, 0.3, 0.3], devices: [] },
    ],
  };
}

// sends: track.sends[k] goes to the k-th effect track; effect tracks and the master have none
const SEND_DEFAULT = 0;
const effects = (doc) => doc.tracks.filter((t) => t.type === 'Effect');
const hasSends = (t) => !!t && t.type !== 'Effect' && t.type !== 'Master';
function sendText(v) {
  if (v <= 0) return '-inf dB';
  return (20 * Math.log10(v) * 2).toFixed(1) + ' dB';
}

function volText(v) {
  if (v <= 0) return '-inf dB';
  const db = 40 * Math.log10(v / UNITY);
  return (db > 0 ? '+' : '') + db.toFixed(1) + ' dB';
}
function panText(v) {
  const p = Math.round((v - 0.5) * 200);
  return p === 0 ? 'C' : (p < 0 ? 'L' : 'R') + Math.abs(p) + '%';
}
function paramText(p) {
  const x = p.lo + p.value * (p.hi - p.lo);
  return (Math.abs(x) >= 100 ? x.toFixed(0) : x.toFixed(2));
}

// --- the API stand-in ----------------------------------------------------------------------------

class Value {
  constructor(get, onSet, host) {
    this.get = get;
    this.onSet = onSet;
    this.host = host;
    this.obs = [];
    this.last = undefined;
    host.values.push(this);
  }
  markInterested() {}
  addValueObserver(...a) { this.obs.push(a[a.length - 1]); this.changed(true); }
  set(v) { if (this.onSet) this.onSet(v); this.host.update(); }
  toggle() { this.set(!this.get()); }
  getRaw() { return this.get(); }
  changed(force) {
    const v = this.get();
    const key = JSON.stringify(v);
    if (!force && key === this.last) return;
    this.last = key;
    for (const cb of this.obs) {
      if (Array.isArray(v)) cb(...v);
      else cb(v);
    }
  }
}

class Host {
  constructor(doc, opts) {
    this.doc = doc;
    this.opts = opts;
    this.values = [];
    this.flushQueued = false;
    this.timers = new Set();
    this.log = [];
    this.scroll = 0;
    this.cursor = { track: -1, device: -1 }; // document indices
    this.observed = null;
    this.meters = [];
    this.position = 0;
    this.transport = { play: false, rec: false, loop: false };
  }
  track(slot) { return this.doc.tracks[this.scroll + slot]; }
  device() {
    const t = this.doc.tracks[this.cursor.track];
    return t && t.devices[this.cursor.device];
  }
  update() {
    for (const v of this.values) v.changed(false);
    this.directChanged();
    this.requestFlush();
  }
  requestFlush() {
    if (this.flushQueued) return;
    this.flushQueued = true;
    setImmediate(() => { this.flushQueued = false; if (this.script) this.script.flush(); });
  }
  scheduleTask(fn, ms) {
    const h = setTimeout(() => { this.timers.delete(h); fn(); }, ms);
    this.timers.add(h);
  }

  // the direct parameters of the cursor device
  directChanged() {
    const d = this.device();
    const ids = d ? d.params.map((p) => p.id) : [];
    const key = (this.cursor.track + '/' + this.cursor.device) + ':' + ids.join(',');
    const cd = this.cursorDevice;
    if (!cd) return;
    if (key !== cd.lastKey) {
      cd.lastKey = key;
      cd.lastVals = {};
      cd.lastDisp = {};
      for (const cb of cd.idObs) cb(ids.slice());
      if (d) for (const p of d.params) for (const cb of cd.nameObs) cb(p.id, p.name);
    }
    if (!d) return;
    for (const p of d.params) {
      if (cd.lastVals[p.id] !== p.value) {
        cd.lastVals[p.id] = p.value;
        for (const cb of cd.valObs) cb(p.id, p.value);
      }
      const s = paramText(p);
      if (this.observed && this.observed.includes(p.id) && cd.lastDisp[p.id] !== s) {
        cd.lastDisp[p.id] = s;
        for (const cb of cd.dispObs) cb(p.id, s);
      }
    }
  }
}

function api(H) {
  const V = (get, set) => new Value(get, set, H);
  const trackApi = (slot) => {
    const t = () => H.track(slot) || {};
    const field = (k) => V(() => t()[k], (v) => { if (H.track(slot)) H.track(slot)[k] = v; H.log.push(`${k} ${slot} ${v}`); });
    const param = (k) => {
      const p = {
        value: () => field(k),
        reset: () => { if (H.track(slot)) H.track(slot)[k] = k === 'vol' ? UNITY : 0.5; H.log.push(`reset ${k} ${slot}`); H.update(); },
      };
      const value = field(k);
      const text = V(() => (k === 'vol' ? volText : panText)(t()[k] ?? 0));
      value.displayedValue = () => text;
      p.value = () => value;
      return p;
    };
    const vol = param('vol');
    const pan = param('pan');
    const mute = field('mute');
    const solo = field('solo');
    const arm = field('arm');
    const devApis = [];
    const sendApis = [];
    const sendBank = {
      getItemAt(k) {
        if (sendApis[k]) return sendApis[k];
        const on = () => hasSends(H.track(slot)) && k < effects(H.doc).length;
        const get = () => (on() ? (t().sends && t().sends[k]) || 0 : 0);
        const value = V(get, (v) => {
          if (!on()) return;
          if (!t().sends) t().sends = [];
          t().sends[k] = v;
          H.log.push(`send ${slot} ${k} ${v}`);
        });
        const text = V(() => sendText(get()));
        value.displayedValue = () => text;
        sendApis[k] = {
          exists: () => V(on),
          value: () => value,
          reset() { if (on()) { if (!t().sends) t().sends = []; t().sends[k] = SEND_DEFAULT; } H.log.push(`reset send ${slot} ${k}`); H.update(); },
        };
        return sendApis[k];
      },
    };
    return {
      sendBank: () => sendBank,
      exists: () => V(() => !!H.track(slot)),
      name: () => V(() => t().name || ''),
      trackType: () => V(() => t().type || ''),
      volume: () => vol,
      pan: () => pan,
      mute: () => mute,
      solo: () => solo,
      arm: () => arm,
      color: () => V(() => t().color || [0, 0, 0]),
      addVuMeterObserver(range, ch, peak, cb) {
        H.meters.push({ slot, range, cb });
      },
      createDeviceBank(n) {
        return {
          getItemAt(d) {
            if (devApis[d]) return devApis[d];
            const dev = () => (H.track(slot) || { devices: [] }).devices[d];
            devApis[d] = {
              slot, d,
              exists: () => V(() => !!dev()),
              name: () => V(() => (dev() ? dev().name : '')),
              isEnabled: () => V(() => (dev() ? dev().enabled : false), (v) => { if (dev()) dev().enabled = v; H.log.push(`enabled ${slot}/${d} ${v}`); }),
              isPlugin: () => V(() => (dev() ? dev().plugin : false)),
            };
            return devApis[d];
          },
        };
      },
      _slot: slot,
    };
  };

  const trackApis = [];
  const cursorDevice = {
    idObs: [], nameObs: [], valObs: [], dispObs: [], lastKey: null, lastVals: {}, lastDisp: {},
    pinned: false,
    isPinned: () => V(() => cursorDevice.pinned, (v) => { cursorDevice.pinned = v; }),
    exists: () => V(() => !!H.device()),
    name: () => V(() => (H.device() ? H.device().name : '')),
    isPlugin: () => V(() => (H.device() ? H.device().plugin : false)),
    position: () => V(() => (H.device() ? H.cursor.device : -1)),
    selectDevice(devApi) {
      // a device of the bank of track slot s: the cursor goes there
      H.cursor = { track: H.scroll + devApi.slot, device: devApi.d };
      H.log.push(`select ${devApi.slot}/${devApi.d}`);
      H.update();
    },
    addDirectParameterIdObserver(cb) { cursorDevice.idObs.push(cb); },
    addDirectParameterNameObserver(max, cb) { cursorDevice.nameObs.push(cb); },
    addDirectParameterNormalizedValueObserver(cb) { cursorDevice.valObs.push(cb); },
    addDirectParameterValueDisplayObserver(max, cb) {
      cursorDevice.dispObs.push(cb);
      return { setObservedParameterIds(ids) { H.observed = ids ? Array.from(ids) : null; cursorDevice.lastDisp = {}; H.update(); } };
    },
    setDirectParameterValueNormalized(id, value, resolution) {
      const d = H.device();
      const p = d && d.params.find((q) => q.id === id);
      H.log.push(`param ${id} ${value}/${resolution}`);
      if (p) p.value = Math.max(0, Math.min(1, value / (resolution - 1)));
      H.update();
    },
  };
  H.cursorDevice = cursorDevice;

  return {
    loadAPI(n) { H.apiVersion = n; },
    setShouldFailOnDeprecatedUse() {},
    defineController(...a) { H.definition = a; },
    defineMidiPorts() {},
    getPreferences: () => ({
      getNumberSetting: (label, cat, min, max, step, unit, init) => V(() => H.opts.port ?? init),
    }),
    createTransport: () => {
      const tr = H.transport;
      const pos = V(() => H.position, (v) => { H.position = v; H.log.push('position ' + v); });
      pos.getFormatted = () => {
        const beats = H.position;
        return `${String(Math.floor(beats / 4) + 1).padStart(3, '0')}:${Math.floor(beats % 4) + 1}:${Math.floor((beats % 1) * 4) + 1}`;
      };
      const play = V(() => tr.play);
      const rec = V(() => tr.rec, (v) => { tr.rec = v; H.log.push('rec ' + v); });
      const loop = V(() => tr.loop, (v) => { tr.loop = v; H.log.push('loop ' + v); });
      return {
        isPlaying: () => play,
        isArrangerRecordEnabled: () => rec,
        isArrangerLoopEnabled: () => loop,
        getPosition: () => pos,
        play() { tr.play = true; H.log.push('play'); H.update(); },
        stop() { tr.play = false; H.log.push('stop'); H.update(); },
      };
    },
    createBeatTimeFormatter: () => ({}),
    createTrackBank(n) {
      for (let k = 0; k < n; k++) trackApis[k] = trackApi(k);
      return {
        itemCount: () => V(() => H.doc.tracks.length),
        scrollPosition: () => V(() => H.scroll, (v) => { H.scroll = Math.max(0, Math.min(v, H.doc.tracks.length - 1)); }),
        getItemAt: (k) => trackApis[k],
      };
    },
    createEffectTrackBank(n) {
      return {
        getItemAt: (k) => ({
          exists: () => V(() => k < effects(H.doc).length),
          name: () => V(() => (effects(H.doc)[k] ? effects(H.doc)[k].name : '')),
        }),
      };
    },
    createCursorTrack(id, name, sends, scenes, follow) {
      H.cursorFollows = follow;
      return {
        selectChannel(t) { H.cursor.track = H.scroll + t._slot; H.log.push('channel ' + t._slot); },
        createCursorDevice: () => cursorDevice,
      };
    },
    createRemoteConnection(name, port) {
      const sock = { cb: null, port };
      H.socket = sock;
      return { setClientConnectCallback(cb) { sock.cb = cb; }, getPort: () => port };
    },
    scheduleTask: (fn, ms) => H.scheduleTask(fn, ms),
    requestFlush: () => H.requestFlush(),
    println: (s) => H.log.push('println ' + s),
  };
}

/** Starts the script and its TCP socket. Returns { port, host (the stand-in), doc, close() }. */
async function start(opts = {}) {
  const doc = opts.doc || defaultDocument();
  const H = new Host(doc, { port: 0, headerOut: false, ...opts });
  const hostApi = api(H);
  const ctx = {
    host: hostApi,
    loadAPI: hostApi.loadAPI,
    println: hostApi.println,
    Math, JSON, String, Number, Array, Object, Date,
  };
  vm.createContext(ctx);
  vm.runInContext(fs.readFileSync(SCRIPT, 'utf8'), ctx, { filename: SCRIPT });
  H.script = ctx;
  ctx.init();
  H.requestFlush(); // Bitwig calls flush() after init and whenever observers fired

  const conns = new Set();
  const server = net.createServer((s) => {
    conns.add(s);
    let disconnectCb = null;
    let receiveCb = null;
    let buf = Buffer.alloc(0);
    const conn = {
      setDisconnectCallback(cb) { disconnectCb = cb; },
      setReceiveCallback(cb) { receiveCb = cb; },
      send(arr) {
        const body = Buffer.from(Int8Array.from(arr).buffer);
        if (H.opts.headerOut) {
          const h = Buffer.alloc(4);
          h.writeUInt32BE(body.length);
          s.write(Buffer.concat([h, body]));
        } else {
          s.write(body);
        }
      },
      disconnect() { s.destroy(); },
    };
    s.on('data', (chunk) => {
      buf = Buffer.concat([buf, chunk]);
      while (buf.length >= 4) {
        const n = buf.readUInt32BE(0);
        if (buf.length < 4 + n) break;
        const body = buf.subarray(4, 4 + n);
        buf = buf.subarray(4 + n);
        H.received = (H.received || 0) + 1;
        // a Java byte[] in the script: signed values
        if (receiveCb) receiveCb(Array.from(new Int8Array(body.buffer, body.byteOffset, body.length)));
      }
    });
    s.on('close', () => { conns.delete(s); if (disconnectCb) disconnectCb(); });
    s.on('error', () => {});
    H.socket.cb(conn);
  });
  await new Promise((r) => server.listen(opts.port || 0, '127.0.0.1', r));

  // meters: a slowly moving peak on every track
  let phase = 0;
  const meterTimer = setInterval(() => {
    phase += 0.2;
    for (const m of H.meters) {
      const t = H.track(m.slot);
      if (t) m.cb(Math.round((0.5 + 0.3 * Math.sin(phase + m.slot)) * (m.range - 1)));
    }
    if (H.transport.play) { H.position += 0.25; H.update(); }
  }, 60);

  return {
    port: server.address().port,
    host: H,
    doc,
    script: ctx,
    clients: () => conns.size,
    /** Changes the document "in Bitwig" and lets the observers see it. */
    change(fn) { fn(doc); H.update(); },
    close() {
      clearInterval(meterTimer);
      for (const h of H.timers) clearTimeout(h);
      H.timers.clear();
      for (const s of conns) s.destroy();
      server.close();
      try { ctx.exit(); } catch (e) { /* closed */ }
    },
  };
}

module.exports = { start, defaultDocument, UNITY };

if (require.main === module) {
  start({ port: parseInt(process.argv[2], 10) || 8818, headerOut: process.argv.includes('--header') })
    .then((b) => console.log(`software Bitwig with the NULL JSFX script on TCP ${b.port}`));
}
