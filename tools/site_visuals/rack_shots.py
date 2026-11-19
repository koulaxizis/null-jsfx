#!/usr/bin/env python3
"""Still screenshots of the Rack for the site's /rack/ page: the Rack's JACK standalone runs under
Xvfb with a dummy JACK server at 2x its 960 x 680 design, a preset is picked from its preset
list with xdotool (the positions of tools/video/make_rack_video.py), optionally a slot or the
modulation view is opened, and the window is captured.

Writes <out dir>/rack-<name>.png (1920 x 1360), and with --clips the short silent loops of the
page, <out dir>/rack-<name>.mp4 (1280 x 906, 30 fps, H.264, no audio), filmed with ffmpeg x11grab
while the Rack moves by itself (LFOs, sequencer) or a macro is turned.
Usage: rack_shots.py <out dir> [--build DIR] [--clips]
Needs Xvfb, jackd2 (dummy driver), xdotool, ImageMagick (import), and the Rack built as a JACK
standalone (built into DIR if missing, default tests/.build/rack-jack).
"""
import argparse
import os
import random
import subprocess
import sys
import time
from pathlib import Path

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools/video"))
from make_rack_video import MACRO1, MOD, PRESET_ITEM, PRESET_NAME, SLOT  # noqa: E402

K = 2
UI_W, UI_H = 960 * K, 680 * K
# (file name, preset, then clicks in the 960 x 680 design)
SHOTS = [
    ("mastering", "Mastering Chain", []),
    ("bands", "Multiband Distortion", [SLOT(2)]),
    ("lfo", "Auto Filter", []),
    ("macros", "Build-Up", [MOD]),
    ("sequencer", "Rhythm Engine", []),
    ("lanes", "Parallel Drum Smash", [SLOT(3)]),
    ("space", "Cosmic Space", [SLOT(3)]),
]

# (file name, preset, clicks before filming, seconds, drags while filming [(start s, point, dy px, s)])
CLIPS = [
    ("macro", "Build-Up", [MOD], 6.0, [(0.5, MACRO1, -280, 2.3), (3.3, MACRO1, 280, 2.3)]),
    ("lfo", "Auto Filter", [SLOT(1)], 5.0, []),
    ("sequencer", "Rhythm Engine", [], 5.0, []),
]


def at(p):
    return str(int(p[0] * K)), str(int(p[1] * K))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--build", default=str(ROOT / "tests/.build/rack-jack"))
    ap.add_argument("--clips", action="store_true")
    a = ap.parse_args()
    out, build = Path(a.out), Path(a.build)
    out.mkdir(parents=True, exist_ok=True)
    exe = build / "bin/null_jsfx_rack"
    if not exe.exists():
        subprocess.run(["cmake", "-S", str(ROOT / "native"), "-B", str(build), "-G", "Ninja", "-DNULL_JACK=ON",
                        "-DNULL_VST3=OFF", "-DNULL_CLAP=OFF", "-DNULL_PLUGINS=rack"], check=True, stdout=subprocess.DEVNULL)
        subprocess.run(["cmake", "--build", str(build)], check=True, stdout=subprocess.DEVNULL)
    disp = f":{random.randint(120, 220)}"
    server = f"shots{os.getpid()}"
    env = {**os.environ, "DISPLAY": disp, "JACK_DEFAULT_SERVER": server}
    procs = [subprocess.Popen(["Xvfb", disp, "-screen", "0", f"{UI_W + 64}x{UI_H + 64}x24"], stderr=subprocess.DEVNULL)]
    try:
        time.sleep(1)
        procs.append(subprocess.Popen(["jackd", "-n", server, "-d", "dummy", "-r", "48000"], env=env,
                                      stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL))
        time.sleep(1)
        x = lambda *c: subprocess.run(["xdotool", *c], env=env, check=True)  # noqa: E731
        jobs = [(n, p, c, None, None) for n, p, c in SHOTS]
        if a.clips:
            jobs += [(n, p, c, sec, drags) for n, p, c, sec, drags in CLIPS]
        for name, preset, clicks, seconds, drags in jobs:
            # a fresh Rack for each shot, so every view starts from the same state
            rack = subprocess.Popen([str(exe)], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            try:
                win = ""
                for _ in range(40):
                    r = subprocess.run(["xdotool", "search", "--pid", str(rack.pid)], env=env, capture_output=True, text=True)
                    win = r.stdout.split()[-1] if r.stdout.split() else ""
                    if win:
                        break
                    time.sleep(0.3)
                x("windowmove", win, "0", "0", "windowsize", win, str(UI_W), str(UI_H))
                time.sleep(1.2)
                x("mousemove", *at(PRESET_NAME), "click", "1")
                time.sleep(0.6)
                x("mousemove", *at(PRESET_ITEM[preset]), "click", "1")
                time.sleep(0.8)
                for c in clicks:
                    x("mousemove", *at(c), "click", "1")
                    time.sleep(0.6)
                x("mousemove", str(UI_W + 40), str(UI_H + 40))  # the pointer off the window
                time.sleep(1.0)
                if seconds:
                    film(env, disp, out / f"rack-{name}.mp4", seconds, drags, x)
                    print(f"rack-{name}.mp4 ({preset})")
                    continue
                f = out / f"rack-{name}.png"
                subprocess.run(["import", "-window", "root", "-crop", f"{UI_W}x{UI_H}+0+0", "+repage", str(f)],
                               env=env, check=True)
                print(f"{f.name} ({preset})")
            finally:
                rack.terminate()
                rack.wait()
    finally:
        for p in reversed(procs):
            p.terminate()


def film(env, disp, path, seconds, drags, x):
    """Films the window for seconds while the drags run, then encodes the web copy."""
    raw = path.with_suffix(".mkv")
    rec = subprocess.Popen(["ffmpeg", "-y", "-loglevel", "error", "-f", "x11grab", "-draw_mouse", "0",
                            "-video_size", f"{UI_W}x{UI_H}", "-framerate", "30", "-i", f"{disp}+0,0",
                            "-t", f"{seconds}", "-c:v", "libx264rgb", "-preset", "ultrafast", "-qp", "0", str(raw)], env=env)
    t0 = time.time()
    for start, p, dy, dur in drags:
        while time.time() < t0 + start:
            time.sleep(0.01)
        px, py = (int(v) for v in at(p))
        x("mousemove", str(px), str(py), "mousedown", "1")
        steps = int(dur * 40)
        for i in range(1, steps + 1):
            x("mousemove", str(px), str(int(py + dy * i / steps)))
            time.sleep(dur / steps)
        x("mouseup", "1")
        x("mousemove", str(UI_W + 40), str(UI_H + 40))
    rec.wait()
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(raw), "-vf", "scale=1280:-2:flags=lanczos,format=yuv420p",
                    "-c:v", "libx264", "-preset", "slow", "-crf", "27", "-an", "-movflags", "+faststart", str(path)], check=True)
    raw.unlink()


if __name__ == "__main__":
    main()
