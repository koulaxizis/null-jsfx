# name=NULL JSFX Remote for DAWs
# NULL JSFX Remote for FL Studio: a MIDI script that lets the Android app Remote for DAWs (tab
# FL Studio) run FL Studio's mixer, transport and the effects of every mixer track, with the
# NULL JSFX plugins drawn by their own interface on the phone.
# NULL JSFX - https://nulljsfx.tech - MIT License
#
# Install: copy the folder "NULL JSFX" into Documents/Image-Line/FL Studio/Settings/Hardware,
# open a network MIDI session named "NULL Remote" on the computer, then in FL Studio's MIDI
# settings give that input the controller type "NULL JSFX Remote for DAWs (user)" and its
# output the same port number. See ../INSTALL.txt.
#
# FL Studio's scripts speak MIDI only, so the phone reaches this script over network MIDI
# (RTP-MIDI). Messages are the protocol of our Bitwig controller script (version 1,
# remote/bitwig/NullJSFX/NullJSFX.control.js), one JSON object per System Exclusive message:
#     F0 7D 4E 4A <the JSON in ASCII, other characters as \uXXXX> F7
# What differs from Bitwig, because FL Studio's API gives no more: a window of 32 mixer tracks
# (the master, then the inserts up to the last one in use); the 10 effect slots of each track,
# without a bypass per slot (FL has one switch for all slots of a track); no sends; loop is not
# a switch (FL's pattern/song mode is not a loop); every state is read in OnIdle and compared
# with what the phone has, so only changes go out.
#
# Written against Image-Line's MIDI scripting documentation (FL Studio API stubs).

import device
import mixer
import plugins
import transport

try:
    import json
except ImportError:  # FL Studio's Python may come without it: our own, for the few types we use
    json = None

PROTO = 1
VERSION = '1.0'
SIZE = 32  # tracks in the window
SLOTS = 10  # effect slots per mixer track
MAX_PARAMS = 256  # parameters of the open plugin
VST_PARAMS = 4096  # FL lists a VST's parameters first, then 128 CCs and 16 aftertouch
POLL = 3  # OnIdle calls (about 20 ms each) between two looks at the mixer
METERS = 5  # OnIdle calls between two meter messages

# ---- messages ---------------------------------------------------------------------------------


def _quote(s):
    out = ['"']
    for c in s:
        o = ord(c)
        if c == '"' or c == '\\':
            out.append('\\' + c)
        elif 0x20 <= o < 0x7f:
            out.append(c)
        elif o > 0xffff:  # a surrogate pair
            o -= 0x10000
            out.append('\\u%04x\\u%04x' % (0xd800 + (o >> 10), 0xdc00 + (o & 0x3ff)))
        else:
            out.append('\\u%04x' % o)
    out.append('"')
    return ''.join(out)


def encode(v):
    """JSON in ASCII, the same with or without the json module."""
    if v is True:
        return 'true'
    if v is False:
        return 'false'
    if v is None:
        return 'null'
    if isinstance(v, (int, float)):
        if isinstance(v, float):
            return repr(round(v, 6))
        return str(v)
    if isinstance(v, str):
        return _quote(v)
    if isinstance(v, (list, tuple)):
        return '[' + ','.join(encode(x) for x in v) + ']'
    if isinstance(v, dict):
        return '{' + ','.join(_quote(str(k)) + ':' + encode(x) for k, x in v.items()) + '}'
    return _quote(str(v))


def _decode(s):
    """A small JSON reader for what the phone sends (objects, numbers, strings, true/false)."""
    pos = [0]

    def ws():
        while pos[0] < len(s) and s[pos[0]] in ' \t\r\n':
            pos[0] += 1

    def value():
        ws()
        c = s[pos[0]]
        if c == '{':
            pos[0] += 1
            out = {}
            ws()
            if s[pos[0]] == '}':
                pos[0] += 1
                return out
            while True:
                ws()
                k = value()
                ws()
                pos[0] += 1  # :
                out[k] = value()
                ws()
                c = s[pos[0]]
                pos[0] += 1
                if c == '}':
                    return out
        if c == '[':
            pos[0] += 1
            out = []
            ws()
            if s[pos[0]] == ']':
                pos[0] += 1
                return out
            while True:
                out.append(value())
                ws()
                c = s[pos[0]]
                pos[0] += 1
                if c == ']':
                    return out
        if c == '"':
            pos[0] += 1
            out = []
            while s[pos[0]] != '"':
                c = s[pos[0]]
                if c == '\\':
                    e = s[pos[0] + 1]
                    if e == 'u':
                        out.append(chr(int(s[pos[0] + 2:pos[0] + 6], 16)))
                        pos[0] += 6
                        continue
                    out.append({'n': '\n', 't': '\t', 'r': '\r', 'b': '\b', 'f': '\f'}.get(e, e))
                    pos[0] += 2
                    continue
                out.append(c)
                pos[0] += 1
            pos[0] += 1
            return ''.join(out)
        for word, v in (('true', True), ('false', False), ('null', None)):
            if s.startswith(word, pos[0]):
                pos[0] += len(word)
                return v
        start = pos[0]
        while pos[0] < len(s) and s[pos[0]] in '+-0123456789.eE':
            pos[0] += 1
        t = s[start:pos[0]]
        return float(t) if ('.' in t or 'e' in t or 'E' in t) else int(t)

    return value()


def decode(s):
    if json is not None:
        return json.loads(s)
    return _decode(s)


def send(obj):
    body = encode(obj)
    device.midiOutSysex(bytes([0xf0, 0x7d, 0x4e, 0x4a]) + body.encode('ascii') + bytes([0xf7]))


# ---- reading FL Studio ------------------------------------------------------------------------

def _try(fn, default):
    try:
        return fn()
    except Exception:
        return default


def track_count():
    # the master and the inserts, without "Current" (FL's last track)
    n = _try(lambda: mixer.trackCount(), 127)
    return max(1, n - 1)


def used(i):
    if i == 0:
        return True
    name = mixer.getTrackName(i)
    if name and name != 'Insert %d' % i:
        return True
    for s in range(SLOTS):
        if plugins.isValid(i, s):
            return True
    return False


def last_used():
    last = 0
    for i in range(track_count()):
        if used(i):
            last = i
    return last


def color(i):
    c = _try(lambda: mixer.getTrackColor(i), 0) & 0xffffff  # 0x--BBGGRR
    return '#%02x%02x%02x' % (c & 0xff, (c >> 8) & 0xff, (c >> 16) & 0xff)


def vol_text(i):
    db = _try(lambda: mixer.getTrackVolume(i, 1), None)  # mode 1: decibels
    if db is None:
        return ''
    if db <= -100:
        return '-inf dB'
    return '%.1f dB' % db


def track_state(i):
    return {
        'i': None, 'name': mixer.getTrackName(i) or ('Master' if i == 0 else 'Insert %d' % i),
        'type': 'Master' if i == 0 else 'Insert', 'canRec': i != 0,
        'vol': round(mixer.getTrackVolume(i), 6), 'volText': vol_text(i),
        'pan': round((mixer.getTrackPan(i) + 1.0) / 2.0, 6), 'panText': '',
        'mute': bool(mixer.isTrackMuted(i)), 'solo': bool(mixer.isTrackSolo(i)),
        'arm': bool(_try(lambda: mixer.isTrackArmed(i), False)), 'color': color(i),
    }


def device_list(i):
    out = []
    for s in range(SLOTS):
        if plugins.isValid(i, s):
            out.append({'d': s, 'name': plugins.getPluginName(i, s), 'on': True, 'plugin': True})
    return out


def param_ids(i, s):
    n = min(plugins.getParamCount(i, s), VST_PARAMS)
    ids = []
    for p in range(n):
        if plugins.getParamName(p, i, s):
            ids.append(p)
            if len(ids) >= MAX_PARAMS:
                break
    return ids


def param_text(p, i, s):
    return _try(lambda: plugins.getParamValueString(p, i, s), '') or ''


# ---- the session -------------------------------------------------------------------------------

class Remote(object):
    def __init__(self):
        self.reset()

    def reset(self):
        self.phone = False
        self.start = 0  # window: mixer tracks start .. start + SIZE - 1
        self.tracks = {}  # slot -> state the phone has
        self.total = 0
        self.devices = {}  # slot -> list the phone has
        self.watch = None  # {'i', 'd', 'ids', 'vals', 'texts'}
        self.transport = None
        self.time = None
        self.ticks = 0

    def window(self):
        return range(self.start, min(self.start + SIZE, self.total))

    # -- to the phone --
    def hello(self):
        self.phone = True
        self.tracks = {}
        self.devices = {}
        self.transport = None
        self.time = None
        send({'t': 'hello', 'proto': PROTO, 'ext': 'flstudio ' + VERSION, 'size': SIZE})
        send({'t': 'sends', 'list': []})
        self.send_tracks()

    def send_tracks(self):
        self.total = last_used() + 1
        self.start = max(0, min(self.start, self.total - 1))
        listed = []
        self.tracks = {}
        for slot, i in enumerate(self.window()):
            st = track_state(i)
            st['i'] = slot
            self.tracks[slot] = st
            listed.append(dict(st))
        send({'t': 'tracks', 'total': self.total, 'from': self.start, 'list': listed})

    def poll(self):
        if last_used() + 1 != self.total:
            self.send_tracks()
        else:
            for slot, i in enumerate(self.window()):
                st = track_state(i)
                st['i'] = slot
                old = self.tracks.get(slot)
                if old is None:
                    self.send_tracks()
                    break
                diff = dict((k, v) for k, v in st.items() if old.get(k) != v)
                if diff:
                    self.tracks[slot] = st
                    diff['t'] = 'track'
                    diff['i'] = slot
                    send(diff)
        for slot in list(self.devices.keys()):
            i = self.start + slot
            lst = device_list(i) if i < self.total else []
            if lst != self.devices[slot]:
                self.devices[slot] = lst
                send({'t': 'devices', 'i': slot, 'list': lst})
        playing = bool(transport.isPlaying())
        rec = bool(transport.isRecording())
        tr = (playing, rec)
        if tr != self.transport:
            self.transport = tr
            send({'t': 'transport', 'play': playing, 'rec': rec, 'loop': False})
        t = _try(lambda: transport.getSongPosHint(), '')
        if t != self.time:
            self.time = t
            send({'t': 'time', 's': t})
        self.poll_params()

    def poll_params(self):
        w = self.watch
        if not w:
            return
        i, s = self.start + w['i'], w['d']
        if not plugins.isValid(i, s) or plugins.getPluginName(i, s) != w['name']:
            self.open_device(w['i'], w['d'])
            return
        for p in w['ids']:
            v = round(plugins.getParamValue(p, i, s), 6)
            if v != w['vals'].get(p):
                w['vals'][p] = v
                send({'t': 'pval', 'id': str(p), 'v': v})
            txt = param_text(p, i, s)
            if txt != w['texts'].get(p):
                w['texts'][p] = txt
                send({'t': 'pdisp', 'id': str(p), 's': txt})

    def meters(self):
        v = []
        for i in self.window():
            v.append(round(min(1.0, max(0.0, _try(lambda: mixer.getTrackPeaks(i, 2), 0.0))), 3))
        send({'t': 'meters', 'v': v})

    def open_device(self, slot, d):
        i = self.start + slot
        ok = 0 <= d < SLOTS and i < self.total and plugins.isValid(i, d)
        name = plugins.getPluginName(i, d) if ok else ''
        send({'t': 'device', 'i': slot, 'd': d, 'name': name, 'plugin': True, 'exists': bool(ok)})
        if not ok:
            self.watch = None
            return
        ids = param_ids(i, d)
        self.watch = {'i': slot, 'd': d, 'name': name, 'ids': ids, 'vals': {}, 'texts': {}}
        send({'t': 'pids', 'ids': [str(p) for p in ids]})
        for p in ids:
            send({'t': 'pname', 'id': str(p), 'n': plugins.getParamName(p, i, d)})
        self.poll_params()

    # -- from the phone --
    def handle(self, m):
        t = m.get('t')
        slot = m.get('i')
        i = self.start + slot if isinstance(slot, int) else -1
        valid = 0 <= i < self.total
        if t == 'hello':
            self.hello()
        elif not self.phone:
            return
        elif t == 'vol' and valid:
            mixer.setTrackVolume(i, float(m['v']))
        elif t == 'pan' and valid:
            mixer.setTrackPan(i, float(m['v']) * 2.0 - 1.0)
        elif t == 'mute' and valid:
            mixer.muteTrack(i, 1 if m.get('on') else 0)
        elif t == 'solo' and valid:
            mixer.soloTrack(i, 1 if m.get('on') else 0)
        elif t == 'arm' and valid:
            if bool(mixer.isTrackArmed(i)) != bool(m.get('on')):
                mixer.armTrack(i)  # FL's arm only toggles
        elif t == 'reset' and valid:
            if m.get('what') == 'vol':
                mixer.setTrackVolume(i, 0.8)  # FL's default, 0 dB
            elif m.get('what') == 'pan':
                mixer.setTrackPan(i, 0.0)
        elif t == 'scroll':
            self.start = max(0, int(m.get('from', 0)))
            self.devices = {}
            self.watch = None
            self.send_tracks()
        elif t == 'list' and valid:
            lst = device_list(i)
            self.devices[slot] = lst
            send({'t': 'devices', 'i': slot, 'list': lst})
        elif t == 'open' and valid:
            if slot not in self.devices:
                self.devices[slot] = device_list(i)
            self.open_device(slot, int(m.get('d', -1)))
        elif t == 'close':
            self.watch = None
        elif t == 'param':
            w = self.watch
            if w is not None:
                p = int(m.get('id', -1))
                if p in w['ids']:
                    v = min(1.0, max(0.0, float(m['v'])))
                    plugins.setParamValue(v, p, self.start + w['i'], w['d'])
                    w['vals'][p] = round(v, 6)  # the phone has it already
        elif t == 'transport':
            what = m.get('do')
            if what == 'play':
                transport.start()  # play/pause
            elif what == 'stop':
                transport.stop()
            elif what == 'rec':
                transport.record()
            elif what == 'start':
                transport.setSongPos(0)
        # bypass: FL Studio has no per-slot switch in its API; loop: no loop switch either

    def idle(self):
        if not self.phone:
            return
        self.ticks += 1
        if self.ticks % POLL == 0:
            self.poll()
        if self.ticks % METERS == 0:
            self.meters()


remote = Remote()


def OnInit():
    remote.reset()


def OnDeInit():
    remote.reset()


def OnIdle():
    remote.idle()


def OnSysEx(event):
    data = bytes(event.sysex)
    if len(data) < 6 or data[:4] != bytes([0xf0, 0x7d, 0x4e, 0x4a]):
        return
    event.handled = True
    try:
        m = decode(data[4:-1].decode('ascii'))
    except Exception:
        return
    if isinstance(m, dict):
        remote.handle(m)
