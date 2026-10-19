#!/usr/bin/env python3
"""Renders the Rack video: the Rack's JACK standalone runs live under Xvfb with the demo loop
going through it, its window is filmed while a script picks presets, opens modules, shows the
modulation and turns a macro, and the result gets the same frame, captions and before/after
panels as the plugin videos (make_videos.py).

Writes assets/video/rack.mp4 and rack.jpg (and <hq dir>/rack.mp4 with --hq).
Usage: make_rack_video.py [--hq DIR] [--build DIR]
Needs Xvfb, jackd2 (dummy driver), xdotool, libjack, ffmpeg, and the Rack built as a JACK
standalone: cmake -S native -B <DIR> -G Ninja -DNULL_JACK=ON -DNULL_VST3=OFF -DNULL_CLAP=OFF
-DNULL_PLUGINS=rack && cmake --build <DIR>   (default DIR: tests/.build/rack-jack, built if missing)
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import numpy as np

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import make_videos as mv  # noqa: E402
from make_videos import FPS, SR  # noqa: E402

ROOT = HERE.parent.parent
UI_W, UI_H = 1440, 1020  # the window, 1.5 x the 960 x 680 design
K = UI_W / 960

# where things are in the 960 x 680 design (see native/plugins/rack/UI.cpp)
PRESET_NAME = (118, 82)
PRESET_ITEM = {  # in the open preset list
    "Drum Bus": (62, 281), "Mastering Chain": (75, 395), "Multiband Distortion": (439, 167),
    "Auto Filter": (419, 243), "Build-Up": (770, 167), "Rhythm Engine": (605, 186),
    "Parallel Drum Smash": (439, 585), "Psychedelic Echo": (608, 167), "Cosmic Space": (425, 566),
}
SLOT = lambda i: (150, 161 + 35 * (i - 1))  # noqa: E731
MOD = (386, 459)
MACRO1 = (47, 515)

INFO = {"name": "Rack", "category": "VST3 · CLAP", "desc": "Eight NULL modules in one plugin, with modulation",
        "sliders": [], "units": [], "mode": "fx"}

# (seconds, kind, caption or label, bypass, actions [(seconds into the segment, action, args)])
SCRIPT = [
    (3.0, "title", "", True, []),
    (4.0, "before", "The loop, untouched", True, [(2.6, "preset", "Drum Bus")]),
    (7.0, "caption", ("CHAIN PRESET", "Drum Bus"), False, [(3.5, "click", SLOT(2))]),
    (2.0, "ab", ("CHAIN PRESET", "Drum Bus"), True, []),
    (2.5, "ab", ("CHAIN PRESET", "Drum Bus"), False, [(1.1, "preset", "Mastering Chain")]),
    (7.0, "caption", ("CHAIN PRESET", "Mastering Chain"), False, [(3.0, "click", SLOT(2)),
                                                                  (5.6, "preset", "Multiband Distortion")]),
    (7.0, "caption", ("BANDS: SPLIT INTO THREE", "Multiband Distortion"), False, [(2.5, "click", SLOT(2)),
                                                                                  (5.6, "preset", "Auto Filter")]),
    (8.0, "caption", ("AN LFO MOVES THE FILTER", "Auto Filter"), False, [(2.5, "click", MOD), (6.0, "click", MOD),
                                                                         (6.6, "preset", "Build-Up")]),
    (9.0, "caption", ("MACRO 1 DRIVES THREE MODULES", "Build-Up"), False, [(1.4, "drag", (MACRO1, -230, 5.0)),
                                                                            (7.6, "preset", "Rhythm Engine")]),
    (7.0, "caption", ("A STEP SEQUENCER MOVES THE FILTER", "Rhythm Engine"), False,
     [(5.6, "preset", "Parallel Drum Smash")]),
    (7.0, "caption", ("LANE B RUNS IN PARALLEL", "Parallel Drum Smash"), False, [(5.6, "preset", "Psychedelic Echo")]),
    (7.0, "caption", ("A FEEDBACK LOOP AROUND FOUR MODULES", "Psychedelic Echo"), False,
     [(5.6, "preset", "Cosmic Space")]),
    (7.0, "caption", ("ROUTING PRESET", "Cosmic Space"), False, []),
    (4.5, "end", "", False, []),
]


def sh(*a, **k):
    return subprocess.run(a, check=True, capture_output=True, text=True, **k).stdout


def at(design):
    return int(design[0] * K), int(design[1] * K)


def run_actions(env, t0, actions, log):
    """Performs [(wall second from t0, action, args)] with xdotool on the Rack window and logs
    the pointer in log as (wall time, x, y, pressed): Xvfb's own pointer is an X, so the video
    draws an arrow there instead."""
    pressed = [0]

    def x(*a):
        subprocess.run(["xdotool", *map(str, a)], env=env, check=True)
        if a[0] == "mousemove":
            log.append((time.time(), a[1], a[2], pressed[0]))
        elif a[0] in ("click", "mousedown", "mouseup"):
            pressed[0] = a[0] == "mousedown"
            if log:
                log.append((time.time(), log[-1][1], log[-1][2], 1 if a[0] != "mouseup" else 0))

    def move(p, dur=0.45):
        x0, y0 = [int(v) for v in re.findall(r"\d+", sh("xdotool", "getmouselocation", env=env))[:2]]
        steps = max(1, int(dur * 60))
        for i in range(1, steps + 1):
            q = 0.5 - 0.5 * np.cos(np.pi * i / steps)
            x("mousemove", int(x0 + (p[0] - x0) * q), int(y0 + (p[1] - y0) * q))
            time.sleep(dur / steps)

    for when, what, arg in sorted(actions, key=lambda a: a[0]):
        while time.time() < t0 + when - (1.4 if what == "preset" else 0.5):
            time.sleep(0.005)
        if what == "click":
            move(at(arg))
            x("click", 1)
        elif what == "preset":
            move(at(PRESET_NAME))
            x("click", 1)
            time.sleep(0.5)
            move(at(PRESET_ITEM[arg]))
            while time.time() < t0 + when:
                time.sleep(0.005)
            x("click", 1)
        elif what == "drag":
            p, dy, dur = arg
            move(at(p))
            x("mousedown", 1)
            px, py = at(p)
            steps = int(dur * 60)
            for i in range(1, steps + 1):
                q = 0.5 - 0.5 * np.cos(np.pi * i / steps)
                x("mousemove", px, int(py + dy * q))
                time.sleep(dur / steps)
            x("mouseup", 1)
        time.sleep(0.05)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hq", type=Path)
    ap.add_argument("--build", type=Path, default=ROOT / "tests/.build/rack-jack")
    a = ap.parse_args()
    exe = a.build / "bin/null_jsfx_rack"
    if not exe.exists():
        sh("cmake", "-S", str(ROOT / "native"), "-B", str(a.build), "-G", "Ninja", "-DNULL_JACK=ON", "-DNULL_VST3=OFF",
           "-DNULL_CLAP=OFF", "-DNULL_PLUGINS=rack")
        sh("cmake", "--build", str(a.build))
    mv.OUT.mkdir(parents=True, exist_ok=True)
    if a.hq:
        a.hq.mkdir(parents=True, exist_ok=True)
    total = sum(s[0] for s in SCRIPT)
    starts = np.cumsum([0] + [s[0] for s in SCRIPT])
    actions = [(starts[i] + w, what, arg) for i, s in enumerate(SCRIPT) for w, what, arg in s[4]]

    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        loop = mv.load_loop()
        loop.astype(np.float32).tofile(tmp / "in.f32")
        jl = tmp / "jack_loop"
        sh("cc", "-O2", str(HERE / "jack_loop.c"), "-ljack", "-o", str(jl))
        disp = f":{90 + os.getpid() % 100}"
        env = dict(os.environ, DISPLAY=disp, JACK_DEFAULT_SERVER=f"video{os.getpid()}")
        procs = []
        try:
            procs.append(subprocess.Popen(["Xvfb", disp, "-screen", "0", "1920x1200x24"], stdout=subprocess.DEVNULL,
                                          stderr=subprocess.DEVNULL))
            time.sleep(1)
            procs.append(subprocess.Popen(["jackd", "-n", env["JACK_DEFAULT_SERVER"], "-d", "dummy", "-r", str(SR), "-p",
                                           "256"], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL))
            time.sleep(1.5)
            rack = subprocess.Popen([str(exe)], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            procs.append(rack)
            for _ in range(50):
                w = subprocess.run(["xdotool", "search", "--pid", str(rack.pid)], env=env, capture_output=True,
                                   text=True).stdout.split()
                if w:
                    break
                time.sleep(0.2)
            win = w[-1]
            subprocess.run(["xdotool", "windowmove", win, "0", "0", "windowsize", win, str(UI_W), str(UI_H)], env=env,
                           check=True)
            subprocess.run(["xdotool", "mousemove", "1300", "560"], env=env, check=True)
            time.sleep(1.5)
            cap = subprocess.Popen(["ffmpeg", "-y", "-f", "x11grab", "-draw_mouse", "0", "-framerate", str(FPS),
                                    "-video_size", f"{UI_W}x{UI_H}", "-i", f"{disp}.0+0,0", "-c:v", "libx264",
                                    "-preset", "ultrafast", "-qp", "0", str(tmp / "cap.mkv")],
                                   env=env, stdin=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            time.sleep(1.5)
            jack = subprocess.Popen([str(jl), str(tmp / "in.f32"), str(tmp / "out.f32"), str(total + 1),
                                     "Rack:audio_in_1", "Rack:audio_in_2", "Rack:audio_out_1", "Rack:audio_out_2"],
                                    env=env, stdout=subprocess.PIPE, text=True)
            ts, p0 = jack.stdout.readline().split()
            t0 = float(ts) - int(p0) / SR  # wall time of the first recorded sample
            log = [(time.time(), 1300, 560, 0)]
            run_actions(env, t0, actions, log)
            jack.wait()
            cap.stdin.write("q")
            cap.stdin.flush()
            err = cap.communicate()[1]
        finally:
            for p in reversed(procs):
                p.terminate()
        w0 = float(re.search(r"start: ([\d.]+)", err).group(1))  # x11grab stamps frames with the wall clock

        wet = np.fromfile(tmp / "out.f32", np.float32).reshape(-1, 2).astype(np.float64)
        n = int(total * SR)
        dry = np.vstack([loop] * (n // len(loop) + 2))
        # the JACK round trip: line the output up with the input (Init passes it through)
        seg = slice(int(3.5 * SR), int(6.5 * SR))
        lags = range(0, 4096)
        lag = max(lags, key=lambda L: float(np.dot(wet[seg.start + L:seg.stop + L, 0], dry[seg, 0])))
        wet = np.vstack([wet[lag:], np.zeros((lag, 2))])[:n]
        dry = dry[:n]
        byp = np.array([s[3] for s in SCRIPT for _ in range(int(round(s[0] * FPS)))], float)
        g = np.repeat(1 - byp, SR // FPS)[:n]
        g = np.concatenate([g, np.full(max(0, n - len(g)), g[-1])])
        k = int(0.025 * SR)
        g = np.convolve(np.concatenate([np.full(k, g[0]), g]), np.ones(k) / k, "valid")[:n]
        t = np.arange(n) / SR
        mix = (dry * (1 - g[:, None]) + wet * g[:, None]) * (np.clip(t, 0, 1) * np.clip((total - t) / 2, 0, 1))[:, None]
        mv.master(mix, tmp / "mix.f32")
        print(f"rack: JACK round trip {lag} samples", flush=True)

        dec = subprocess.Popen(["ffmpeg", "-loglevel", "error", "-i", str(tmp / "cap.mkv"), "-f", "rawvideo",
                                "-pix_fmt", "rgb24", "-"], stdout=subprocess.PIPE)
        state = {"idx": -1, "img": None}

        def ui_at(f):
            want = max(0, int(round((t0 + f / FPS - w0) * FPS)))
            while state["idx"] < want:
                buf = dec.stdout.read(UI_W * UI_H * 3)
                if len(buf) < UI_W * UI_H * 3:
                    break
                state["img"] = np.frombuffer(buf, np.uint8).reshape(UI_H, UI_W, 3)
                state["idx"] += 1
            return pointer(state["img"], log, t0 + f / FPS)

        script = [(d, kind, label, None, byp_, None) for d, kind, label, byp_, _ in SCRIPT]
        st = mv.stage(INFO, UI_W, UI_H)
        mv.film("rack", INFO, script, st, ui_at, dry, wet, None, tmp / "mix.f32", a.hq, rack_title(), rack_end())
        dec.terminate()
    print(f"rack: {total:.1f} s, {(mv.OUT / 'rack.mp4').stat().st_size / 1e6:.2f} MB")


ARROW = [(0, 0), (0, 19), (5, 14.5), (8.5, 22), (12, 20.5), (8.5, 13), (15, 13)]


def pointer(img, log, wall):
    """The frame with an arrow pointer where the log says it was at that wall time."""
    from PIL import Image, ImageDraw
    k = max(0, min(len(log) - 1, int(np.searchsorted([e[0] for e in log], wall, side="right") - 1)))
    _, px, py, down = log[k]
    im = Image.fromarray(img)
    d = ImageDraw.Draw(im)
    if down and wall - log[k][0] < 0.4:
        r = 14 + 30 * (wall - log[k][0])
        d.ellipse([px - r, py - r, px + r, py + r], outline=(220, 220, 220), width=2)
    s = 1.5
    pts = [(px + a * s, py + b * s) for a, b in ARROW]
    d.polygon([(a + 2, b + 3) for a, b in pts], fill=(0, 0, 0))
    d.polygon(pts, fill=(242, 242, 242), outline=(17, 17, 17))
    return np.asarray(im)


def rack_title():
    from PIL import ImageDraw
    im = mv.background()
    d = ImageDraw.Draw(im)
    mv.logo(d, mv.W // 2 - 50, 250, 100)
    mv.ctext(d, 380, "VST3  ·  CLAP", mv.font(False, 28), mv.MUTED)
    mv.ctext(d, 420, "Rack", mv.font(True, 120), mv.TEXT)
    mv.ctext(d, 590, "Eight NULL modules in one plugin, for any DAW", mv.font(False, 40), mv.SILVER)
    mv.ctext(d, 660, "Bands  ·  Mid/Side  ·  LFOs  ·  Envelope  ·  Macros  ·  51 presets", mv.font(False, 30),
             mv.MUTED)
    return im


def rack_end():
    im = mv.end_card({"name": "Rack"})
    return im


if __name__ == "__main__":
    main()
