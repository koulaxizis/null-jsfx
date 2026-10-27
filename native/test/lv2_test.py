#!/usr/bin/env python3
"""LV2 check (Linux): loads every LV2 bundle of a native build with lilv (liblilv-0.so.0, through
ctypes, so no headers are needed), runs it on noise at 48 kHz and compares the output sample by
sample with its JSFX rendered by tests/.build/render_ref (web/test/render_ref.cpp).

Each plugin is run at its defaults and at two random settings (rounded to each slider's step);
the LV2's control inputs are the JSFX's sliders without the file sliders, in order. Plugins with
an event input (instruments, MIDI effects, tempo-synced effects) get the same few notes in
both, and the LV2 gets the transport render_ref plays (120 BPM, 4/4, from 0). Tolerance 1e-5, as for
the CLAP (native/test/compare.cpp).

Usage: native/test/lv2_test.py <build>/bin [name ...]
"""
import ctypes as C
import os
import random
import re
import subprocess
import sys
import tempfile

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
JSFX = os.path.join(ROOT, "DATA", "Effects", "null_jsfx")
REF = os.path.join(ROOT, "tests", ".build", "render_ref")
SR, BLOCK, FRAMES, TOL = 48000, 256, 48000, 1e-5
# (frame, status, data1, data2): two overlapping notes, then a low one
NOTES = [(1000, 0x90, 60, 100), (12000, 0x90, 67, 90), (20000, 0x80, 60, 0), (30000, 0x80, 67, 0),
         (36000, 0x90, 48, 110), (44000, 0x80, 48, 0)]

LV2 = "http://lv2plug.in/ns/lv2core#"
ATOM = "http://lv2plug.in/ns/ext/atom#"


class Feature(C.Structure):
    _fields_ = [("URI", C.c_char_p), ("data", C.c_void_p)]


MAPFN = C.CFUNCTYPE(C.c_uint32, C.c_void_p, C.c_char_p)


class URIDMap(C.Structure):
    _fields_ = [("handle", C.c_void_p), ("map", MAPFN)]


SCHEDFN = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_uint32, C.c_void_p)


class WorkerSchedule(C.Structure):
    # plugins with state ask for a worker; nothing here changes their state, so it never runs
    _fields_ = [("handle", C.c_void_p), ("schedule_work", SCHEDFN)]


_sched = SCHEDFN(lambda h, size, data: 1)  # LV2_WORKER_ERR_UNKNOWN


class Option(C.Structure):
    _fields_ = [("context", C.c_int), ("subject", C.c_uint32), ("key", C.c_uint32), ("size", C.c_uint32),
                ("type", C.c_uint32), ("value", C.c_void_p)]


class Descriptor(C.Structure):
    _fields_ = [("URI", C.c_char_p), ("instantiate", C.c_void_p),
                ("connect_port", C.CFUNCTYPE(None, C.c_void_p, C.c_uint32, C.c_void_p)),
                ("activate", C.CFUNCTYPE(None, C.c_void_p)),
                ("run", C.CFUNCTYPE(None, C.c_void_p, C.c_uint32)),
                ("deactivate", C.CFUNCTYPE(None, C.c_void_p)),
                ("cleanup", C.CFUNCTYPE(None, C.c_void_p)),
                ("extension_data", C.c_void_p)]


class Instance(C.Structure):
    _fields_ = [("lv2_descriptor", C.POINTER(Descriptor)), ("lv2_handle", C.c_void_p), ("pimpl", C.c_void_p)]


lilv = C.CDLL("liblilv-0.so.0")
for name, res, args in (
        ("lilv_world_new", C.c_void_p, []),
        ("lilv_new_file_uri", C.c_void_p, [C.c_void_p, C.c_char_p, C.c_char_p]),
        ("lilv_new_uri", C.c_void_p, [C.c_void_p, C.c_char_p]),
        ("lilv_world_load_bundle", None, [C.c_void_p, C.c_void_p]),
        ("lilv_world_get_all_plugins", C.c_void_p, [C.c_void_p]),
        ("lilv_plugins_get_by_uri", C.c_void_p, [C.c_void_p, C.c_void_p]),
        ("lilv_plugin_get_num_ports", C.c_uint32, [C.c_void_p]),
        ("lilv_plugin_get_port_by_index", C.c_void_p, [C.c_void_p, C.c_uint32]),
        ("lilv_port_is_a", C.c_bool, [C.c_void_p, C.c_void_p, C.c_void_p]),
        ("lilv_port_get_symbol", C.c_void_p, [C.c_void_p, C.c_void_p]),
        ("lilv_node_as_string", C.c_char_p, [C.c_void_p]),
        ("lilv_plugin_get_port_ranges_float", None, [C.c_void_p, C.c_void_p, C.c_void_p, C.c_void_p]),
        ("lilv_plugin_instantiate", C.POINTER(Instance), [C.c_void_p, C.c_double, C.c_void_p]),
        ("lilv_instance_free", None, [C.c_void_p])):
    f = getattr(lilv, name)
    f.restype, f.argtypes = res, args

_urids = {}


@MAPFN
def _map(_handle, uri):
    return _urids.setdefault(uri.decode(), len(_urids) + 1)


def urid(uri):
    return _map(None, uri.encode())


def jsfx_sliders(path):
    """[(index, default, min, max, step, enum, is_file)] from the JSFX header."""
    out = []
    for line in open(path, encoding="utf-8"):
        m = re.match(r"slider(\d+):(.*)$", line.strip())
        if not m:
            continue
        rest = m.group(2)
        if re.match(r"/|[^<]*:[^<]*:", rest) and "<" not in rest.split(">")[0]:
            out.append((int(m.group(1)), 0, 0, 0, 0, False, True))
            continue
        r = re.match(r"([-\d.e]+)<([-\d.e]+),([-\d.e]+)(?:,([-\d.e]+))?", rest)
        if not r:
            out.append((int(m.group(1)), 0, 0, 0, 0, False, True))
            continue
        d, lo, hi = float(r.group(1)), float(r.group(2)), float(r.group(3))
        step = float(r.group(4)) if r.group(4) else 0.0
        out.append((int(m.group(1)), d, lo, hi, step, "{" in rest, False))
    return out


def render_jsfx(path, nin, x, values, midi):
    with tempfile.TemporaryDirectory() as t:
        fin, fout = os.path.join(t, "in.f64"), os.path.join(t, "out.f64")
        np.concatenate([x[c] for c in range(nin)] + [np.zeros(0)]).astype("<f8").tofile(fin)
        args = [REF, path, str(SR), str(BLOCK), str(FRAMES), fin, fout]
        args += [f"{i}={v!r}" for i, v in values.items()]
        if midi:
            fmid = os.path.join(t, "midi.txt")
            with open(fmid, "w") as f:
                f.writelines(f"{at} {a} {b} {c}\n" for at, a, b, c in NOTES)
            args.append(f"midi={fmid}")
        subprocess.run(args, check=True, capture_output=True)
        y = np.fromfile(fout, "<f8")
    return y.reshape(-1, FRAMES)


def time_position(b, w):
    """Writes a time:Position event at frame 0 (render_ref's transport) into the sequence b at
    word w; returns the next free word."""
    T = "http://lv2plug.in/ns/ext/time#"
    flt = lambda v: C.c_uint32.from_buffer_copy(C.c_float(v)).value
    props = [("speed", "Float", [flt(1.0), 0]), ("beatsPerMinute", "Float", [flt(120.0), 0]),
             ("frame", "Long", [0, 0]), ("bar", "Long", [0, 0]), ("barBeat", "Float", [flt(0.0), 0]),
             ("beatUnit", "Int", [4, 0]), ("beatsPerBar", "Float", [flt(4.0), 0])]
    body = [0, urid(T + "Position")]
    for key, kind, val in props:
        body += [urid(T + key), 0, 8 if kind == "Long" else 4, urid(ATOM + kind)] + val
    b[w], b[w + 1], b[w + 2], b[w + 3] = 0, 0, 4 * len(body), urid(ATOM + "Object")
    for i, v in enumerate(body):
        b[w + 4 + i] = v
    return w + 4 + len(body)


def run_lv2(world, plugin, ports, x, controls):
    feats = []
    umap = URIDMap(None, _map)
    worker = WorkerSchedule(None, _sched)
    i32 = C.c_int32(BLOCK)
    f32 = C.c_float(SR)
    opts = (Option * 4)(
        Option(0, 0, urid("http://lv2plug.in/ns/ext/buf-size#nominalBlockLength"), 4, urid(ATOM + "Int"), C.cast(C.pointer(i32), C.c_void_p)),
        Option(0, 0, urid("http://lv2plug.in/ns/ext/buf-size#maxBlockLength"), 4, urid(ATOM + "Int"), C.cast(C.pointer(i32), C.c_void_p)),
        Option(0, 0, urid("http://lv2plug.in/ns/ext/parameters#sampleRate"), 4, urid(ATOM + "Float"), C.cast(C.pointer(f32), C.c_void_p)),
        Option(0, 0, 0, 0, 0, None))
    for uri, data in (("http://lv2plug.in/ns/ext/urid#map", C.cast(C.pointer(umap), C.c_void_p)),
                      ("http://lv2plug.in/ns/ext/options#options", C.cast(opts, C.c_void_p)),
                      ("http://lv2plug.in/ns/ext/worker#schedule", C.cast(C.pointer(worker), C.c_void_p))):
        feats.append(Feature(uri.encode(), data))
    farr = (C.POINTER(Feature) * (len(feats) + 1))(*[C.pointer(f) for f in feats], None)
    inst = lilv.lilv_plugin_instantiate(plugin, SR, C.cast(farr, C.c_void_p))
    if not inst:
        raise RuntimeError("instantiate failed")
    d, h = inst.contents.lv2_descriptor.contents, inst.contents.lv2_handle
    bufs = {}
    ain = [p for p in ports if p[1] == "ain"]
    aout = [p for p in ports if p[1] == "aout"]
    for idx, kind, sym in ports:
        if kind in ("ain", "aout"):
            bufs[idx] = (C.c_float * BLOCK)()
        elif kind == "cin":
            bufs[idx] = C.c_float(controls[idx])
        elif kind == "cout":
            bufs[idx] = C.c_float(0)
        elif kind == "atom_in":
            bufs[idx] = (C.c_uint32 * 1024)(8, urid(ATOM + "Sequence"))  # empty until the first block
        elif kind == "atom_out":
            b = (C.c_uint32 * 2048)()
            bufs[idx] = b
        d.connect_port(h, idx, C.cast(C.pointer(bufs[idx]), C.c_void_p) if kind in ("cin", "cout") else C.cast(bufs[idx], C.c_void_p))
    # the host's settings before the start, as the CLAP test sets them: a run of 0 frames
    # reads the control ports, then a new activate starts from them (no smoothing from the
    # defaults)
    d.activate(h)
    d.run(h, 0)
    d.deactivate(h)
    d.activate(h)
    y = np.zeros((len(aout), FRAMES))
    for o in range(0, FRAMES, BLOCK):
        n = min(BLOCK, FRAMES - o)
        for c, (idx, _, _) in enumerate(ain):
            blk = np.ascontiguousarray(x[c][o:o + n], dtype=np.float32)  # kept alive for the copy
            C.memmove(bufs[idx], blk.ctypes.data, 4 * n)
        for idx, kind, _ in ports:
            if kind == "atom_out":
                bufs[idx][0], bufs[idx][1] = 8184, urid(ATOM + "Chunk")
            elif kind == "atom_in":
                # LV2_Atom_Sequence: atom header, body (unit, pad), then the block's events
                # (int64 frame, atom header, 3 MIDI bytes padded to 8)
                b, words = bufs[idx], 4
                if o == 0:
                    words = time_position(b, words)
                for at, st, d1, d2 in NOTES:
                    if o <= at < o + n:
                        b[words], b[words + 1] = at - o, 0
                        b[words + 2], b[words + 3] = 3, urid("http://lv2plug.in/ns/ext/midi#MidiEvent")
                        b[words + 4], b[words + 5] = st | d1 << 8 | d2 << 16, 0
                        words += 6
                b[0], b[1], b[2], b[3] = 4 * (words - 2), urid(ATOM + "Sequence"), 0, 0
        d.run(h, n)
        for c, (idx, _, _) in enumerate(aout):
            y[c, o:o + n] = np.frombuffer(bufs[idx], dtype=np.float32, count=n)
    d.deactivate(h)
    lilv.lilv_instance_free(inst)
    return y


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    bindir = os.path.abspath(sys.argv[1])
    only = set(sys.argv[2:])
    world = lilv.lilv_world_new()
    cls = {k: lilv.lilv_new_uri(world, v.encode()) for k, v in (
        ("audio", LV2 + "AudioPort"), ("control", LV2 + "ControlPort"), ("input", LV2 + "InputPort"),
        ("output", LV2 + "OutputPort"), ("atom", ATOM + "AtomPort"))}
    rng = random.Random(7)
    failed = total = 0
    for bundle in sorted(os.listdir(bindir)):
        m = re.match(r"null_jsfx_(.+)\.lv2$", bundle)
        if not m or (only and m.group(1) not in only):
            continue
        name = m.group(1)
        path = os.path.join(JSFX, name + ".jsfx")
        if not os.path.exists(path):
            continue  # the Rack has no JSFX
        total += 1
        lilv.lilv_world_load_bundle(world, lilv.lilv_new_file_uri(world, None, (os.path.join(bindir, bundle) + "/").encode()))
        plugin = lilv.lilv_plugins_get_by_uri(lilv.lilv_world_get_all_plugins(world),
                                              lilv.lilv_new_uri(world, f"https://nulljsfx.tech/plugins/{name}".encode()))
        if not plugin:
            # the URI is the plugin's identity: read it from its DistrhoPluginInfo.h
            info = open(os.path.join(ROOT, "native", "plugins", name, "DistrhoPluginInfo.h")).read()
            uri = re.search(r'DISTRHO_PLUGIN_URI\s+"([^"]+)"', info).group(1)
            plugin = lilv.lilv_plugins_get_by_uri(lilv.lilv_world_get_all_plugins(world), lilv.lilv_new_uri(world, uri.encode()))
        if not plugin:
            print(f"FAIL {name}: not found in its bundle")
            failed += 1
            continue
        np_ = lilv.lilv_plugin_get_num_ports(plugin)
        mins, maxs, defs = (C.c_float * np_)(), (C.c_float * np_)(), (C.c_float * np_)()
        lilv.lilv_plugin_get_port_ranges_float(plugin, mins, maxs, defs)
        ports = []
        for i in range(np_):
            p = lilv.lilv_plugin_get_port_by_index(plugin, i)
            isin = lilv.lilv_port_is_a(plugin, p, cls["input"])
            sym = lilv.lilv_node_as_string(lilv.lilv_port_get_symbol(plugin, p)).decode()
            if lilv.lilv_port_is_a(plugin, p, cls["audio"]):
                kind = "ain" if isin else "aout"
            elif lilv.lilv_port_is_a(plugin, p, cls["control"]):
                kind = "cin" if isin else "cout"
            else:
                kind = "atom_in" if isin else "atom_out"
            ports.append((i, kind, sym))
        sl = [s for s in jsfx_sliders(path) if not s[6]]
        cin = [p for p in ports if p[1] == "cin"]
        nin = len([p for p in ports if p[1] == "ain"])
        midi = any(p[1] == "atom_in" for p in ports)
        if len(cin) != len(sl):
            print(f"FAIL {name}: {len(cin)} LV2 control inputs, {len(sl)} JSFX sliders")
            failed += 1
            continue
        # the LV2 gets float samples: the JSFX gets the same values (a bit crusher's rounding
        # would otherwise flip on the difference)
        x = np.random.default_rng(1).uniform(-0.5, 0.5, (max(nin, 1), FRAMES)).astype(np.float32).astype(np.float64)
        worst = 0.0
        for k in range(3):
            vals = {}
            for s in sl:
                v = s[1]
                if k:
                    v = rng.uniform(s[2], s[3])
                    if s[4] > 0:
                        v = s[2] + round((v - s[2]) / s[4]) * s[4]
                    v = min(max(v, s[2]), s[3])
                vals[s[0]] = v
            controls = {p[0]: vals[s[0]] for p, s in zip(cin, sl)}
            ref = render_jsfx(path, nin, x, vals, midi)
            out = run_lv2(world, plugin, ports, x, controls)
            n = min(len(ref), len(out))
            worst = max(worst, float(np.max(np.abs(ref[:n] - out[:n]))) if n else 0.0)
        ok = worst <= TOL
        failed += not ok
        print(f"{'ok  ' if ok else 'FAIL'} {name}: max |diff| {worst:.3g}")
    print(f"\n{failed} of {total} LV2 plugins failed")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
