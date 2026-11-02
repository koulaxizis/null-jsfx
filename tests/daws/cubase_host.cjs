// A software Cubase for the tests of the Cubase tab: it runs the real MIDI Remote script
// (remote/cubase/nulljsfx/remotefordaws/nulljsfx_remotefordaws.js) in a Node VM against a
// stand-in for the parts of Cubase's MIDI Remote API it uses (midiremote_api_v1, as Steinberg's
// documentation describes them), and connects the script's MIDI port to a network MIDI session
// (rtpmidi_peer.cjs), as rtpMIDI or macOS's network MIDI driver would. It checks that the script
// uses the API as documented; whether Cubase behaves the same is Bible C19.
//   Host values: a surface variable bound to one gets its value and display text when the host
//   value changes and when the binding is made; setProcessValue on it sets the host value, and
//   every variable bound to that host value hears it (its own too). A bank zone of n channels
//   moves by n with mNextBank / mPrevBank and tells every channel again. The selected track's
//   insert viewers tell their plugin, bypass and parameters again when the selection changes.
// NULL JSFX • https://nulljsfx.tech • MIT License
'use strict';
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const Peer = require('./rtpmidi_peer.cjs');

const SCRIPT = path.join(__dirname, '../../remote/cubase/nulljsfx/remotefordaws/nulljsfx_remotefordaws.js');
const UNITY = 0.7; // this host's volume value for 0 dB
const volText = (v) => (v <= 0 ? '-∞' : (40 * Math.log10(v / UNITY)).toFixed(2));

// a NULL plugin as Cubase lists its parameters (VST3: one per slider, normalized 0..1)
function nullPlugin(id) {
  require(path.join(__dirname, '../../web/fx', id + '.js'));
  const FX = globalThis.NULLFX[id];
  return { name: 'NULL ' + FX.name, bypass: 0, params: FX.sliders.map((s) => ({
    name: s.name.replace(/\s*\([^)]*\)\s*$/, ''), v: (s.def - s.min) / (s.max - s.min || 1),
  })) };
}

function defaultProject(n = 3) {
  const tracks = [
    { name: 'Vocal', color: [0.8, 0.3, 0.2], vol: UNITY, pan: 0.5, mute: 0, solo: 0, arm: 0, sel: 1, meter: 0.4, sends: [0.25, 0, 0, 0],
      inserts: [nullPlugin('delay'), null, { name: 'Frequency 2', bypass: 0, params: [{ name: 'Gain 1', v: 0.5 }] }] },
    { name: 'Μπάσο', color: [0.2, 0.5, 0.8], vol: 0.5, pan: 0.3, mute: 1, solo: 0, arm: 1, sel: 0, meter: 0.1, sends: [0, 0, 0, 0], inserts: [] },
  ];
  for (let i = tracks.length; i < n; i++) tracks.push({ name: 'Track ' + (i + 1), color: [0.5, 0.5, 0.5], vol: UNITY, pan: 0.5, mute: 0, solo: 0, arm: 0, sel: 0, meter: 0, sends: [0, 0, 0, 0], inserts: [] });
  return { tracks, transport: { play: 0, stop: 0, rec: 0, cycle: 0, position: 1000 } };
}

async function start(opts = {}) {
  const project = opts.project || defaultProject(opts.tracks);
  const peer = await Peer.start();
  const activeDevice = { id: 'device' };
  const commands = [];

  // ---- host values -------------------------------------------------------------------------
  // a host value: get() -> {v, text}, set(v); bound surface variables hear it through tell()
  const hostValues = [];
  function hostValue(get, set, title) {
    const h = { get, set, title: title || (() => ''), bound: new Set() };
    hostValues.push(h);
    return h;
  }
  function tell(h) {
    const { v, text } = h.get();
    const [obj, name] = h.title();
    for (const sv of h.bound) {
      if (sv.mOnTitleChange) sv.mOnTitleChange(activeDevice, obj || '', name || '');
      if (sv.mOnProcessValueChange) sv.mOnProcessValueChange(activeDevice, v, 0);
      if (sv.mOnDisplayValueChange) sv.mOnDisplayValueChange(activeDevice, text == null ? String(v) : text, '');
    }
  }
  const tellAll = () => hostValues.forEach(tell);

  // ---- the API ------------------------------------------------------------------------------
  let driver;
  const zones = [];
  const viewers = [];
  const trackAt = (zone, k) => project.tracks[zone.offset + k];
  function makeZone() {
    const zone = { offset: 0, channels: [] };
    zones.push(zone);
    const chain = () => zone;
    Object.assign(zone, {
      excludeInputChannels: chain, excludeOutputChannels: chain, includeAudioChannels: chain, setFollowVisibility: chain,
      mAction: { mNextBank: { act: () => move(zone, 1) }, mPrevBank: { act: () => move(zone, -1) } },
      makeMixerBankChannel() {
        const k = zone.channels.length;
        const t = () => trackAt(zone, k);
        const tv = (field, text) => hostValue(() => {
          const x = t();
          return x ? { v: x[field], text: text ? text(x[field]) : String(x[field]) } : { v: 0, text: '' };
        }, (v) => { const x = t(); if (x) { x[field] = v; if (field === 'sel' && v) select(x); } });
        const ch = {
          mValue: { mVolume: tv('vol', volText), mPan: tv('pan', (v) => (v === 0.5 ? 'C' : v < 0.5 ? 'L' + Math.round((0.5 - v) * 200) : 'R' + Math.round((v - 0.5) * 200))),
            mMute: tv('mute'), mSolo: tv('solo'), mRecordEnable: tv('arm'), mSelected: tv('sel'), mVUMeter: tv('meter') },
          mSends: { getByIndex: (s) => ({ mLevel: hostValue(() => { const x = t(); return x ? { v: x.sends[s], text: volText(x.sends[s]) } : { v: 0, text: '' }; },
            (v) => { const x = t(); if (x) x.sends[s] = v; }), mOn: hostValue(() => ({ v: 1 }), () => {}) }), getSize: () => 4 },
        };
        ch.tellTitle = () => {
          const x = t();
          if (ch.mOnTitleChange) ch.mOnTitleChange(activeDevice, {}, x ? x.name : '');
          if (ch.mOnColorChange && x) ch.mOnColorChange(activeDevice, {}, x.color[0], x.color[1], x.color[2], 1, true);
        };
        zone.channels.push(ch);
        return ch;
      },
    });
    return zone;
  }
  function move(zone, dir) {
    const n = zone.channels.length;
    const to = zone.offset + dir * n;
    if (to < 0 || to >= project.tracks.length) return;
    zone.offset = to;
    for (const ch of zone.channels) ch.tellTitle();
    tellAll();
  }
  function select(x) {
    for (const t of project.tracks) t.sel = t === x ? 1 : 0;
    tellAll();
    for (const v of viewers) v.tell();
  }
  const selectedTrack = () => project.tracks.find((t) => t.sel);
  function makeViewer() {
    const viewer = { slot: 0, params: [] };
    viewers.push(viewer);
    const ins = () => { const t = selectedTrack(); return (t && t.inserts[viewer.slot]) || null; };
    Object.assign(viewer, {
      accessSlotAtIndex(i) { viewer.slot = i; return viewer; },
      excludeEmptySlots() { return viewer; },
      mBypass: hostValue(() => ({ v: ins() ? ins().bypass : 0 }), (v) => { if (ins()) ins().bypass = v; }),
      mOn: hostValue(() => ({ v: 1 }), () => {}),
      mParameterBankZone: {
        makeParameterValue() {
          const p = viewer.params.length;
          const par = () => (ins() && ins().params[p]) || null;
          const h = hostValue(() => (par() ? { v: par().v, text: (par().v * 100).toFixed(1) + ' %' } : { v: 0, text: '' }),
            (v) => { if (par()) par().v = v; }, () => [ins() ? ins().name : '', par() ? par().name : '']);
          viewer.params.push(h);
          return h;
        },
      },
      tell() {
        if (viewer.mOnChangePluginIdentity) viewer.mOnChangePluginIdentity(activeDevice, {}, ins() ? ins().name : '', 'NULL JSFX', '3.0', 'VST3');
      },
    });
    return viewer;
  }
  const tp = project.transport;
  const tvals = {};
  const transportValue = (field) => (tvals[field] = hostValue(() => ({ v: tp[field] }), (v) => {
    tp[field] = v;
    if (field === 'stop' && v) { tp.play = 0; setTimeout(() => tell(tvals.play), 0); } // Stop stops play
  }));

  const api = {
    makeDeviceDriver(vendor, device, by) {
      driver = { vendor, device, by, detection: [] };
      const surfaceValue = (name) => ({
        name, binding: null,
        setProcessValue(dev, v) {
          if (dev !== activeDevice) throw new Error('setProcessValue without the active device');
          const b = this.binding;
          if (!b) return;
          if (b.kind === 'value') { b.host.set(v); tell(b.host); } else if (v > 0) b.fire();
        },
      });
      const page = {
        mHostAccess: {
          mMixConsole: { makeMixerBankZone: () => makeZone() },
          mTrackSelection: { mMixerChannel: { mInsertAndStripEffects: { makeInsertEffectViewer: () => makeViewer() } } },
          mTransport: { mValue: { mStart: transportValue('play'), mStop: transportValue('stop'), mRecord: transportValue('rec'), mCycleActive: transportValue('cycle') } },
        },
        makeValueBinding(sv, h) {
          if (!h || !h.bound) throw new Error('makeValueBinding: not a host value');
          sv.binding = { kind: 'value', host: h };
          h.bound.add(sv);
          return { setTypeToggle() { return this; } };
        },
        makeActionBinding(sv, action) { if (!action || !action.act) throw new Error('not an action'); sv.binding = { kind: 'action', fire: action.act }; return {}; },
        makeCommandBinding(sv, category, name) { sv.binding = { kind: 'command', fire: () => { commands.push(category + '/' + name); if (name === 'To Project Start') tp.position = 0; } }; return {}; },
      };
      Object.assign(driver, {
        mPorts: { makeMidiInput: () => (driver.midiIn = {}), makeMidiOutput: () => (driver.midiOut = { sendMidi(dev, bytes) { out(dev, bytes); } }) },
        makeDetectionUnit: () => ({ detectPortPair(i, o) {
          const pair = { i, o, expect: [] };
          driver.detection.push(pair);
          const add = (k) => (s) => { pair.expect.push([k, s]); return pairApi; };
          const pairApi = { expectInputNameContains: add('in'), expectOutputNameContains: add('out'),
            expectInputNameEquals: add('in='), expectOutputNameEquals: add('out=') };
          return pairApi;
        } }),
        mSurface: { makeCustomValueVariable: (name) => surfaceValue(name) },
        mMapping: { makePage: () => page },
      });
      return driver;
    },
  };
  const sent = []; // every message the script sent
  function out(dev, bytes) {
    if (dev !== activeDevice) throw new Error('sendMidi without the active device');
    const b = Array.from(bytes);
    sent.push(b);
    peer.send([b]);
  }

  const sandbox = { require: (n) => { if (n !== 'midiremote_api_v1') throw new Error('require ' + n); return api; }, JSON, Math, Date, String, Number, Array, Object, console };
  vm.createContext(sandbox);
  vm.runInContext(fs.readFileSync(SCRIPT, 'utf8'), sandbox, { filename: SCRIPT });

  // Cubase starts the script: the port is found, the mapping tells every value
  driver.mOnActivate(activeDevice);
  for (const z of zones) for (const ch of z.channels) ch.tellTitle();
  tellAll();
  for (const v of viewers) v.tell();
  peer.onMidi((m) => { if (m[0] === 0xf0) driver.midiIn.mOnSysex(activeDevice, m); });
  const idle = setInterval(() => driver.mOnIdle(activeDevice), 15);

  return {
    peer, port: peer.port, project, driver, sent, commands, zones, viewers,
    // a change made in Cubase itself
    change(fn) { fn(project); for (const z of zones) for (const ch of z.channels) ch.tellTitle(); tellAll(); for (const v of viewers) v.tell(); },
    close() { clearInterval(idle); peer.close(); },
  };
}

module.exports = { start, defaultProject, nullPlugin, UNITY, volText };
