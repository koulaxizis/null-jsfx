# NULL JSFX Remote for Ableton Live: a control surface (MIDI Remote Script) that lets the
# Android app Remote for DAWs (android/web/daws, tab Ableton) run Live's mixer, transport and
# devices, with the NULL JSFX plugins drawn by their own interface on the phone.
# NULL JSFX • https://nulljsfx.tech • MIT License
#
# Install: copy the folder NullJSFX into Live's User Library, folder "Remote Scripts", then
# Settings > Link, Tempo & MIDI > Control Surface: NullJSFX (Input and Output: None).
#
# The phone connects over TCP, port 8819. The protocol is the one of the Bitwig controller
# script (remote/bitwig/NullJSFX/NullJSFX.control.js, written out at its top), version 1, so
# the phone has one session for both:
#   phone -> Live: JSON objects in UTF-8, each with a 4-byte big-endian length in front.
#   Live -> phone: one JSON object per line ("\n").
# Here the parameter ids are the indices of a device's parameters as strings ("1", "2", ...;
# 0 is Live's "Device On", which the phone sees as the device's bypass), and values are
# normalized 0..1 over each parameter's own min and max.
#
# Pairing (1.3): a phone talks to the script only once it holds a key this computer knows. Until
# then the script answers nothing but {"t": "auth"}, carries out nothing, and shows a 6-digit
# code in Live's status bar; the phone sends that code once with a new random key ({"t": "pair",
# "code", "key"}), and the script keeps a hash of the key in KEYS_FILE (delete the file to forget
# every phone). A code lives 2 minutes; after 5 wrong codes there is a new one and a pause of
# 30 s. Messages per connection are limited, and so are connections. The protocol's pairing
# messages are written out in the Bitwig script.
#
# Live calls update_display() about ten times a second on its main thread. Everything
# happens there: the sockets are non-blocking, the commands are carried out, and the
# document is read and compared with what the phones already have, so only changes go out.
# No listeners: nothing to add or remove when tracks and devices come and go.
#
# Written for the Python of Live 11 (3.7) and newer.
from __future__ import absolute_import

import errno
import hashlib
import hmac
import json
import os
import random
import socket
import struct
import time

import Live

try:
    from ableton.v2.control_surface import ControlSurface
except ImportError:  # older Live
    from _Framework.ControlSurface import ControlSurface

PROTO = 1
VERSION = '1.3'  # 1.1: sends; 1.2: devices inside racks; 1.3: pairing
PORT = 8819
SIZE = 32  # tracks in the window
DEVICES = 16  # top-level devices per track
FLAT = 64  # devices per track with those inside racks, depth first
DEPTH = 4  # racks inside racks shown
SENDS = 8  # sends per track, to the first return tracks
PARAMS = 256  # parameters of the open device
MAX_IN = 1 << 20  # bytes of one message from a phone
MAX_OUT = 8 << 20  # bytes waiting for a phone that does not read
RELISTEN = 20  # update_display calls between tries to open the port
KEYS_FILE = os.path.join(os.path.expanduser('~'), '.nulljsfx', 'live-remote-keys.json')
CODE_LIFE = 120.0  # s a pairing code is valid
TRIES = 5  # wrong codes before a new one and a pause
PAUSE = 30.0  # s
UNPAIRED_LIFE = 180.0  # s a connection may stay without pairing
MAX_CLIENTS = 8
RATE = 400  # messages per second and connection; more are dropped
MAX_KEYS = 16  # phones remembered
_random = random.SystemRandom()

PLUGIN_CLASSES = ('PluginDevice', 'AuPluginDevice')
TRACK_FIELDS = ('vol', 'volText', 'pan', 'panText', 'mute', 'solo', 'arm', 'color', 'sends', 'sendTexts')

_AGAIN = (errno.EAGAIN, errno.EWOULDBLOCK, getattr(errno, 'WSAEWOULDBLOCK', errno.EAGAIN))


def _line(m):
    return json.dumps(m, separators=(',', ':'))


def _get(obj, name, default=None):
    # the master track has no mute, solo or arm, a MIDI track without an instrument no meter
    try:
        return getattr(obj, name)
    except Exception:
        return default


def _unit(v):
    try:
        v = float(v)
    except (TypeError, ValueError):
        return None
    if v != v:
        return None
    return max(0.0, min(1.0, v))


def _norm(p):
    lo, hi = float(p.min), float(p.max)
    return 0.0 if hi == lo else (float(p.value) - lo) / (hi - lo)


def _text(p):
    try:
        return str(p.str_for_value(p.value))
    except Exception:
        return str(p)


def _set_norm(p, v):
    if not _get(p, 'is_enabled', True):
        return
    x = float(p.min) + v * (float(p.max) - float(p.min))
    if _get(p, 'is_quantized', False):
        x = float(round(x))
    p.value = x


class _Client(object):
    def __init__(self, sock):
        self.sock = sock
        self.inbuf = b''
        self.out = bytearray()
        self.fresh = False  # asked for hello: gets everything at the next update
        self.dead = False
        self.paired = False
        self.asked = False  # said hello without a known key: the code stays on screen
        self.born = time.time()
        self.second = 0
        self.count = 0


class Remote(ControlSurface):
    def __init__(self, c_instance):
        ControlSurface.__init__(self, c_instance)
        self._c = c_instance
        self._server = None
        self._port = None
        self._wait = 0
        self._clients = []
        self._from = 0
        self._open = None  # {'i', 'd', 'dev'}: the device whose parameters the phones see
        self._sent = None  # what the phones have, see _snapshot()
        self._keys = self._load_keys()
        self._code = {'value': '', 'until': 0.0, 'tries': 0, 'pause': 0.0, 'shown': 0.0}
        self._listen()

    # --- Live -----------------------------------------------------------------------------

    def _song(self):
        return Live.Application.get_application().get_document()

    def _say(self, text):
        try:
            self._c.log_message('NULL JSFX Remote: ' + text)
        except Exception:
            pass

    def update_display(self):
        ControlSurface.update_display(self)
        try:
            self._update()
        except Exception as e:
            self._say('update: %r' % (e,))

    def disconnect(self):
        for c in self._clients:
            self._drop(c)
        self._clients = []
        if self._server is not None:
            try:
                self._server.close()
            except Exception:
                pass
            self._server = None
        ControlSurface.disconnect(self)

    # --- sockets ----------------------------------------------------------------------------

    def _listen(self):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            s.bind(('', PORT))
            s.listen(4)
            s.setblocking(False)
        except Exception as e:
            self._wait = RELISTEN
            self._say('cannot open TCP %d (%s); trying again' % (PORT, e))
            return
        self._server = s
        self._port = s.getsockname()[1]
        self._say('%s listening on TCP %d' % (VERSION, self._port))
        try:
            self._c.show_message('NULL JSFX Remote: TCP %d' % self._port)
        except Exception:
            pass

    def _accept(self):
        while True:
            try:
                sock, _ = self._server.accept()
            except socket.error as e:
                if e.args and e.args[0] not in _AGAIN:
                    self._say('accept: %s' % (e,))
                return
            if len(self._clients) >= MAX_CLIENTS:
                # full: the oldest connection that never paired makes room, else the new one goes
                waiting = [c for c in self._clients if not c.paired]
                if not waiting:
                    try:
                        sock.close()
                    except Exception:
                        pass
                    continue
                self._drop(waiting[0])
                self._clients.remove(waiting[0])
            sock.setblocking(False)
            try:
                sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            except Exception:
                pass
            self._clients.append(_Client(sock))

    def _read(self, c):
        while not c.dead:
            try:
                data = c.sock.recv(65536)
            except socket.error as e:
                if not (e.args and e.args[0] in _AGAIN):
                    c.dead = True
                return
            if not data:
                c.dead = True
                return
            c.inbuf += data
            while len(c.inbuf) >= 4:
                n = struct.unpack('>I', c.inbuf[:4])[0]
                if n > MAX_IN:
                    c.dead = True
                    return
                if len(c.inbuf) < 4 + n:
                    break
                body = c.inbuf[4:4 + n]
                c.inbuf = c.inbuf[4 + n:]
                second = int(time.time())
                if second != c.second:
                    c.second, c.count = second, 0
                c.count += 1
                if c.count > RATE:
                    continue
                try:
                    m = json.loads(body.decode('utf-8'))
                except ValueError:
                    continue
                if isinstance(m, dict):
                    try:
                        if c.paired:
                            self._handle(c, m)
                        else:
                            self._pairing(c, m)
                    except Exception as e:
                        self._say('%s: %r' % (m.get('t'), e))

    def _write(self, c):
        while c.out and not c.dead:
            try:
                n = c.sock.send(bytes(c.out[:65536]))
            except socket.error as e:
                if not (e.args and e.args[0] in _AGAIN):
                    c.dead = True
                return
            del c.out[:n]
        if len(c.out) > MAX_OUT:
            c.dead = True

    def _drop(self, c):
        try:
            c.sock.close()
        except Exception:
            pass

    def _queue(self, c, lines):
        if lines:
            c.out += ('\n'.join(lines) + '\n').encode('utf-8')

    def _update(self):
        if self._server is None:
            self._wait -= 1
            if self._wait <= 0:
                self._listen()
            return
        self._accept()
        for c in self._clients:
            self._read(c)
        t = time.time()
        for c in self._clients:
            if not c.paired:
                if t - c.born > UNPAIRED_LIFE:
                    c.dead = True
                elif c.asked and t >= self._code['pause']:
                    self._show_code(t)
                self._write(c)
        paired = [c for c in self._clients if c.paired and not c.dead]
        if not paired:
            self._sent = None
        else:
            self._push(paired)
        for c in [c for c in self._clients if c.dead]:
            self._drop(c)
            self._clients.remove(c)

    def _push(self, paired):
        now = self._snapshot()
        diff = self._diff(self._sent, now)
        full = None
        for c in paired:
            if c.fresh:
                c.fresh = False
                if full is None:
                    full = self._diff(None, now)
                self._queue(c, full)
            else:
                self._queue(c, diff)
            self._write(c)
        self._sent = now

    # --- pairing ---------------------------------------------------------------------------

    def _load_keys(self):
        try:
            with open(KEYS_FILE) as f:
                keys = json.load(f).get('keys', [])
            return [k for k in keys if isinstance(k, str) and len(k) == 64][-MAX_KEYS:]
        except Exception:
            return []

    def _save_keys(self):
        try:
            folder = os.path.dirname(KEYS_FILE)
            if not os.path.isdir(folder):
                os.makedirs(folder)
            tmp = KEYS_FILE + '.tmp'
            with open(tmp, 'w') as f:
                json.dump({'keys': self._keys}, f)
            os.replace(tmp, KEYS_FILE)
        except Exception as e:
            self._say('cannot keep the paired phones (%s); they pair again after a restart' % (e,))

    @staticmethod
    def _valid_key(k):
        return isinstance(k, str) and 32 <= len(k) <= 128 and all(ch in '0123456789abcdef' for ch in k)

    @staticmethod
    def _hash(k):
        return hashlib.sha256(k.encode('ascii')).hexdigest()

    def _known(self, k):
        if not self._valid_key(k):
            return False
        h = self._hash(k)
        hit = False
        for x in self._keys:
            hit = hmac.compare_digest(x, h) or hit
        return hit

    def _show_code(self, t):
        code = self._code
        if not code['value'] or t >= code['until']:
            code.update(value='%06d' % _random.randrange(1000000), until=t + CODE_LIFE, tries=0, shown=0.0)
        if t - code['shown'] >= 5.0:
            code['shown'] = t
            try:
                self._c.show_message('NULL JSFX Remote: pairing code %s %s' % (code['value'][:3], code['value'][3:]))
            except Exception:
                pass

    def _welcome(self, c):
        c.paired = True
        c.fresh = True
        self._queue(c, [_line({'t': 'paired'})])

    def _pairing(self, c, m):
        t = time.time()
        code = self._code
        kind = m.get('t')
        if kind == 'hello':
            if self._known(m.get('key')):
                return self._welcome(c)
            c.asked = True
        elif kind == 'pair':
            if t < code['pause']:
                return self._queue(c, [_line({'t': 'auth', 'need': 'wait', 's': int(code['pause'] - t + 0.999)})])
            given = ''.join(ch for ch in str(m.get('code', ''))[:16] if ch.isdigit())
            if code['value'] and t < code['until'] and self._valid_key(m.get('key')) \
                    and hmac.compare_digest(given, code['value']):
                code['value'] = ''
                self._keys = (self._keys + [self._hash(m['key'])])[-MAX_KEYS:]
                self._save_keys()
                try:
                    self._c.show_message('NULL JSFX Remote: phone paired')
                except Exception:
                    pass
                return self._welcome(c)
            code['tries'] += 1
            if code['tries'] >= TRIES:
                code['value'] = ''
                code['pause'] = t + PAUSE
                return self._queue(c, [_line({'t': 'auth', 'need': 'wait', 's': int(PAUSE)})])
            self._show_code(t)
            return self._queue(c, [_line({'t': 'auth', 'need': 'code', 'wrong': True})])
        else:
            return  # nothing else before pairing
        if t < code['pause']:
            return self._queue(c, [_line({'t': 'auth', 'need': 'wait', 's': int(code['pause'] - t + 0.999)})])
        self._show_code(t)
        self._queue(c, [_line({'t': 'auth', 'need': 'code'})])

    # --- the document as the phone sees it -------------------------------------------------------

    def _all_tracks(self):
        song = self._song()
        return list(song.tracks) + list(song.return_tracks) + [song.master_track]

    def _window(self):
        all_tracks = self._all_tracks()
        self._from = max(0, min(self._from, max(0, len(all_tracks) - 1)))
        return all_tracks, all_tracks[self._from:self._from + SIZE]

    def _kind(self, t, returns, master):
        if t == master:
            return 'Master'
        if t in returns:
            return 'Return'
        if _get(t, 'is_foldable', False):
            return 'Group'
        if _get(t, 'has_midi_input', False):
            return 'MIDI'
        return 'Audio'

    def _sends(self, t, count):
        # Live's sends of a track, one per return track; None where it has none (the master)
        # or where the send is off (a return's send to itself)
        params = list(_get(t.mixer_device, 'sends', None) or [])[:count]
        sends, texts = [], []
        for k in range(count):
            p = params[k] if k < len(params) else None
            if p is None or not _get(p, 'is_enabled', True):
                sends.append(None)
                texts.append('')
            else:
                sends.append(round(_norm(p), 6))
                texts.append(_text(p))
        return sends, texts

    def _track(self, i, t, kind, send_count=0):
        mix = t.mixer_device
        vol, pan = mix.volume, mix.panning
        color = _get(t, 'color', 0) or 0
        arm = bool(_get(t, 'arm', False)) if _get(t, 'can_be_armed', False) else False
        sends, send_texts = self._sends(t, send_count)
        return {
            'sends': sends, 'sendTexts': send_texts,
            'i': i, 'name': str(t.name), 'type': kind,
            'vol': round(_norm(vol), 6), 'volText': _text(vol),
            'pan': round(_norm(pan), 6), 'panText': _text(pan),
            'mute': bool(_get(t, 'mute', False)), 'solo': bool(_get(t, 'solo', False)), 'arm': arm,
            'color': '#%06x' % (int(color) & 0xffffff),
        }

    def _flat(self, t):
        # the track's devices depth first, those inside a rack's chains right after the rack:
        # [(device, depth, chain name)]; a device's index here is its id `d` for the phone
        out = []

        def walk(devs, depth, chain):
            for dev in devs:
                if len(out) >= FLAT:
                    return
                out.append((dev, depth, chain))
                if depth + 1 < DEPTH and _get(dev, 'can_have_chains', False):
                    for c in list(_get(dev, 'chains', None) or []):
                        walk(list(c.devices), depth + 1, str(c.name))

        walk(list(t.devices)[:DEVICES], 0, '')
        return out

    def _devices(self, t):
        out = []
        for d, (dev, depth, chain) in enumerate(self._flat(t)):
            params = list(dev.parameters)
            on = bool(params[0].value) if params else True
            m = {'d': d, 'name': str(dev.name), 'on': on, 'plugin': _get(dev, 'class_name', '') in PLUGIN_CLASSES}
            if depth:
                m['depth'] = depth  # inside a rack: indented on the phone, under its chain's name
                m['chain'] = chain
            out.append(m)
        return out

    def _snapshot(self):
        song = self._song()
        all_tracks, window = self._window()
        returns = list(song.return_tracks)
        master = song.master_track
        tracks, devices, meters = [], [], []
        send_names = [str(r.name) for r in returns[:SENDS]]
        for i, t in enumerate(window):
            tracks.append(self._track(i, t, self._kind(t, returns, master), len(send_names)))
            devices.append(self._devices(t))
            level = _get(t, 'output_meter_level', 0.0) if _get(t, 'has_audio_output', True) else 0.0
            meters.append(round(max(0.0, min(1.0, float(level or 0.0))), 3))
        meters += [0.0] * (SIZE - len(meters))
        bt = song.get_current_beats_song_time()
        snap = {
            'total': len(all_tracks), 'from': self._from, 'sendNames': send_names, 'tracks': tracks, 'devices': devices, 'meters': meters,
            'transport': {'play': bool(song.is_playing), 'rec': bool(song.record_mode), 'loop': bool(song.loop)},
            'time': '%03d:%d:%d' % (bt.bars, bt.beats, bt.sub_division),
            'open': None,
        }
        if self._open is not None:
            snap['open'] = self._open_state(window)
        return snap

    def _open_state(self, window):
        o = self._open
        dev = o['dev']
        pos, exists = -1, False
        if o['i'] < len(window):
            listed = [x[0] for x in self._flat(window[o['i']])]
            for k, x in enumerate(listed):
                if x == dev:
                    pos, exists = k, True
        params = []
        if exists:
            for k, p in enumerate(list(dev.parameters)[1:PARAMS + 1]):
                params.append((str(k + 1), str(p.name), round(_norm(p), 6), _text(p)))
        return {
            'device': {'t': 'device', 'i': o['i'], 'd': o['d'], 'name': str(dev.name) if exists else '',
                       'plugin': exists and _get(dev, 'class_name', '') in PLUGIN_CLASSES, 'exists': exists, 'pos': pos},
            'params': params,
        }

    def _diff(self, old, new):
        lines = []
        if old is None:
            lines.append(_line({'t': 'hello', 'proto': PROTO, 'ext': VERSION, 'size': SIZE}))
        if old is None or old['sendNames'] != new['sendNames']:
            lines.append(_line({'t': 'sends', 'list': new['sendNames']}))
        heads = lambda s: [(t['i'], t['name'], t['type']) for t in s['tracks']]
        if old is None or heads(old) != heads(new) or old['total'] != new['total'] or old['from'] != new['from']:
            lines.append(_line({'t': 'tracks', 'total': new['total'], 'from': new['from'], 'list': new['tracks']}))
        else:
            for a, b in zip(old['tracks'], new['tracks']):
                m = dict((f, b[f]) for f in TRACK_FIELDS if a[f] != b[f])
                if 'sends' in m or 'sendTexts' in m:
                    m['sends'], m['sendTexts'] = b['sends'], b['sendTexts']  # the phone takes both together
                if m:
                    m['t'] = 'track'
                    m['i'] = b['i']
                    lines.append(_line(m))
        for i, devs in enumerate(new['devices']):
            if old is None or i >= len(old['devices']) or old['devices'][i] != devs:
                lines.append(_line({'t': 'devices', 'i': i, 'list': devs}))
        if old is None or old['transport'] != new['transport']:
            m = dict(new['transport'])
            m['t'] = 'transport'
            lines.append(_line(m))
        if old is None or old['time'] != new['time']:
            lines.append(_line({'t': 'time', 's': new['time']}))
        if old is not None and old['meters'] != new['meters']:
            lines.append(_line({'t': 'meters', 'v': new['meters']}))
        o, n = (old or {}).get('open'), new['open']
        if n is not None:
            if o is None or o['device'] != n['device']:
                lines.append(_line(n['device']))
            ids = [p[0] for p in n['params']]
            if o is None or [p[0] for p in o['params']] != ids:
                lines.append(_line({'t': 'pids', 'ids': ids}))
                o = None
            before = dict((p[0], p) for p in (o['params'] if o else []))
            for pid, name, v, text in n['params']:
                was = before.get(pid)
                if was is None or was[1] != name:
                    lines.append(_line({'t': 'pname', 'id': pid, 'n': name}))
                if was is None or was[2] != v:
                    lines.append(_line({'t': 'pval', 'id': pid, 'v': v}))
                if was is None or was[3] != text:
                    lines.append(_line({'t': 'pdisp', 'id': pid, 's': text}))
        return lines

    # --- what the phone asks -----------------------------------------------------------------

    def _slot(self, i):
        try:
            i = int(i)
        except (TypeError, ValueError):
            return None
        _, window = self._window()
        return window[i] if 0 <= i < len(window) else None

    def _send(self, tr, k):
        if tr is None or not isinstance(k, int) or not 0 <= k < SENDS:
            return None
        sends = list(_get(tr.mixer_device, 'sends', None) or [])
        return sends[k] if k < len(sends) else None

    def _handle(self, client, m):
        t = m.get('t')
        song = self._song()
        if t == 'hello':
            client.fresh = True
        elif t in ('vol', 'pan'):
            tr, v = self._slot(m.get('i')), _unit(m.get('v'))
            if tr is not None and v is not None:
                mix = tr.mixer_device
                _set_norm(mix.volume if t == 'vol' else mix.panning, v)
        elif t in ('mute', 'solo', 'arm'):
            tr = self._slot(m.get('i'))
            if tr is None or tr == song.master_track:
                return
            if t == 'arm' and not _get(tr, 'can_be_armed', False):
                return
            setattr(tr, t, bool(m.get('on')))
        elif t == 'reset':
            tr = self._slot(m.get('i'))
            p = None
            if tr is not None and m.get('what') in ('vol', 'pan'):
                p = tr.mixer_device.volume if m.get('what') == 'vol' else tr.mixer_device.panning
            elif tr is not None and m.get('what') == 'send':
                p = self._send(tr, m.get('k'))
            if p is not None and _get(p, 'is_enabled', True):
                p.value = p.default_value
        elif t == 'send':
            p, v = self._send(self._slot(m.get('i')), m.get('k')), _unit(m.get('v'))
            if p is not None and v is not None and _get(p, 'is_enabled', True):
                _set_norm(p, v)
        elif t == 'open':
            tr = self._slot(m.get('i'))
            devs = [x[0] for x in self._flat(tr)] if tr is not None else []
            d = m.get('d')
            if isinstance(d, int) and 0 <= d < len(devs):
                self._open = {'i': int(m.get('i')), 'd': d, 'dev': devs[d]}
                if self._sent is not None:
                    self._sent['open'] = None  # the phone waits for the whole list again
        elif t == 'close':
            self._open = None
        elif t == 'param':
            v = _unit(m.get('v'))
            if self._open is None or v is None or not isinstance(m.get('id'), str):
                return
            try:
                k = int(m['id'])
            except ValueError:
                return
            params = list(self._open['dev'].parameters)
            if 1 <= k < len(params):
                _set_norm(params[k], v)
        elif t == 'bypass':
            tr = self._slot(m.get('i'))
            devs = [x[0] for x in self._flat(tr)] if tr is not None else []
            d = m.get('d')
            if isinstance(d, int) and 0 <= d < len(devs):
                params = list(devs[d].parameters)
                if params and _get(params[0], 'is_enabled', True):
                    params[0].value = 1.0 if m.get('on') else 0.0
        elif t == 'transport':
            do = m.get('do')
            if do == 'play':
                song.start_playing()
            elif do == 'stop':
                song.stop_playing()
            elif do == 'rec':
                song.record_mode = not song.record_mode
            elif do == 'loop':
                song.loop = not song.loop
            elif do == 'start':
                song.current_song_time = 0.0
        elif t == 'scroll':
            try:
                self._from = max(0, int(m.get('from') or 0))
            except (TypeError, ValueError):
                pass
