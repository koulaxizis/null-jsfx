"""A software Ableton Live for the tests of the Ableton tab of Remote for DAWs.

It runs the real control surface (remote/ableton/NullJSFX) against a stand-in for the parts
of Live's Python API the script uses (Live.Application, Song, Track, MixerDevice, Device,
DeviceParameter, as the Live Object Model names them) and calls update_display() on one
thread, the way Live does. The script's TCP server is real.

Usage: python3 -B tests/daws/ableton_live.py PORT < commands
  The document comes as the first line of stdin (JSON, see ableton_host.cjs); every line after
  it is a command {"id", "op": "get" | "set" | "call", ...}, answered on stdout with the same id.
  The first line on stdout is {"port": N}. The script's log goes to stderr.
NULL JSFX • https://nulljsfx.tech • MIT License
"""
import json
import math
import os
import queue
import sys
import threading
import time
import types

HERE = os.path.dirname(os.path.abspath(__file__))
SCRIPTS = os.path.join(HERE, '..', '..', 'remote', 'ableton')
UNITY = 0.85  # Live's volume position for 0 dB
TICK = 0.05  # s between update_display calls (Live: about 0.1)


def vol_text(v):
    if v <= 0:
        return '-inf dB'
    return '%.1f dB' % (40 * math.log10(v / UNITY))


def pan_text(v):
    p = int(round(v * 50))
    return 'C' if p == 0 else '%d%s' % (abs(p), 'L' if p < 0 else 'R')


class Param(object):
    def __init__(self, name, value, lo=0.0, hi=1.0, default=None, text=None, quantized=False, shown=None):
        self.name = name
        self.value = float(value)
        self.min = float(lo)
        self.max = float(hi)
        self.default_value = float(value if default is None else default)
        self.is_quantized = quantized
        self.is_enabled = True
        self._text = text
        self._shown = shown  # (lo, hi) of the plugin's own range, for the value text

    def str_for_value(self, v):
        if self._text:
            return self._text(v)
        if self._shown:
            x = self._shown[0] + v * (self._shown[1] - self._shown[0])
            return '%.0f' % x if abs(x) >= 100 else '%.2f' % x
        return '%.2f' % v

    def __str__(self):
        return self.str_for_value(self.value)

    def dump(self):
        return {'name': self.name, 'value': self.value, 'min': self.min, 'max': self.max}


class Device(object):
    def __init__(self, d):
        self.name = d['name']
        self.class_name = d.get('class_name', 'PluginDevice')
        self.parameters = [Param('Device On', 1.0 if d.get('on', True) else 0.0, quantized=True)]
        for p in d['params']:
            self.parameters.append(Param(p['name'], p['value'], p.get('min', 0), p.get('max', 1), shown=p.get('shown')))

    def dump(self):
        return {'name': self.name, 'on': self.parameters[0].value == 1.0, 'parameters': [p.dump() for p in self.parameters]}


def send_text(v):
    # Live's sends: 0..1, 0 dB at 1.0 (its own law; the test only needs a text that moves)
    if v <= 0:
        return '-inf dB'
    return '%.1f dB' % (40 * math.log10(v))


class Mixer(object):
    def __init__(self, vol, pan, sends=()):
        self.volume = Param('Track Volume', vol, 0.0, 1.0, UNITY, vol_text)
        self.panning = Param('Track Panning', pan, -1.0, 1.0, 0.0, pan_text)
        self.sends = [Param('%s-Send' % chr(65 + k), v, 0.0, 1.0, 0.0, send_text) for k, v in enumerate(sends)]


class Track(object):
    def __init__(self, d, kind, returns=0):
        self._kind = kind
        self.name = d['name']
        self.color = d.get('color', 0)
        # one send per return track, none on the master; a return's send to itself is off
        sends = list(d.get('sends', [])) + [0.0] * returns
        self.mixer_device = Mixer(d.get('vol', UNITY), d.get('pan', 0.0), sends[:returns])
        self.devices = [Device(x) for x in d.get('devices', [])]
        self._mute = d.get('mute', False)
        self._solo = d.get('solo', False)
        self._arm = d.get('arm', False)
        self.is_foldable = kind == 'group'
        self.has_midi_input = kind == 'midi'
        self.has_audio_output = True
        self.output_meter_level = 0.0

    def _no(self, what):
        if self._kind == 'master':
            raise RuntimeError('Master track has no %s' % what)

    @property
    def can_be_armed(self):
        return self._kind in ('audio', 'midi')

    @property
    def mute(self):
        self._no('mute')
        return self._mute

    @mute.setter
    def mute(self, v):
        self._no('mute')
        self._mute = bool(v)

    @property
    def solo(self):
        self._no('solo')
        return self._solo

    @solo.setter
    def solo(self, v):
        self._no('solo')
        self._solo = bool(v)

    @property
    def arm(self):
        if not self.can_be_armed:
            raise RuntimeError('Track cannot be armed')
        return self._arm

    @arm.setter
    def arm(self, v):
        if not self.can_be_armed:
            raise RuntimeError('Track cannot be armed')
        self._arm = bool(v)

    def dump(self):
        return {
            'name': self.name, 'vol': self.mixer_device.volume.value, 'pan': self.mixer_device.panning.value,
            'sends': [p.value for p in self.mixer_device.sends],
            'mute': self._mute, 'solo': self._solo, 'arm': self._arm,
            'devices': [d.dump() for d in self.devices],
        }


class BeatTime(object):
    def __init__(self, beats):
        self.bars = int(beats // 4) + 1
        self.beats = int(beats % 4) + 1
        self.sub_division = int((beats % 1) * 4) + 1
        self.ticks = 1


class Song(object):
    def __init__(self, doc):
        n = len(doc.get('returns', []))
        self.tracks = [Track(t, t.get('kind', 'audio'), n) for t in doc['tracks']]
        self.return_tracks = [Track(t, 'return', n) for t in doc.get('returns', [])]
        for k, r in enumerate(self.return_tracks):
            r.mixer_device.sends[k].is_enabled = False
        self.master_track = Track(doc['master'], 'master')
        self.is_playing = False
        self.record_mode = False
        self.loop = False
        self.current_song_time = 0.0
        self.log = []

    def start_playing(self):
        self.is_playing = True
        self.log.append('play')

    def stop_playing(self):
        self.is_playing = False
        self.log.append('stop')

    def get_current_beats_song_time(self):
        return BeatTime(self.current_song_time)

    def dump(self):
        return {
            'tracks': [t.dump() for t in self.tracks], 'returns': [t.dump() for t in self.return_tracks],
            'master': self.master_track.dump(), 'play': self.is_playing, 'rec': self.record_mode,
            'loop': self.loop, 'time': self.current_song_time, 'log': self.log,
        }


def install_live(song):
    """The modules a Remote Script imports: Live, and the older _Framework (no ableton.v2 here)."""
    app = types.SimpleNamespace(get_document=lambda: song)
    live = types.ModuleType('Live')
    live.Application = types.SimpleNamespace(get_application=lambda: app)
    sys.modules['Live'] = live

    class ControlSurface(object):
        def __init__(self, c_instance):
            self.c_instance = c_instance
            self.updates = 0

        def update_display(self):
            self.updates += 1

        def disconnect(self):
            pass

    fw = types.ModuleType('_Framework')
    cs = types.ModuleType('_Framework.ControlSurface')
    cs.ControlSurface = ControlSurface
    fw.ControlSurface = cs
    sys.modules['_Framework'] = fw
    sys.modules['_Framework.ControlSurface'] = cs


class CInstance(object):
    def log_message(self, s):
        sys.stderr.write(s + '\n')
        sys.stderr.flush()

    def show_message(self, s):
        self.log_message('[status] ' + s)


def walk(song, path):
    obj = song
    for k in path[:-1]:
        obj = obj[k] if isinstance(k, int) else getattr(obj, k)
    return obj, path[-1]


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8819
    doc = json.loads(sys.stdin.readline())
    song = Song(doc)
    install_live(song)
    sys.path.insert(0, SCRIPTS)
    import NullJSFX
    import NullJSFX.remote as remote
    remote.PORT = port
    surface = NullJSFX.create_instance(CInstance())
    print(json.dumps({'port': surface._port}), flush=True)

    commands = queue.Queue()

    def reader():
        for line in sys.stdin:
            commands.put(json.loads(line))
        commands.put(None)

    threading.Thread(target=reader, daemon=True).start()
    phase = 0.0
    while True:
        # everything on this one thread, as in Live
        while not commands.empty():
            c = commands.get()
            if c is None:
                surface.disconnect()
                return
            reply = {'id': c['id']}
            try:
                if c['op'] == 'get':
                    reply['v'] = song.dump()
                elif c['op'] == 'set':
                    obj, k = walk(song, c['path'])
                    setattr(obj, k, c['value'])
                elif c['op'] == 'call':
                    obj, k = walk(song, c['path'])
                    getattr(obj, k)()
                elif c['op'] == 'remove':
                    obj, k = walk(song, c['path'])
                    del obj[k]
            except Exception as e:
                reply['error'] = repr(e)
            print(json.dumps(reply), flush=True)
        phase += 0.2
        for n, t in enumerate(song.tracks + song.return_tracks + [song.master_track]):
            t.output_meter_level = 0.5 + 0.3 * math.sin(phase + n)
        if song.is_playing:
            song.current_song_time += 0.25
        surface.update_display()
        time.sleep(TICK)


if __name__ == '__main__':
    main()
