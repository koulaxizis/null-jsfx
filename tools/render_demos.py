#!/usr/bin/env python3
"""Renders the short audio demos of the plugins: the same music dry and through each plugin.

Each plugin plays a factory preset (tools/presets/, the first one after Default unless DEMO
below picks another) on a 10 second excerpt of the source, through the real JSFX in ysfx
(tools/demo_render). Written to assets/audio/demos/:
  dry.mp3          the excerpt as it is
  <plugin>.mp3     the excerpt through the plugin, at the same level (no loudness matching:
                   what the plugin does to the level is part of the demo)
  demos.json       {plugin: preset name}; tools/build_catalog.py puts the demos in the catalog
The Hub's plugin page plays them (dry / with the plugin). Analyzers, the noise generator and the
band splitter make no sound of their own worth hearing and get none.

Needs ffmpeg and the ysfx build from tests/run.sh. Run it again after adding a plugin or
changing the source music.
Usage: render_demos.py --source <music.wav> [plugin ...]
"""
import argparse
import json
import math
import struct
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True  # no __pycache__ in tools/
sys.path.insert(0, str(Path(__file__).resolve().parent))
from build_presets import file_sliders, load  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
PLUGINS = ROOT / "DATA/Effects/null_jsfx"
OUT = ROOT / "assets/audio/demos"
BUILD = ROOT / "tests/.build"
RATE = 48000
START, LENGTH, PREROLL = 20.0, 10.0, 2.0  # seconds; the pre-roll fills delays and reverbs
# analyzers, and tools that make no difference worth hearing on this source (a finished, nearly
# mono mix): they wait for breaths, sibilance, pauses or stereo width it does not have
SKIP = {
    "loudness_meter", "spectrum_analyzer", "stereo_scope", "noise_generator", "band_splitter",
    "breath_control", "auto_leveler", "deesser", "dynamic_eq", "gate", "mono_maker", "stereo_width",
    "mid_side_matrix", "de_plosive",
}
# where the first preset is too subtle to hear on a full mix, or not meant for one
DEMO = {
    "auto_pan": "Quarter Note Pan",
    "chorus_one": "Lush Eighties",
    "delay": "Quarter Note Echo",
    "panner": "Hard Left Balance",
    "reverb": "Large Hall",
    "spring_reverb": "Surf Guitar Drench",
    "brighter": "Muffled Rescue",
    "doubler": "Wide Mono Guitar",
    "driver": "Drum Bus Glue",
    "exciter": "Cymbal Forward",
    "grit": "Lo-Fi Drums",
    "multiband_compressor": "Broadcast Dense",
    "pressure": "Room Mic Squash",
    "pumper": "Heavy EDM Pump",
    "saturation": "Drum Crunch",
    "snappy": "Snare Crack",
    "spread": "Wide Pad",
    "volume_shaper": "Hard Dance Pump",
    "wetter": "Big Long Tail",
    "wah": "Slow Sweep",
}
# slider moves during the demo (seconds into the excerpt, slider index from 0, value)
MOVES = {
    "tape_stop": [(3.0, 0, 1), (6.5, 0, 0)],  # stop, then start again
    "spectral_freeze": [(3.0, 0, 1), (7.0, 0, 0)],  # freeze, then let go
}


def renderer():
    exe = BUILD / "demo_render"
    src = ROOT / "tools/demo_render/demo_render.cpp"
    lib = BUILD / "ysfx/build/libysfx.a"
    if not lib.exists():
        sys.exit("render_demos: build ysfx first with tests/run.sh")
    if not exe.exists() or src.stat().st_mtime > exe.stat().st_mtime:
        subprocess.run(["g++", "-O2", "-std=c++17", "-I", str(BUILD / "ysfx/include"), str(src), str(lib),
                        "-lpthread", "-ldl", "-o", str(exe)], check=True)
    return exe


def excerpt(source):
    """The excerpt with its pre-roll, as raw stereo float."""
    return subprocess.run(["ffmpeg", "-v", "error", "-ss", str(START - PREROLL), "-t", str(LENGTH + PREROLL),
                           "-i", str(source), "-ac", "2", "-ar", str(RATE), "-f", "f32le", "-"],
                          check=True, capture_output=True).stdout


def rms(raw):
    n = len(raw) // 4
    vals = struct.unpack(f"<{n}f", raw[: n * 4])
    return math.sqrt(sum(v * v for v in vals) / max(n, 1))


def difference(a, b):
    n = len(a) // 4
    x, y = struct.unpack(f"<{n}f", a[: n * 4]), struct.unpack(f"<{n}f", b[: n * 4])
    return struct.pack(f"<{n}f", *(p - q for p, q in zip(x, y)))


def db(ratio):
    return 20 * math.log10(max(ratio, 1e-9))


def encode(raw, dest):
    """Drops the pre-roll, fades in and out, keeps peaks under 0 dBFS, writes an mp3."""
    fade = f"afade=t=in:d=0.05,afade=t=out:st={LENGTH - 0.6}:d=0.6,alimiter=limit=0.95:level=disabled"
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "f32le", "-ar", str(RATE), "-ac", "2", "-i", "-",
                    "-ss", str(PREROLL), "-af", fade, "-c:a", "libmp3lame", "-b:a", "96k", "-map_metadata", "-1",
                    str(dest)], input=raw, check=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--source", required=True)
    ap.add_argument("plugins", nargs="*")
    args = ap.parse_args()
    exe = renderer()
    dry = excerpt(args.source)
    OUT.mkdir(parents=True, exist_ok=True)
    encode(dry, OUT / "dry.mp3")
    index_file = OUT / "demos.json"
    index = json.loads(index_file.read_text()) if index_file.exists() else {}
    stems = args.plugins or sorted(p.stem for p in PLUGINS.glob("*.jsfx"))
    dry_tail = dry[int(PREROLL * RATE) * 8:]
    for stem in stems:
        if stem in SKIP:
            index.pop(stem, None)
            (OUT / f"{stem}.mp3").unlink(missing_ok=True)
            continue
        presets = load(stem)
        name = DEMO.get(stem) or (presets[1][0] if len(presets) > 1 else "Default")
        values = dict(presets)[name]
        files = file_sliders(stem)
        sliders = [f"{i}={v}" for i, v in enumerate(values) if i not in files]
        sliders += [f"{PREROLL + t}@{i}={v}" for t, i, v in MOVES.get(stem, [])]
        wet = subprocess.run([str(exe), str(PLUGINS / f"{stem}.jsfx"), str(RATE), *sliders],
                             input=dry, check=True, capture_output=True).stdout
        if len(wet) != len(dry):
            sys.exit(f"render_demos: {stem}: rendered {len(wet)} bytes of {len(dry)}")
        encode(wet, OUT / f"{stem}.mp3")
        index[stem] = name
        wet_tail = wet[int(PREROLL * RATE) * 8:]
        # how loud it ends up, and how much it changed (the difference against the dry excerpt)
        level = db(rms(wet_tail) / rms(dry_tail))
        change = db(rms(difference(wet_tail, dry_tail)) / rms(dry_tail))
        print(f"{stem:22s} {name:24s} level {level:+5.1f} dB  change {change:+5.1f} dB")
    index_file.write_text(json.dumps(dict(sorted(index.items())), indent=1) + "\n")


if __name__ == "__main__":
    main()
