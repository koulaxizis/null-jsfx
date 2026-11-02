# A software FL Studio for the tests of the FL Studio tab: it runs the real MIDI script
# (remote/flstudio/NULL JSFX/device_NULLJSFX.py) against stand-ins for the FL Studio modules it
# imports (device, mixer, plugins, transport, as Image-Line's MIDI scripting documentation
# describes them). fl_host.cjs drives it over stdin and stdout, one JSON object per line:
#   in:  {"op": "project", "p": {...}}   the project (fl_host.cjs builds it)
#        {"op": "init"}                  FL Studio starts the script (OnInit)
#        {"op": "sysex", "hex": "f0..."} a System Exclusive message from the MIDI input (OnSysEx)
#        {"op": "idle"}                  one OnIdle call
#        {"op": "set", "path": [...], "v": x}  a change made in FL Studio itself
#        {"op": "get", "id": n}          the project, answered {"id": n, "project": {...}}
#   out: {"sysex": "f0..."}              device.midiOutSysex
# With --no-json the script's "import json" fails, so its own JSON writer and reader run.
# It checks that the script uses the API as documented; whether FL Studio behaves the same is
# Bible C20. Usage: python3 tests/daws/fl_sim.py <script> [--no-json]
# NULL JSFX - https://nulljsfx.tech - MIT License
import importlib.abc
import importlib.util
import json as _json
import math
import sys
import types

sys.dont_write_bytecode = True  # no __pycache__ next to the script that ships
SCRIPT = sys.argv[1]
TRACKS = 127  # FL Studio 2024: the master, 125 inserts and "Current"
P = {'tracks': [], 'transport': {'play': 0, 'rec': 0, 'pos': 0}, 'calls': []}


def out(obj):
    sys.stdout.write(_json.dumps(obj) + '\n')
    sys.stdout.flush()


def track(i):
    if not 0 <= i < TRACKS:
        raise TypeError('Track index out of range')  # what FL raises for a bad index
    t = P['tracks']
    return t[i] if i < len(t) else None


def field(i, k, default):
    t = track(i)
    return t.get(k, default) if t else default


def setf(i, k, v):
    t = track(i)
    if t is None:
        while len(P['tracks']) <= i:
            P['tracks'].append({})
        t = P['tracks'][i]
    t[k] = v


def slot(i, s):
    if not 0 <= s < 10:
        raise TypeError('Slot index out of range')
    fx = field(i, 'fx', [])
    return fx[s] if s < len(fx) else None


def plugin(i, s):
    p = slot(i, s)
    if p is None:
        raise TypeError('Plugin not found')
    return p


# ---- device ------------------------------------------------------------------------------------
device = types.ModuleType('device')


def midiOutSysex(message):
    b = bytes(message)
    assert b[0] == 0xf0 and b[-1] == 0xf7, 'midiOutSysex takes the whole message, F0 to F7'
    assert all(x < 0x80 for x in b[1:-1]), 'a data byte over 7 bits'
    out({'sysex': b.hex()})


device.midiOutSysex = midiOutSysex
device.isAssigned = lambda: True

# ---- mixer ---------------------------------------------------------------------------------------
mixer = types.ModuleType('mixer')
mixer.trackCount = lambda: TRACKS


def getTrackName(i):
    return field(i, 'name', None) or ('Master' if i == 0 else 'Insert %d' % i)


def getTrackVolume(i, mode=0):
    v = field(i, 'vol', 0.8)
    if mode:  # decibels, FL's fader law (here: a stand-in with 0 dB at 0.8)
        return -100.0 if v <= 0 else 40 * math.log10(v / 0.8)
    return v


def setTrackVolume(i, v, pickupMode=0):
    setf(i, 'vol', float(v))


def muteTrack(i, value=-1):
    setf(i, 'mute', (0 if field(i, 'mute', 0) else 1) if value < 0 else int(value))


def soloTrack(i, value=-1, mode=-1):
    setf(i, 'solo', (0 if field(i, 'solo', 0) else 1) if value < 0 else int(value))


def armTrack(i):
    P['calls'].append('armTrack')
    setf(i, 'arm', 0 if field(i, 'arm', 0) else 1)


def getTrackPeaks(i, mode):
    assert mode in (0, 1, 2)
    return field(i, 'peak', 0.0)


for name, fn in {
    'getTrackName': getTrackName, 'getTrackVolume': getTrackVolume, 'setTrackVolume': setTrackVolume,
    'getTrackPan': lambda i: field(i, 'pan', 0.0),
    'setTrackPan': lambda i, v, pickupMode=0: setf(i, 'pan', float(v)),
    'isTrackMuted': lambda i: field(i, 'mute', 0), 'muteTrack': muteTrack,
    'isTrackSolo': lambda i: field(i, 'solo', 0), 'soloTrack': soloTrack,
    'isTrackArmed': lambda i: field(i, 'arm', 0), 'armTrack': armTrack,
    'getTrackColor': lambda i: field(i, 'color', 0x565656) | -0x1000000,  # FL's colours are signed ARGB
    'getTrackPeaks': getTrackPeaks,
}.items():
    setattr(mixer, name, fn)

# ---- plugins -------------------------------------------------------------------------------------
plugins = types.ModuleType('plugins')
VST_EXTRA = 128 + 16  # a VST's parameter list ends with 128 MIDI CCs and 16 aftertouch


def getParamCount(i, s, useGlobalIndex=False):
    p = plugin(i, s)
    return 4096 + VST_EXTRA if p.get('vst') else len(p['params'])


def getParamName(n, i, s, useGlobalIndex=False):
    p = plugin(i, s)
    if n < len(p['params']):
        return p['params'][n]['name']
    if p.get('vst') and n >= 4096:
        return 'MIDI CC #%d' % (n - 4096) if n < 4096 + 128 else 'Aftertouch %d' % (n - 4096 - 128)
    return ''


def getParamValue(n, i, s, useGlobalIndex=False):
    p = plugin(i, s)
    return p['params'][n]['v'] if n < len(p['params']) else 0.0


def setParamValue(v, n, i, s, pickupMode=0, useGlobalIndex=False):
    p = plugin(i, s)
    assert 0.0 <= v <= 1.0
    p['params'][n]['v'] = v


def getParamValueString(n, i, s, useGlobalIndex=False):
    p = plugin(i, s)
    return '%.1f %%' % (p['params'][n]['v'] * 100) if n < len(p['params']) else ''


for name, fn in {
    'isValid': lambda i, s=-1, useGlobalIndex=False: slot(i, s) is not None,
    'getPluginName': lambda i, s=-1, userName=False, useGlobalIndex=False: plugin(i, s)['name'],
    'getParamCount': getParamCount, 'getParamName': getParamName, 'getParamValue': getParamValue,
    'setParamValue': setParamValue, 'getParamValueString': getParamValueString,
}.items():
    setattr(plugins, name, fn)

# ---- transport -----------------------------------------------------------------------------------
transport = types.ModuleType('transport')
TP = P['transport']


def start():  # play/pause
    P['transport']['play'] = 0 if P['transport']['play'] else 1


def stop():
    P['transport']['play'] = 0
    P['transport']['pos'] = 0


def record():
    P['transport']['rec'] = 0 if P['transport']['rec'] else 1


def setSongPos(pos, mode=-1):
    assert mode in (-1, 0, 1, 2, 3, 4, 5)
    P['transport']['pos'] = pos


def getSongPosHint():
    pos = P['transport']['pos']
    return '%d:%02d:%03d' % (pos // 16 + 1, pos % 16 + 1, 0)


for name, fn in {
    'start': start, 'stop': stop, 'record': record, 'setSongPos': setSongPos, 'getSongPosHint': getSongPosHint,
    'isPlaying': lambda: P['transport']['play'], 'isRecording': lambda: P['transport']['rec'],
}.items():
    setattr(transport, name, fn)

for m in (device, mixer, plugins, transport):
    sys.modules[m.__name__] = m


class NoJson(importlib.abc.MetaPathFinder):
    def find_spec(self, name, path, target=None):
        if name == 'json':
            raise ImportError('no json here')
        return None


_json_dumps = _json.dumps
if '--no-json' in sys.argv:
    del sys.modules['json']
    sys.meta_path.insert(0, NoJson())

spec = importlib.util.spec_from_file_location('device_NULLJSFX', SCRIPT)
script = importlib.util.module_from_spec(spec)
spec.loader.exec_module(script)
if '--no-json' in sys.argv:
    assert script.json is None, 'the script found a json module'


class Event(object):
    def __init__(self, data):
        self.sysex = data
        self.handled = False
        self.status = data[0]


for line in sys.stdin:
    cmd = _json.loads(line)
    op = cmd['op']
    if op == 'project':
        P['tracks'] = cmd['p']['tracks']
        P['transport'].update(cmd['p'].get('transport', {}))
    elif op == 'init':
        script.OnInit()
    elif op == 'sysex':
        e = Event(bytes.fromhex(cmd['hex']))
        script.OnSysEx(e)
        if not e.handled:
            out({'unhandled': cmd['hex']})
    elif op == 'idle':
        script.OnIdle()
    elif op == 'set':
        o = P
        for k in cmd['path'][:-1]:
            o = o[k]
        k = cmd['path'][-1]
        while isinstance(o, list) and len(o) <= k:  # a track beyond the last one
            o.append({})
        o[k] = cmd['v']
    elif op == 'get':
        out({'id': cmd['id'], 'project': P})
    elif op == 'quit':
        break
