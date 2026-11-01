// NULL JSFX Remote for Bitwig Studio: a controller script that lets the Android app
// Remote for DAWs (android/web/daws/bitwig/) run Bitwig's mixer, transport and plugins,
// with the NULL JSFX plugins drawn by their own interface on the phone.
// NULL JSFX • https://nulljsfx.tech • MIT License
//
// Install: copy the folder NullJSFX into Documents/Bitwig Studio/Controller Scripts, then
// Settings > Controllers > Add controller > NULL JSFX > Remote for DAWs. Nothing else.
//
// The phone connects over TCP (port 8818 unless changed in the controller's settings).
// Protocol version 1, JSON objects in UTF-8:
//   phone -> Bitwig: one object per message, each with Bitwig's 4-byte big-endian length in
//     front (RemoteConnection's framing). {"t": ...}:
//       hello                              -> everything below, from scratch
//       vol|pan  i v(0..1)                 track i of the window (i < 0: nothing)
//       mute|solo|arm  i on
//       reset  i what("vol"|"pan")
//       open  i d     the device d of track i becomes the one whose parameters are reported
//       close
//       param  id v(0..1)                  a parameter of the open device
//       bypass  i d on                     on = active
//       transport  do("play"|"stop"|"rec"|"loop"|"start")
//       scroll  from                       first track of the window
//       send  i k v(0..1)                  send k of track i (to effect track k)
//       reset  i what("send") k            send k back to its default
//   Bitwig -> phone: one object per line ("\n"); a send holds one or more lines. (If Bitwig puts
//   its length header in front of a send as well, the phone skips it.)
//       hello  proto ext size              size = tracks in the window
//       tracks  total from list[{i name type vol volText pan panText mute solo arm color sends sendTexts}]
//                                          sends[k]: 0..1, or null where track i has no send k
//       sends  list[name]                  the effect tracks, the targets of sends 0..SENDS-1
//       track  i + the fields that changed
//       devices  i list[{d name on plugin}]
//       meters  v[]                         peak per track of the window, 0..1
//       transport  play rec loop
//       time  s                             bars:beats:sixteenths
//       device  i d name plugin exists      the open device (where the cursor really is)
//       pids  ids[]                         its parameters, in order
//       pname  id n,  pval  id v(0..1),  pdisp  id s
//
// Written for Bitwig's script engine: ES5 only (var, function, no arrows or template strings).

loadAPI(10);
host.setShouldFailOnDeprecatedUse(false);
host.defineController('NULL JSFX', 'Remote for DAWs', '1.1', 'cfc6570f-19fc-4fe2-86ee-bebc547acd68', 'NULL JSFX');
host.defineMidiPorts(0, 0);

var PROTO = 1;
var VERSION = '1.1'; // 1.1: sends
var DEFAULT_PORT = 8818;
var SIZE = 32; // tracks in the window
var DEVICES = 16; // devices per track
var TICK = 40; // ms between meter and clock updates
var SENDS = 8; // sends per track, to the first effect tracks

var bank, fxBank, cursorTrack, cursorDevice, transport, formatter, displays;
var sendNames = []; // per send slot: the effect track's name, or null
var dirtySends = false;
var tracks = []; // per window slot: state sent to the phone
var devices = []; // per window slot: [{name, on, plugin, exists}]
var deviceBanks = [];
var meters = [];
var metersSent = [];
var total = 0;
var from = 0;
var clients = [];
var out = []; // lines waiting for flush()
var dirtyTracks = false;
var dirtyTrack = {}; // slot -> changed fields
var dirtyDevices = {}; // slot -> true
var tstate = { play: false, rec: false, loop: false };
var dirtyTransport = false;
var timeText = '';
var timeSent = '';
var open = { i: -1, d: -1, name: '', plugin: false, exists: false, pos: -1 };
var dirtyOpen = false;
var pids = [];
var pnames = {};
var pvals = {};
var pdisps = {};

function init() {
  var port = DEFAULT_PORT;
  try {
    var setting = host.getPreferences().getNumberSetting('Port', 'Network (restart the controller after a change)', 1024, 65535, 1, '', DEFAULT_PORT);
    setting.markInterested();
    var raw = setting.getRaw();
    if (raw >= 1024 && raw <= 65535) port = Math.round(raw);
  } catch (e) { /* older hosts: the default port */ }

  transport = host.createTransport();
  formatter = host.createBeatTimeFormatter(':', 3, 1, 1, 0);
  transport.isPlaying().addValueObserver(function (v) { tstate.play = v; dirtyTransport = true; });
  transport.isArrangerRecordEnabled().addValueObserver(function (v) { tstate.rec = v; dirtyTransport = true; });
  transport.isArrangerLoopEnabled().addValueObserver(function (v) { tstate.loop = v; dirtyTransport = true; });
  var position = transport.getPosition();
  position.markInterested();
  position.addValueObserver(function () { timeText = position.getFormatted(formatter); });

  // every track of the document, effect tracks and the master included, nested ones flattened
  bank = host.createTrackBank(SIZE, SENDS, 0, true);
  bank.itemCount().addValueObserver(function (n) { total = n; dirtyTracks = true; });
  bank.scrollPosition().addValueObserver(function (n) { from = n; dirtyTracks = true; });
  for (var i = 0; i < SIZE; i++) watchTrack(i);
  // the names of the send targets: the effect tracks, in the order of every track's sends
  fxBank = host.createEffectTrackBank(SENDS, 0);
  for (var k = 0; k < SENDS; k++) watchSendName(k);

  // the device whose parameters the phone sees: its own cursors, so Bitwig's selection stays
  cursorTrack = host.createCursorTrack('null-jsfx-remote', 'NULL JSFX Remote', 0, 0, false);
  cursorDevice = cursorTrack.createCursorDevice();
  cursorDevice.isPinned().markInterested();
  cursorDevice.exists().addValueObserver(function (v) { open.exists = v; dirtyOpen = true; });
  cursorDevice.name().addValueObserver(function (v) { open.name = String(v); dirtyOpen = true; });
  cursorDevice.isPlugin().addValueObserver(function (v) { open.plugin = v; dirtyOpen = true; });
  cursorDevice.position().addValueObserver(function (v) { open.pos = v; dirtyOpen = true; });
  cursorDevice.addDirectParameterIdObserver(function (ids) {
    pids = [];
    for (var k = 0; k < ids.length; k++) pids.push(String(ids[k]));
    pnames = {};
    pvals = {};
    pdisps = {};
    queue({ t: 'pids', ids: pids });
    displays.setObservedParameterIds(null);
    var watched = pids.slice(0);
    // as Bitwig's own scripts do: start the display observer once the cursor has settled
    host.scheduleTask(function () { if (watched.length) displays.setObservedParameterIds(watched); }, 50);
  });
  cursorDevice.addDirectParameterNameObserver(64, function (id, name) {
    id = String(id);
    pnames[id] = String(name);
    queue({ t: 'pname', id: id, n: pnames[id] });
  });
  cursorDevice.addDirectParameterNormalizedValueObserver(function (id, v) {
    id = String(id);
    if (v !== v) return; // NaN: not readable
    pvals[id] = v;
    queue({ t: 'pval', id: id, v: v });
  });
  displays = cursorDevice.addDirectParameterValueDisplayObserver(24, function (id, s) {
    id = String(id);
    pdisps[id] = String(s);
    queue({ t: 'pdisp', id: id, s: pdisps[id] });
  });

  var socket = host.createRemoteConnection('NULL JSFX Remote', port);
  socket.setClientConnectCallback(function (c) { addClient(c); });
  host.scheduleTask(tick, TICK);
  println('NULL JSFX Remote ' + VERSION + ': listening on TCP ' + port);
}

function watchTrack(i) {
  var t = bank.getItemAt(i);
  var s = { i: i, exists: false, name: '', type: '', vol: 0, volText: '', pan: 0.5, panText: '', mute: false, solo: false, arm: false, color: '', sends: [], sendTexts: [], sendOn: [] };
  tracks[i] = s;
  meters[i] = 0;
  metersSent[i] = -1;
  var changed = function (field) {
    return function (v) {
      // names and texts come from Java: plain JS strings for JSON
      s[field] = typeof v === 'boolean' || typeof v === 'number' ? v : String(v);
      if (field === 'exists' || field === 'name' || field === 'type') dirtyTracks = true;
      else mark(i, field);
    };
  };
  t.exists().addValueObserver(changed('exists'));
  t.name().addValueObserver(changed('name'));
  t.trackType().addValueObserver(changed('type'));
  t.volume().value().addValueObserver(changed('vol'));
  t.volume().value().displayedValue().addValueObserver(changed('volText'));
  t.pan().value().addValueObserver(changed('pan'));
  t.pan().value().displayedValue().addValueObserver(changed('panText'));
  t.mute().addValueObserver(changed('mute'));
  t.solo().addValueObserver(changed('solo'));
  t.arm().addValueObserver(changed('arm'));
  t.color().addValueObserver(function (r, g, b) { s.color = hex(r, g, b); mark(i, 'color'); });
  t.addVuMeterObserver(1000, -1, true, function (v) { meters[i] = v / 999; });

  for (var k = 0; k < SENDS; k++) watchSend(i, s, t.sendBank().getItemAt(k), k);

  var db = t.createDeviceBank(DEVICES);
  deviceBanks[i] = db;
  devices[i] = [];
  for (var d = 0; d < DEVICES; d++) watchDevice(i, db, d);
}

function watchSend(i, s, send, k) {
  s.sends[k] = 0;
  s.sendTexts[k] = '';
  s.sendOn[k] = false;
  send.exists().addValueObserver(function (v) { s.sendOn[k] = v; mark(i, 'sends'); });
  send.value().addValueObserver(function (v) { s.sends[k] = v; mark(i, 'sends'); });
  send.value().displayedValue().addValueObserver(function (v) { s.sendTexts[k] = String(v); mark(i, 'sends'); });
}

function watchSendName(k) {
  var fx = fxBank.getItemAt(k);
  var on = false;
  var name = '';
  sendNames[k] = null;
  var set = function () { sendNames[k] = on ? name : null; dirtySends = true; };
  fx.exists().addValueObserver(function (v) { on = v; set(); });
  fx.name().addValueObserver(function (v) { name = String(v); set(); });
}

function sendsOut() {
  // the names up to the last send that exists
  var list = sendNames.slice(0);
  while (list.length && list[list.length - 1] == null) list.pop();
  return { t: 'sends', list: list };
}

function watchDevice(i, db, d) {
  var dev = db.getItemAt(d);
  var s = { d: d, exists: false, name: '', on: true, plugin: false };
  devices[i][d] = s;
  var changed = function (field) {
    return function (v) { s[field] = field === 'name' ? String(v) : v; dirtyDevices[i] = true; };
  };
  dev.exists().addValueObserver(changed('exists'));
  dev.name().addValueObserver(changed('name'));
  dev.isEnabled().addValueObserver(changed('on'));
  dev.isPlugin().addValueObserver(changed('plugin'));
}

function mark(i, field) {
  if (!dirtyTrack[i]) dirtyTrack[i] = {};
  dirtyTrack[i][field] = true;
}

function hex(r, g, b) {
  var c = function (x) { var n = Math.max(0, Math.min(255, Math.round(x * 255))); return (n < 16 ? '0' : '') + n.toString(16); };
  return '#' + c(r) + c(g) + c(b);
}

// --- what goes to the phone --------------------------------------------------------------------

function trackOut(s) {
  var m = { i: s.i, name: s.name, type: s.type, vol: s.vol, volText: s.volText, pan: s.pan, panText: s.panText, mute: s.mute, solo: s.solo, arm: s.arm, color: s.color };
  addSends(m, s);
  return m;
}

function addSends(m, s) {
  m.sends = [];
  m.sendTexts = [];
  for (var k = 0; k < SENDS; k++) {
    m.sends.push(s.sendOn[k] ? s.sends[k] : null);
    m.sendTexts.push(s.sendOn[k] ? s.sendTexts[k] : '');
  }
}

function trackList() {
  var list = [];
  for (var i = 0; i < SIZE; i++) if (tracks[i].exists) list.push(trackOut(tracks[i]));
  return { t: 'tracks', total: total, from: from, list: list };
}

function deviceList(i) {
  var list = [];
  var ds = devices[i];
  for (var d = 0; d < ds.length; d++) {
    if (ds[d].exists) list.push({ d: d, name: ds[d].name, on: ds[d].on, plugin: ds[d].plugin });
  }
  return { t: 'devices', i: i, list: list };
}

function openOut() {
  return { t: 'device', i: open.i, d: open.d, name: open.name, plugin: open.plugin, exists: open.exists, pos: open.pos };
}

function queue(m) {
  if (!clients.length) return;
  out.push(JSON.stringify(m));
  host.requestFlush();
}

function hello(c) {
  var lines = [];
  var add = function (m) { lines.push(JSON.stringify(m)); };
  add({ t: 'hello', proto: PROTO, ext: VERSION, size: SIZE });
  add(sendsOut());
  add(trackList());
  for (var i = 0; i < SIZE; i++) if (tracks[i].exists) add(deviceList(i));
  add({ t: 'transport', play: tstate.play, rec: tstate.rec, loop: tstate.loop });
  add({ t: 'time', s: timeText });
  if (open.i >= 0) {
    add(openOut());
    add({ t: 'pids', ids: pids });
    for (var k = 0; k < pids.length; k++) {
      var id = pids[k];
      if (pnames[id] != null) add({ t: 'pname', id: id, n: pnames[id] });
      if (pvals[id] != null) add({ t: 'pval', id: id, v: pvals[id] });
      if (pdisps[id] != null) add({ t: 'pdisp', id: id, s: pdisps[id] });
    }
  }
  sendTo(c, lines);
}

function flush() {
  if (!clients.length) {
    dirtyTracks = false;
    dirtyTrack = {};
    dirtyDevices = {};
    dirtyTransport = false;
    dirtyOpen = false;
    dirtySends = false;
    out = [];
    return;
  }
  var lines = [];
  if (dirtySends) lines.push(JSON.stringify(sendsOut()));
  if (dirtyTracks) {
    lines.push(JSON.stringify(trackList()));
    dirtyTrack = {};
  }
  for (var k in dirtyTrack) {
    var i = Number(k);
    var s = tracks[i];
    if (!s.exists) continue;
    var m = { t: 'track', i: i };
    for (var f in dirtyTrack[k]) {
      if (f === 'sends') addSends(m, s);
      else m[f] = s[f];
    }
    lines.push(JSON.stringify(m));
  }
  for (var d in dirtyDevices) if (tracks[Number(d)].exists) lines.push(JSON.stringify(deviceList(Number(d))));
  if (dirtyTransport) lines.push(JSON.stringify({ t: 'transport', play: tstate.play, rec: tstate.rec, loop: tstate.loop }));
  if (dirtyOpen && open.i >= 0) lines.push(JSON.stringify(openOut()));
  dirtyTracks = false;
  dirtyTrack = {};
  dirtyDevices = {};
  dirtyTransport = false;
  dirtyOpen = false;
  dirtySends = false;
  // parameter messages queue in the order Bitwig reported them: after the device line
  lines = lines.concat(out);
  out = [];
  if (lines.length) broadcast(lines);
}

function tick() {
  if (clients.length) {
    var lines = [];
    var v = [];
    var moved = false;
    for (var i = 0; i < SIZE; i++) {
      var x = Math.round(meters[i] * 1000) / 1000;
      v.push(x);
      if (x !== metersSent[i]) moved = true;
      metersSent[i] = x;
    }
    if (moved) lines.push(JSON.stringify({ t: 'meters', v: v }));
    if (timeText !== timeSent) {
      timeSent = timeText;
      lines.push(JSON.stringify({ t: 'time', s: timeText }));
    }
    if (lines.length) broadcast(lines);
  }
  host.scheduleTask(tick, TICK);
}

// --- connections -------------------------------------------------------------------------------

function addClient(c) {
  var client = { c: c, buf: [] };
  clients.push(client);
  c.setDisconnectCallback(function () {
    for (var k = 0; k < clients.length; k++) if (clients[k] === client) clients.splice(k, 1);
  });
  c.setReceiveCallback(function (data) {
    var text = utf8Decode(data);
    var m;
    try { m = JSON.parse(text); } catch (e) { return; }
    try { handle(client, m); } catch (e2) { println('NULL JSFX Remote: ' + e2); }
  });
}

function broadcast(lines) {
  for (var k = 0; k < clients.length; k++) sendTo(clients[k], lines);
}

function sendTo(client, lines) {
  try {
    client.c.send(utf8Encode(lines.join('\n') + '\n'));
  } catch (e) { /* the disconnect callback removes it */ }
}

function utf8Decode(data) {
  var s = '';
  var n = data.length;
  for (var k = 0; k < n; k++) {
    var b = data[k] & 0xff;
    if (b < 0x80) { s += String.fromCharCode(b); continue; }
    var c;
    if (b >= 0xf0 && k + 3 < n) {
      c = ((b & 7) << 18) | ((data[k + 1] & 63) << 12) | ((data[k + 2] & 63) << 6) | (data[k + 3] & 63);
      k += 3;
      c -= 0x10000;
      s += String.fromCharCode(0xd800 + (c >> 10), 0xdc00 + (c & 1023));
      continue;
    }
    if (b >= 0xe0 && k + 2 < n) {
      c = ((b & 15) << 12) | ((data[k + 1] & 63) << 6) | (data[k + 2] & 63);
      k += 2;
    } else if (b >= 0xc0 && k + 1 < n) {
      c = ((b & 31) << 6) | (data[k + 1] & 63);
      k += 1;
    } else {
      c = 0xfffd;
    }
    s += String.fromCharCode(c);
  }
  return s;
}

// signed bytes, as RemoteConnection.send wants them from a script
function utf8Encode(s) {
  var a = [];
  var push = function (b) { a.push(b > 127 ? b - 256 : b); };
  for (var k = 0; k < s.length; k++) {
    var c = s.charCodeAt(k);
    if (c >= 0xd800 && c < 0xdc00 && k + 1 < s.length) {
      c = 0x10000 + ((c - 0xd800) << 10) + (s.charCodeAt(++k) - 0xdc00);
    }
    if (c < 0x80) push(c);
    else if (c < 0x800) { push(0xc0 | (c >> 6)); push(0x80 | (c & 63)); }
    else if (c < 0x10000) { push(0xe0 | (c >> 12)); push(0x80 | ((c >> 6) & 63)); push(0x80 | (c & 63)); }
    else { push(0xf0 | (c >> 18)); push(0x80 | ((c >> 12) & 63)); push(0x80 | ((c >> 6) & 63)); push(0x80 | (c & 63)); }
  }
  return a;
}

// --- what the phone asks -----------------------------------------------------------------------

function slot(i) {
  i = Number(i);
  return i >= 0 && i < SIZE && tracks[i].exists ? bank.getItemAt(i) : null;
}

function unit(v) {
  v = Number(v);
  return v !== v ? null : Math.max(0, Math.min(1, v));
}

function handle(client, m) {
  var t = m && m.t;
  var tr;
  var v;
  if (t === 'hello') return hello(client);
  if (t === 'vol' || t === 'pan') {
    tr = slot(m.i);
    v = unit(m.v);
    if (tr && v != null) (t === 'vol' ? tr.volume() : tr.pan()).value().set(v);
    return;
  }
  if (t === 'mute' || t === 'solo' || t === 'arm') {
    tr = slot(m.i);
    if (tr) tr[t]().set(!!m.on);
    return;
  }
  if (t === 'reset') {
    tr = slot(m.i);
    if (tr && m.what === 'vol') tr.volume().reset();
    if (tr && m.what === 'pan') tr.pan().reset();
    if (tr && m.what === 'send' && Number(m.k) >= 0 && Number(m.k) < SENDS) tr.sendBank().getItemAt(Number(m.k)).reset();
    return;
  }
  if (t === 'send') {
    tr = slot(m.i);
    v = unit(m.v);
    var sk = Number(m.k);
    if (tr && v != null && sk >= 0 && sk < SENDS) tr.sendBank().getItemAt(sk).value().set(v);
    return;
  }
  if (t === 'open') {
    tr = slot(m.i);
    var d = Number(m.d);
    if (!tr || !(d >= 0 && d < DEVICES)) return;
    open.i = Number(m.i);
    open.d = d;
    cursorTrack.selectChannel(tr);
    cursorDevice.isPinned().set(true);
    cursorDevice.selectDevice(deviceBanks[open.i].getItemAt(d));
    dirtyOpen = true;
    host.requestFlush();
    return;
  }
  if (t === 'close') {
    open.i = -1;
    open.d = -1;
    displays.setObservedParameterIds(null);
    return;
  }
  if (t === 'param') {
    v = unit(m.v);
    if (open.i < 0 || v == null || typeof m.id !== 'string') return;
    // value in [0, resolution - 1]: a fine integer grid that reads the same whichever way the
    // host divides (by resolution - 1, as documented, or by resolution)
    cursorDevice.setDirectParameterValueNormalized(m.id, Math.round(v * 1000000), 1000001);
    return;
  }
  if (t === 'bypass') {
    tr = slot(m.i);
    var b = Number(m.d);
    if (tr && b >= 0 && b < DEVICES) deviceBanks[Number(m.i)].getItemAt(b).isEnabled().set(!!m.on);
    return;
  }
  if (t === 'transport') {
    if (m['do'] === 'play') transport.play();
    else if (m['do'] === 'stop') transport.stop();
    else if (m['do'] === 'rec') transport.isArrangerRecordEnabled().toggle();
    else if (m['do'] === 'loop') transport.isArrangerLoopEnabled().toggle();
    else if (m['do'] === 'start') transport.getPosition().set(0);
    return;
  }
  if (t === 'scroll') {
    var f = Math.max(0, Math.round(Number(m.from) || 0));
    bank.scrollPosition().set(f);
  }
}

function exit() {
  for (var k = 0; k < clients.length; k++) {
    try { clients[k].c.disconnect(); } catch (e) { /* gone */ }
  }
  clients = [];
}
