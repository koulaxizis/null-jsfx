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
//       list  i                            track i's devices with those inside containers (1.2)
//       reset  i what("send") k            send k back to its default
//       hello  key                         key (1.3): the phone's key for this computer, if any
//       pair  code key                     the 6-digit code shown here, and the phone's new key
//   Bitwig -> phone: one object per line ("\n"); a send holds one or more lines. (If Bitwig puts
//   its length header in front of a send as well, the phone skips it.)
//       hello  proto ext size              size = tracks in the window
//       tracks  total from list[{i name type vol volText pan panText mute solo arm color sends sendTexts}]
//                                          sends[k]: 0..1, or null where track i has no send k
//       sends  list[name]                  the effect tracks, the targets of sends 0..SENDS-1
//       track  i + the fields that changed
//       devices  i list[{d name on plugin depth chain}]
//                                          depth 1: inside the container listed before it, in its
//                                          layer or slot `chain`; d >= 1000 for those (1.2)
//       meters  v[]                         peak per track of the window, 0..1
//       transport  play rec loop
//       time  s                             bars:beats:sixteenths
//       device  i d name plugin exists      the open device (where the cursor really is)
//       pids  ids[]                         its parameters, in order
//       pname  id n,  pval  id v(0..1),  pdisp  id s
//       auth  need("code"|"wait") wrong s   (1.3) the phone must pair first: Bitwig shows a code;
//                                          wrong: the last code was not it; wait: s seconds
//       paired                             (1.3) the key is kept; everything follows as for hello
//
// Pairing (1.3): a phone talks to the script only once it holds a key this computer knows.
// Until then the script answers nothing but auth, carries out nothing, and shows a 6-digit code
// in a Bitwig notification; the phone sends that code once with a new random key, and the
// script keeps a hash of the key in its preferences (Settings > Controllers, "Paired phones",
// where "Forget all phones" clears them). A code lives 2 minutes; after 5 wrong codes there is
// a new one and a pause of 30 s. Messages per connection are limited, and so are connections.
//
// Written for Bitwig's script engine: ES5 only (var, function, no arrows or template strings).

loadAPI(10);
host.setShouldFailOnDeprecatedUse(false);
host.defineController('NULL JSFX', 'Remote for DAWs', '1.3', 'cfc6570f-19fc-4fe2-86ee-bebc547acd68', 'NULL JSFX');
host.defineMidiPorts(0, 0);

var PROTO = 1;
var VERSION = '1.3'; // 1.1: sends; 1.2: devices inside containers; 1.3: pairing
var DEFAULT_PORT = 8818;
var SIZE = 32; // tracks in the window
var DEVICES = 16; // devices per track
var TICK = 40; // ms between meter and clock updates
var SENDS = 8; // sends per track, to the first effect tracks
var LAYERS = 4; // layers of a container shown (FX Layer, Instrument Layer, Selectors, ...)
var INNER = 8; // devices per layer or slot shown
var CODE_LIFE = 120000; // ms a pairing code is valid
var TRIES = 5; // wrong codes before a new one and a pause
var PAUSE = 30000; // ms
var UNPAIRED_LIFE = 180000; // ms a connection may stay without pairing
var MAX_CLIENTS = 8;
var RATE = 400; // messages per second and connection; more are dropped
var MAX_KEYS = 16; // phones remembered

var bank, fxBank, cursorTrack, cursorDevice, transport, formatter, displays;
var sendNames = []; // per send slot: the effect track's name, or null
var dirtySends = false;
// the devices of the script's cursor track, containers opened one level: the track the phone
// listed or opened last (slot `listed`); the other tracks show their top-level devices
var listed = -1;
var tree = []; // per top device: {exists, name, on, plugin, layers[{exists, name, devs[]}], slot{name, devs[]}}
var inner = {}; // id -> the API's device, for open and bypass
var dirtyTree = false;
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
var keySetting = null; // the hashes of the paired phones' keys, comma separated
var keys = [];
var code = { value: '', until: 0, tries: 0, pauseUntil: 0, shown: 0 };

function init() {
  var port = DEFAULT_PORT;
  try {
    var setting = host.getPreferences().getNumberSetting('Port', 'Network (restart the controller after a change)', 1024, 65535, 1, '', DEFAULT_PORT);
    setting.markInterested();
    var raw = setting.getRaw();
    if (raw >= 1024 && raw <= 65535) port = Math.round(raw);
  } catch (e) { /* older hosts: the default port */ }
  try {
    keySetting = host.getPreferences().getStringSetting('Keys', 'Paired phones', 600, '');
    keySetting.addValueObserver(function (v) { keys = String(v || '').split(',').filter(function (k) { return /^[0-9a-f]{32}$/.test(k); }); });
    var forget = host.getPreferences().getSignalSetting('Forget', 'Paired phones', 'Forget all phones');
    forget.addSignalObserver(function () { setKeys([]); dropAll(); });
  } catch (e) { /* older hosts: phones pair again after every restart */ }

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
  bank.scrollPosition().addValueObserver(function (n) { from = n; listed = -1; dirtyTracks = true; });
  for (var i = 0; i < SIZE; i++) watchTrack(i);
  // the names of the send targets: the effect tracks, in the order of every track's sends
  fxBank = host.createEffectTrackBank(SENDS, 0);
  for (var k = 0; k < SENDS; k++) watchSendName(k);

  // the device whose parameters the phone sees: its own cursors, so Bitwig's selection stays
  cursorTrack = host.createCursorTrack('null-jsfx-remote', 'NULL JSFX Remote', 0, 0, false);
  cursorDevice = cursorTrack.createCursorDevice();
  var treeBank = cursorTrack.createDeviceBank(DEVICES);
  for (var td = 0; td < DEVICES; td++) watchTree(treeBank.getItemAt(td), td);
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

function devState(dev, onChange) {
  var s = { exists: false, name: '', on: true, plugin: false };
  dev.exists().addValueObserver(function (v) { s.exists = v; onChange(); });
  dev.name().addValueObserver(function (v) { s.name = String(v); onChange(); });
  dev.isEnabled().addValueObserver(function (v) { s.on = v; onChange(); });
  dev.isPlugin().addValueObserver(function (v) { s.plugin = v; onChange(); });
  return s;
}

function watchTree(dev, d) {
  var dirty = function () { dirtyTree = true; };
  var top = devState(dev, dirty);
  top.layers = [];
  var layers = dev.createLayerBank(LAYERS);
  for (var l = 0; l < LAYERS; l++) {
    var layer = layers.getItemAt(l);
    var ls = { exists: false, name: '', devs: [] };
    (function (ls) {
      layer.exists().addValueObserver(function (v) { ls.exists = v; dirty(); });
      layer.name().addValueObserver(function (v) { ls.name = String(v); dirty(); });
    })(ls);
    var lb = layer.createDeviceBank(INNER);
    for (var j = 0; j < INNER; j++) {
      ls.devs[j] = devState(lb.getItemAt(j), dirty);
      inner[1000 + d * 100 + l * 10 + j] = lb.getItemAt(j);
    }
    top.layers[l] = ls;
  }
  // a container with a slot (Chain, a device's FX slot): the first slot's devices
  var slot = dev.getCursorSlot();
  top.slot = { name: '', devs: [] };
  dev.slotNames().addValueObserver(function (names) {
    var first = names && names.length ? String(names[0]) : '';
    if (first && first !== top.slot.name) slot.selectSlot(first);
    top.slot.name = first;
    dirty();
  });
  var sb = slot.createDeviceBank(INNER);
  for (var k = 0; k < INNER; k++) {
    top.slot.devs[k] = devState(sb.getItemAt(k), dirty);
    inner[1000 + d * 100 + 90 + k] = sb.getItemAt(k);
  }
  tree[d] = top;
}

// track i's devices with one level inside containers, from the cursor track's banks
function treeList(i) {
  var list = [];
  var add = function (d, s, depth, chain) {
    var m = { d: d, name: s.name, on: s.on, plugin: s.plugin };
    if (depth) { m.depth = depth; m.chain = chain; }
    list.push(m);
  };
  for (var d = 0; d < DEVICES; d++) {
    var top = tree[d];
    if (!top.exists) continue;
    add(d, top, 0, '');
    for (var l = 0; l < LAYERS; l++) {
      var ls = top.layers[l];
      if (!ls.exists) continue;
      for (var j = 0; j < INNER; j++) if (ls.devs[j].exists) add(1000 + d * 100 + l * 10 + j, ls.devs[j], 1, ls.name);
    }
    for (var k = 0; k < INNER; k++) if (top.slot.devs[k].exists) add(1000 + d * 100 + 90 + k, top.slot.devs[k], 1, top.slot.name);
  }
  return { t: 'devices', i: i, list: list };
}

// the API's device for id d of track slot i (null when the cursor track is elsewhere)
function deviceAt(i, d) {
  if (d >= 0 && d < DEVICES) return deviceBanks[i].getItemAt(d);
  if (d >= 1000 && i === listed && inner[d]) return inner[d];
  return null;
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
  if (i === listed) return treeList(i);
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
  if (!paired()) return;
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
  if (!paired()) {
    dirtyTracks = false;
    dirtyTrack = {};
    dirtyDevices = {};
    dirtyTransport = false;
    dirtyOpen = false;
    dirtySends = false;
    dirtyTree = false;
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
  if (dirtyTree && listed >= 0) dirtyDevices[listed] = true;
  for (var d in dirtyDevices) if (tracks[Number(d)].exists) lines.push(JSON.stringify(deviceList(Number(d))));
  if (dirtyTransport) lines.push(JSON.stringify({ t: 'transport', play: tstate.play, rec: tstate.rec, loop: tstate.loop }));
  if (dirtyOpen && open.i >= 0) lines.push(JSON.stringify(openOut()));
  dirtyTracks = false;
  dirtyTrack = {};
  dirtyDevices = {};
  dirtyTransport = false;
  dirtyOpen = false;
  dirtySends = false;
  dirtyTree = false;
  // parameter messages queue in the order Bitwig reported them: after the device line
  lines = lines.concat(out);
  out = [];
  if (lines.length) broadcast(lines);
}

function tick() {
  expire();
  if (paired()) {
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
  if (clients.length >= MAX_CLIENTS) {
    // full: the oldest connection that never paired makes room, else the new one goes
    var w = -1;
    for (var j = 0; j < clients.length && w < 0; j++) if (!clients[j].paired) w = j;
    if (w < 0) {
      try { c.disconnect(); } catch (e) { /* gone */ }
      return;
    }
    var old = clients.splice(w, 1)[0];
    try { old.c.disconnect(); } catch (e3) { /* gone */ }
  }
  var client = { c: c, buf: [], paired: false, born: Date.now(), second: 0, count: 0 };
  clients.push(client);
  c.setDisconnectCallback(function () {
    for (var k = 0; k < clients.length; k++) if (clients[k] === client) clients.splice(k, 1);
  });
  c.setReceiveCallback(function (data) {
    var now = Date.now();
    var second = Math.floor(now / 1000);
    if (second !== client.second) { client.second = second; client.count = 0; }
    if (++client.count > RATE) return;
    if (data.length > 65536) return;
    var text = utf8Decode(data);
    var m;
    try { m = JSON.parse(text); } catch (e) { return; }
    if (!m || typeof m !== 'object') return;
    try {
      if (client.paired) handle(client, m);
      else pairing(client, m, now);
    } catch (e2) { println('NULL JSFX Remote: ' + e2); }
  });
}

function paired() {
  for (var k = 0; k < clients.length; k++) if (clients[k].paired) return true;
  return false;
}

function broadcast(lines) {
  for (var k = 0; k < clients.length; k++) if (clients[k].paired) sendTo(clients[k], lines);
}

// --- pairing -----------------------------------------------------------------------------------

function validKey(k) { return typeof k === 'string' && /^[0-9a-f]{32,128}$/.test(k); }
function keyHash(k) { return sha256(k).slice(0, 32); }

function setKeys(list) {
  keys = list.slice(-MAX_KEYS);
  if (keySetting) keySetting.set(keys.join(','));
}

function known(k) {
  if (!validKey(k)) return false;
  var h = keyHash(k);
  var hit = false;
  for (var i = 0; i < keys.length; i++) if (keys[i] === h) hit = true;
  return hit;
}

function showCode(now) {
  if (!code.value || now >= code.until) {
    code.value = String(100000 + Math.floor(Math.random() * 900000)).slice(-6);
    code.until = now + CODE_LIFE;
    code.tries = 0;
    code.shown = 0;
  }
  if (now - code.shown >= 10000) {
    code.shown = now;
    host.showPopupNotification('NULL JSFX Remote: pairing code ' + code.value.slice(0, 3) + ' ' + code.value.slice(3));
  }
}

function welcome(client) {
  client.paired = true;
  sendTo(client, [JSON.stringify({ t: 'paired' })]);
  hello(client);
}

function pairing(client, m, now) {
  if (m.t === 'hello') {
    if (known(m.key)) return welcome(client);
    client.asked = true;
  } else if (m.t === 'pair') {
    if (now < code.pauseUntil) {
      return sendTo(client, [JSON.stringify({ t: 'auth', need: 'wait', s: Math.ceil((code.pauseUntil - now) / 1000) })]);
    }
    if (code.value && now < code.until && validKey(m.key) && String(m.code).replace(/\D/g, '') === code.value) {
      code.value = '';
      setKeys(keys.concat([keyHash(m.key)]));
      host.showPopupNotification('NULL JSFX Remote: phone paired');
      return welcome(client);
    }
    if (++code.tries >= TRIES) {
      code.value = '';
      code.pauseUntil = now + PAUSE;
      return sendTo(client, [JSON.stringify({ t: 'auth', need: 'wait', s: PAUSE / 1000 })]);
    }
    showCode(now);
    return sendTo(client, [JSON.stringify({ t: 'auth', need: 'code', wrong: true })]);
  } else {
    return; // nothing else before pairing
  }
  if (now < code.pauseUntil) {
    return sendTo(client, [JSON.stringify({ t: 'auth', need: 'wait', s: Math.ceil((code.pauseUntil - now) / 1000) })]);
  }
  showCode(now);
  sendTo(client, [JSON.stringify({ t: 'auth', need: 'code' })]);
}

// connections that never paired go after a while
function expire() {
  var now = Date.now();
  for (var k = clients.length - 1; k >= 0; k--) {
    var client = clients[k];
    if (!client.paired && now - client.born > UNPAIRED_LIFE) {
      clients.splice(k, 1);
      try { client.c.disconnect(); } catch (e) { /* gone */ }
    } else if (!client.paired && client.asked && now >= code.pauseUntil) {
      showCode(now);
    }
  }
}

function dropAll() {
  var all = clients;
  clients = [];
  for (var k = 0; k < all.length; k++) {
    try { all[k].c.disconnect(); } catch (e) { /* gone */ }
  }
}

// SHA-256 of an ASCII string, as lowercase hex (FIPS 180-4); only the keys are hashed
function sha256(ascii) {
  var K = [
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2];
  var H = [0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19];
  var bytes = [];
  for (var i = 0; i < ascii.length; i++) bytes.push(ascii.charCodeAt(i) & 0xff);
  var bits = bytes.length * 8;
  bytes.push(0x80);
  while (bytes.length % 64 !== 56) bytes.push(0);
  for (var s = 56; s >= 0; s -= 8) bytes.push(s >= 32 ? 0 : (bits >>> s) & 0xff);
  var rotr = function (x, n) { return (x >>> n) | (x << (32 - n)); };
  var W = [];
  for (var off = 0; off < bytes.length; off += 64) {
    for (var t = 0; t < 64; t++) {
      if (t < 16) {
        W[t] = (bytes[off + 4 * t] << 24) | (bytes[off + 4 * t + 1] << 16) | (bytes[off + 4 * t + 2] << 8) | bytes[off + 4 * t + 3];
      } else {
        var s0 = rotr(W[t - 15], 7) ^ rotr(W[t - 15], 18) ^ (W[t - 15] >>> 3);
        var s1 = rotr(W[t - 2], 17) ^ rotr(W[t - 2], 19) ^ (W[t - 2] >>> 10);
        W[t] = (W[t - 16] + s0 + W[t - 7] + s1) | 0;
      }
    }
    var a = H[0], b = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], h = H[7];
    for (var r = 0; r < 64; r++) {
      var S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      var t1 = (h + S1 + ((e & f) ^ (~e & g)) + K[r] + W[r]) | 0;
      var S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      var t2 = (S0 + ((a & b) ^ (a & c) ^ (b & c))) | 0;
      h = g; g = f; f = e; e = (d + t1) | 0; d = c; c = b; b = a; a = (t1 + t2) | 0;
    }
    H[0] = (H[0] + a) | 0; H[1] = (H[1] + b) | 0; H[2] = (H[2] + c) | 0; H[3] = (H[3] + d) | 0;
    H[4] = (H[4] + e) | 0; H[5] = (H[5] + f) | 0; H[6] = (H[6] + g) | 0; H[7] = (H[7] + h) | 0;
  }
  var hex = '';
  for (var q = 0; q < 8; q++) hex += ('0000000' + (H[q] >>> 0).toString(16)).slice(-8);
  return hex;
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

// the script's cursor track goes to track slot i: its banks then show i's containers
function listTrack(i, tr) {
  if (listed !== i) {
    var was = listed;
    listed = i;
    cursorTrack.selectChannel(tr);
    if (was >= 0) dirtyDevices[was] = true; // back to its top-level list
  }
  dirtyDevices[i] = true;
  host.requestFlush();
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
  if (t === 'list') {
    tr = slot(m.i);
    if (!tr) return;
    listTrack(Number(m.i), tr);
    return;
  }
  if (t === 'open') {
    tr = slot(m.i);
    var d = Number(m.d);
    var dev = tr ? deviceAt(Number(m.i), d) : null;
    if (!dev) return;
    open.i = Number(m.i);
    open.d = d;
    listTrack(open.i, tr);
    cursorDevice.isPinned().set(true);
    cursorDevice.selectDevice(dev);
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
    var bd = tr ? deviceAt(Number(m.i), Number(m.d)) : null;
    if (bd) bd.isEnabled().set(!!m.on);
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
