// NULL JSFX Remote for Cubase: a MIDI Remote script that lets the Android app Remote for DAWs
// (tab Cubase) run Cubase's mixer, transport and the insert effects of the selected track, with
// the NULL JSFX plugins drawn by their own interface on the phone.
// NULL JSFX • https://nulljsfx.tech • MIT License
//
// Install: copy the folder nulljsfx into Cubase's "MIDI Remote/Driver Scripts/Local" folder
// (Documents/Steinberg/Cubase/MIDI Remote/Driver Scripts/Local), open a network MIDI session
// named "NULL Remote" on the computer (macOS: Audio MIDI Setup > MIDI Network Setup; Windows:
// rtpMIDI), restart Cubase. See ../INSTALL.txt.
//
// Cubase's scripts speak MIDI only, so the phone reaches this script over network MIDI
// (RTP-MIDI): the session's MIDI port is this script's input and output. Messages are the
// protocol of our Bitwig controller script (remote/bitwig/NullJSFX/NullJSFX.control.js, version
// 1), one JSON object per System Exclusive message:
//     F0 7D 4E 4A <the JSON in ASCII, other characters as \uXXXX> F7
// What differs from Bitwig, because the MIDI Remote API gives no more:
//   - a window of 16 tracks (audio, instrument, sampler, MIDI, group and FX channels) moved
//     page by page; Cubase does not say how many tracks there are, so `tracks` carries
//     more: true while the window is full;
//   - the inserts listed and opened are those of the selected track: `list i` and `open i d`
//     select track i first; 16 slots, 48 parameters each;
//   - no clock, no device inside another, no colours of sends' targets (sends are numbered 1..4).
//
// Written against Steinberg's MIDI Remote API documentation (midiremote_api_v1), ES5 only.

var midiremote_api = require('midiremote_api_v1');

var PROTO = 1;
var VERSION = '1.0';
var SIZE = 16; // tracks in the window
var SENDS = 4;
var SLOTS = 16; // insert slots of the selected track
var PARAMS = 48; // parameters per slot
var METER_MS = 100;

var driver = midiremote_api.makeDeviceDriver('NULL JSFX', 'Remote for DAWs', 'NULL JSFX');
var midiIn = driver.mPorts.makeMidiInput();
var midiOut = driver.mPorts.makeMidiOutput();
driver.makeDetectionUnit().detectPortPair(midiIn, midiOut)
  .expectInputNameContains('NULL Remote')
  .expectOutputNameContains('NULL Remote');

var surface = driver.mSurface;
var page = driver.mMapping.makePage('Remote for DAWs');
var hostAccess = page.mHostAccess;

// ---- messages ---------------------------------------------------------------------------------

function ascii(s) {
  var out = '';
  for (var i = 0; i < s.length; i++) {
    var c = s.charCodeAt(i);
    if (c >= 0x20 && c < 0x7f) out += s.charAt(i);
    else out += '\\u' + ('000' + c.toString(16)).slice(-4);
  }
  return out;
}
function send(dev, obj) {
  if (!dev) return;
  var s = ascii(JSON.stringify(obj));
  var bytes = [0xf0, 0x7d, 0x4e, 0x4a];
  for (var i = 0; i < s.length; i++) bytes.push(s.charCodeAt(i));
  bytes.push(0xf7);
  midiOut.sendMidi(dev, bytes);
}
function receive(message) {
  var n = message.length;
  if (n < 6 || message[0] !== 0xf0 || message[1] !== 0x7d || message[2] !== 0x4e || message[3] !== 0x4a) return null;
  var s = '';
  for (var i = 4; i < n - 1; i++) s += String.fromCharCode(message[i]);
  try { return JSON.parse(s); } catch (e) { return null; }
}

// ---- state -------------------------------------------------------------------------------------

var dev = null; // the active device of the last callback
var phone = false; // a phone said hello
var page0 = 0; // window page (tracks page0 * SIZE ..)
var unity = -1; // the volume value Cubase shows as 0 dB, learnt from a fader at 0.00
var tracks = [];
var slots = [];
var dirtyTracks = {}; // i -> {field: true}
var dirtyList = false;
var dirtyDevices = false;
var dirtyTransport = false;
var watch = null; // {i, d}
var watchDirty = {};
var wantSelect = -1; // track to select for `list` or `open`
var lastMeters = 0;
var transport = { play: false, rec: false, loop: false };

function variable(name) { return surface.makeCustomValueVariable(name); }
function markTrack(i, field) {
  if (!dirtyTracks[i]) dirtyTracks[i] = {};
  dirtyTracks[i][field] = true;
}
function exists(t) { return t.name !== ''; }

// ---- the mixer window -------------------------------------------------------------------------

var zone = hostAccess.mMixConsole.makeMixerBankZone('NULL tracks')
  .excludeInputChannels().excludeOutputChannels();
var nextBank = variable('next bank');
var prevBank = variable('prev bank');
page.makeActionBinding(nextBank, zone.mAction.mNextBank);
page.makeActionBinding(prevBank, zone.mAction.mPrevBank);

function trackVar(i, name, hostValue, onValue, onText) {
  var v = variable(name + ' ' + i);
  page.makeValueBinding(v, hostValue);
  v.mOnProcessValueChange = function (activeDevice, value) { dev = activeDevice; onValue(value); };
  if (onText) v.mOnDisplayValueChange = function (activeDevice, value, units) { dev = activeDevice; onText(value, units); };
  return v;
}

for (var i = 0; i < SIZE; i++) (function (i) {
  var ch = zone.makeMixerBankChannel();
  var t = tracks[i] = { i: i, name: '', type: 'Audio', vol: 0, volText: '', pan: 0.5, panText: '', mute: false, solo: false, arm: false, sel: false, color: '', meter: 0, sends: [], sendTexts: [], vars: {} };
  ch.mOnTitleChange = function (activeDevice, activeMapping, title) {
    dev = activeDevice;
    var was = exists(t);
    t.name = title || '';
    if (was !== exists(t)) dirtyList = true; else markTrack(i, 'name');
  };
  ch.mOnColorChange = function (activeDevice, activeMapping, r, g, b) {
    dev = activeDevice;
    var hex = function (x) { return ('0' + Math.round(Math.max(0, Math.min(1, x)) * 255).toString(16)).slice(-2); };
    t.color = '#' + hex(r) + hex(g) + hex(b);
    markTrack(i, 'color');
  };
  var val = ch.mValue;
  t.vars.vol = trackVar(i, 'vol', val.mVolume, function (v) { t.vol = v; markTrack(i, 'vol'); },
    function (text, units) {
      t.volText = text + (units ? ' ' + units : '');
      if (parseFloat(text) === 0) unity = t.vol; // a fader at 0.00 dB
      markTrack(i, 'volText');
    });
  t.vars.pan = trackVar(i, 'pan', val.mPan, function (v) { t.pan = v; markTrack(i, 'pan'); },
    function (text) { t.panText = text; markTrack(i, 'panText'); });
  t.vars.mute = trackVar(i, 'mute', val.mMute, function (v) { t.mute = v > 0.5; markTrack(i, 'mute'); });
  t.vars.solo = trackVar(i, 'solo', val.mSolo, function (v) { t.solo = v > 0.5; markTrack(i, 'solo'); });
  t.vars.arm = trackVar(i, 'arm', val.mRecordEnable, function (v) { t.arm = v > 0.5; markTrack(i, 'arm'); });
  t.vars.sel = trackVar(i, 'sel', val.mSelected, function (v) {
    t.sel = v > 0.5;
    if (t.sel) dirtyDevices = true;
  });
  t.vars.meter = trackVar(i, 'meter', val.mVUMeter, function (v) { t.meter = v; });
  t.vars.sends = [];
  for (var k = 0; k < SENDS; k++) (function (k) {
    t.sends[k] = 0;
    t.sendTexts[k] = '';
    var slot = ch.mSends.getByIndex(k);
    t.vars.sends[k] = trackVar(i, 'send' + k, slot.mLevel, function (v) { t.sends[k] = v; markTrack(i, 'sends'); },
      function (text, units) { t.sendTexts[k] = text + (units ? ' ' + units : ''); markTrack(i, 'sends'); });
  })(k);
})(i);

// ---- the selected track's inserts --------------------------------------------------------------

var selected = hostAccess.mTrackSelection.mMixerChannel;
for (var d = 0; d < SLOTS; d++) (function (d) {
  var viewer = selected.mInsertAndStripEffects.makeInsertEffectViewer('NULL slot ' + d).accessSlotAtIndex(d);
  var s = slots[d] = { d: d, name: '', on: true, params: [], vars: [] };
  viewer.mOnChangePluginIdentity = function (activeDevice, activeMapping, name) {
    dev = activeDevice;
    s.name = name || '';
    dirtyDevices = true;
    if (watch && watch.d === d) watchDirty.all = true;
  };
  s.bypass = variable('bypass ' + d);
  page.makeValueBinding(s.bypass, viewer.mBypass);
  s.bypass.mOnProcessValueChange = function (activeDevice, v) { dev = activeDevice; s.on = !(v > 0.5); dirtyDevices = true; };
  for (var p = 0; p < PARAMS; p++) (function (p) {
    var x = variable('param ' + d + ' ' + p);
    page.makeValueBinding(x, viewer.mParameterBankZone.makeParameterValue());
    var q = s.params[p] = { name: '', v: 0, text: '' };
    s.vars[p] = x;
    x.mOnTitleChange = function (activeDevice, objectTitle, valueTitle) {
      dev = activeDevice;
      q.name = valueTitle || '';
      if (watch && watch.d === d) watchDirty.all = true;
    };
    x.mOnProcessValueChange = function (activeDevice, v) {
      dev = activeDevice;
      q.v = v;
      if (watch && watch.d === d) watchDirty['v' + p] = true;
    };
    x.mOnDisplayValueChange = function (activeDevice, text, units) {
      dev = activeDevice;
      q.text = text + (units ? ' ' + units : '');
      if (watch && watch.d === d) watchDirty['t' + p] = true;
    };
  })(p);
})(d);

// ---- transport ---------------------------------------------------------------------------------

var tv = hostAccess.mTransport.mValue;
function transportVar(name, hostValue, field) {
  var v = variable(name);
  page.makeValueBinding(v, hostValue);
  v.mOnProcessValueChange = function (activeDevice, value) {
    dev = activeDevice;
    if (field) { transport[field] = value > 0.5; dirtyTransport = true; }
  };
  return v;
}
var playVar = transportVar('play', tv.mStart, 'play');
var stopVar = transportVar('stop', tv.mStop, null);
var recVar = transportVar('record', tv.mRecord, 'rec');
var loopVar = transportVar('cycle', tv.mCycleActive, 'loop');
var startVar = variable('to start');
page.makeCommandBinding(startVar, 'Transport', 'To Project Start');

// ---- to the phone -----------------------------------------------------------------------------

function selectedIndex() {
  for (var i = 0; i < SIZE; i++) if (tracks[i].sel && exists(tracks[i])) return i;
  return -1;
}
function trackObject(t) {
  return { i: t.i, name: t.name, type: t.type, vol: t.vol, volText: t.volText, pan: t.pan, panText: t.panText,
    mute: t.mute, solo: t.solo, arm: t.arm, color: t.color, sends: t.sends.slice(), sendTexts: t.sendTexts.slice() };
}
function sendTracks(activeDevice) {
  var list = [];
  for (var i = 0; i < SIZE; i++) if (exists(tracks[i])) list.push(trackObject(tracks[i]));
  var full = list.length === SIZE;
  send(activeDevice, { t: 'tracks', total: page0 * SIZE + list.length + (full ? 1 : 0), from: page0 * SIZE, more: full, list: list });
  dirtyTracks = {};
  dirtyList = false;
}
function sendDevices(activeDevice) {
  var i = selectedIndex();
  dirtyDevices = false;
  if (i < 0) return;
  var list = [];
  for (var d = 0; d < SLOTS; d++) if (slots[d].name) list.push({ d: d, name: slots[d].name, on: slots[d].on, plugin: true });
  send(activeDevice, { t: 'devices', i: i, list: list });
}
function sendDevice(activeDevice) {
  if (!watch) return;
  var s = slots[watch.d];
  var ok = selectedIndex() === watch.i && !!s.name;
  send(activeDevice, { t: 'device', i: watch.i, d: watch.d, name: s.name, plugin: true, exists: ok });
  if (!ok) return;
  var ids = [];
  for (var p = 0; p < PARAMS; p++) if (s.params[p].name) ids.push(String(p));
  send(activeDevice, { t: 'pids', ids: ids });
  for (var k = 0; k < ids.length; k++) {
    var q = s.params[+ids[k]];
    send(activeDevice, { t: 'pname', id: ids[k], n: q.name });
    send(activeDevice, { t: 'pval', id: ids[k], v: q.v });
    if (q.text) send(activeDevice, { t: 'pdisp', id: ids[k], s: q.text });
  }
}
function hello(activeDevice) {
  phone = true;
  send(activeDevice, { t: 'hello', proto: PROTO, ext: 'cubase ' + VERSION, size: SIZE });
  var names = [];
  for (var k = 0; k < SENDS; k++) names.push(String(k + 1)); // the desk shows "Send 1"
  send(activeDevice, { t: 'sends', list: names });
  sendTracks(activeDevice);
  dirtyTransport = true;
}

// changes go out from the idle callback, at most once per field per call
function flush(activeDevice) {
  dev = activeDevice;
  if (!phone) return;
  if (dirtyList) sendTracks(activeDevice);
  for (var key in dirtyTracks) {
    var t = tracks[+key];
    if (!exists(t)) continue;
    var m = { t: 'track', i: t.i };
    var fields = dirtyTracks[key];
    for (var f in fields) {
      if (f === 'sends') { m.sends = t.sends.slice(); m.sendTexts = t.sendTexts.slice(); } else m[f] = t[f];
    }
    send(activeDevice, m);
  }
  dirtyTracks = {};
  if (wantSelect >= 0 && selectedIndex() === wantSelect) { wantSelect = -1; dirtyDevices = true; if (watch) watchDirty.all = true; }
  if (dirtyDevices) sendDevices(activeDevice);
  if (watch) {
    if (watchDirty.all) sendDevice(activeDevice);
    else {
      var s = slots[watch.d];
      for (var w in watchDirty) {
        var p = +w.slice(1);
        if (!s.params[p].name) continue;
        if (w.charAt(0) === 'v') send(activeDevice, { t: 'pval', id: String(p), v: s.params[p].v });
        else send(activeDevice, { t: 'pdisp', id: String(p), s: s.params[p].text });
      }
    }
    watchDirty = {};
  }
  if (dirtyTransport) {
    dirtyTransport = false;
    send(activeDevice, { t: 'transport', play: transport.play, rec: transport.rec, loop: transport.loop });
  }
  var now = Date.now();
  if (now - lastMeters >= METER_MS) {
    lastMeters = now;
    var v = [];
    for (var i = 0; i < SIZE; i++) v.push(exists(tracks[i]) ? Math.round(tracks[i].meter * 1000) / 1000 : 0);
    send(activeDevice, { t: 'meters', v: v });
  }
}

// ---- from the phone ---------------------------------------------------------------------------

function select(activeDevice, i) {
  var t = tracks[i];
  if (!t || !exists(t)) return;
  if (!t.sel) t.vars.sel.setProcessValue(activeDevice, 1);
  wantSelect = i;
}
function press(activeDevice, v) {
  v.setProcessValue(activeDevice, 1);
  v.setProcessValue(activeDevice, 0);
}

function handle(activeDevice, m) {
  var t = m.i != null ? tracks[m.i] : null;
  switch (m.t) {
    case 'hello': hello(activeDevice); return;
    case 'vol': if (t) t.vars.vol.setProcessValue(activeDevice, +m.v); return;
    case 'pan': if (t) t.vars.pan.setProcessValue(activeDevice, +m.v); return;
    case 'mute': case 'solo': case 'arm':
      if (t) t.vars[m.t].setProcessValue(activeDevice, m.on ? 1 : 0);
      return;
    case 'send': if (t && t.vars.sends[m.k]) t.vars.sends[m.k].setProcessValue(activeDevice, +m.v); return;
    case 'reset':
      if (!t) return;
      if (m.what === 'pan') t.vars.pan.setProcessValue(activeDevice, 0.5);
      else if (m.what === 'vol' && unity >= 0) t.vars.vol.setProcessValue(activeDevice, unity);
      else if (m.what === 'send' && t.vars.sends[m.k]) t.vars.sends[m.k].setProcessValue(activeDevice, 0);
      return;
    case 'scroll': {
      var to = Math.max(0, Math.floor((+m.from || 0) / SIZE));
      while (page0 < to) { press(activeDevice, nextBank); page0++; }
      while (page0 > to) { press(activeDevice, prevBank); page0--; }
      dirtyList = true;
      return;
    }
    case 'list':
      if (t) { select(activeDevice, m.i); if (selectedIndex() === m.i) dirtyDevices = true; }
      return;
    case 'open':
      if (!t || m.d == null || !slots[m.d]) return;
      watch = { i: m.i, d: m.d };
      select(activeDevice, m.i);
      watchDirty = { all: true };
      return;
    case 'close': watch = null; watchDirty = {}; return;
    case 'param':
      if (watch && slots[watch.d].vars[+m.id]) slots[watch.d].vars[+m.id].setProcessValue(activeDevice, +m.v);
      return;
    case 'bypass':
      if (slots[m.d] && selectedIndex() === m.i) slots[m.d].bypass.setProcessValue(activeDevice, m.on ? 0 : 1);
      return;
    case 'transport':
      if (m['do'] === 'play') playVar.setProcessValue(activeDevice, transport.play ? 0 : 1);
      else if (m['do'] === 'stop') press(activeDevice, stopVar);
      else if (m['do'] === 'rec') recVar.setProcessValue(activeDevice, transport.rec ? 0 : 1);
      else if (m['do'] === 'loop') loopVar.setProcessValue(activeDevice, transport.loop ? 0 : 1);
      else if (m['do'] === 'start') press(activeDevice, startVar);
      return;
    default:
  }
}

midiIn.mOnSysex = function (activeDevice, message) {
  dev = activeDevice;
  var m = receive(message);
  if (m && typeof m.t === 'string') handle(activeDevice, m);
};
driver.mOnIdle = function (activeDevice) { flush(activeDevice); };
driver.mOnActivate = function (activeDevice) { dev = activeDevice; phone = false; };
