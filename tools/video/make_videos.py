#!/usr/bin/env python3
"""Renders the presentation video of every plugin from its GFX version and the demo loop.

Each video plays the first 32 s (16 bars) of assets/audio/null-jsfx-loop.flac through the real plugin (the GFX JSFX, run by
gfx_video with ysfx, the same DSP as every format) and films its own interface live, next to
a spectrum and a level history of the signal before (grey) and after (silver). The script
is: title, the loop bypassed, up to three factory presets with a quick before/after, a sweep
of the main control, end card. tools/video/videos.json can change a plugin's script:
  "presets": [names]          which factory presets (default: first, middle and last)
  "sweep": [slider, from, to] the control to turn (null: none), "sweep_base": preset to turn it on
  "input": hum|noise|clicks|plosives|sidechain  what goes in (default: the loop)
  "pulse": [slider, every, on] switch a 0/1 slider on for `on` seconds every `every` seconds
  "midi": pattern             the MIDI an instrument or a MIDI effect gets (see PATTERNS; null:
                              none, for an instrument that plays its own groove with the host)
  "play": [instrument, preset] a MIDI effect: the NULL instrument that plays its notes
  "input_label": text         the caption of what goes in (default: the loop, or the MIDI pattern)

Instruments (category Instruments) have no input: the video plays a MIDI pattern into them
(or lets their groove run with the host), shows their output and a roll of the notes played,
and skips the before/after. MIDI effects get the pattern too; an instrument plays what goes
in (before) and what comes out (after), and a roll shows both sets of notes.

Two files per plugin:
  assets/video/<plugin>.mp4      1280x720 H.264 for the site and the Hub (small, faststart)
  assets/video/<plugin>.jpg      its poster frame
  <hq dir>/<plugin>.mp4          1920x1080 at higher quality (Ko-fi posts), with --hq <dir>

Usage: make_videos.py [--hq DIR] [--jobs N] [plugin ...]     (default: every GFX plugin)
Needs ffmpeg, numpy and Pillow; builds gfx_video with tools/video/build.sh.
"""
import argparse
import json
import math
import re
import subprocess
import sys
import tempfile
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(HERE.parent))
from build_chains import parse_sliders  # noqa: E402
import build_presets  # noqa: E402

PLUGINS = ROOT / "DATA/Effects/null_jsfx"
GFX = PLUGINS / "gfx"
OUT = ROOT / "assets/video"
CONTENT = json.loads((HERE.parent / "site_content.json").read_text(encoding="utf-8"))
CONFIG = json.loads((HERE / "videos.json").read_text(encoding="utf-8"))
FONT_B = str(ROOT / "native/common/fonts/NotoSans-Bold.ttf")
FONT_R = str(ROOT / "native/common/fonts/NotoSans-Regular.ttf")

SR, FPS, W, H = 48000, 30, 1920, 1080
BG = (27, 26, 24)
PANEL = (33, 32, 30)
BORDER = (58, 57, 54)
TEXT = (230, 230, 228)
SILVER = (200, 200, 204)
MUTED = (150, 149, 146)
DIM = (96, 95, 92)
SR_HZ = (30.0, 20000.0)

_fonts = {}


def font(bold, size):
    k = (bold, size)
    if k not in _fonts:
        _fonts[k] = ImageFont.truetype(FONT_B if bold else FONT_R, size)
    return _fonts[k]


# ---------------------------------------------------------------- input signals

def load_loop():
    raw = subprocess.run(["ffmpeg", "-loglevel", "error", "-i", str(ROOT / "assets/audio/null-jsfx-loop.flac"), "-t", "32", "-f", "f32le",
                          "-ac", "2", "-ar", str(SR), "-"], check=True, capture_output=True).stdout
    return np.frombuffer(raw, np.float32).reshape(-1, 2).astype(np.float64)


def kick_track(n):
    """The loop's kick pattern alone (120 BPM: beats 1 and 3, and 4 on odd bars), for sidechains."""
    out = np.zeros(n)
    beat = SR // 2
    t = np.arange(int(0.45 * SR)) / SR
    k = np.sin(2 * np.pi * np.cumsum(45 + 110 * np.exp(-t / 0.04)) / SR) * np.exp(-t / 0.18) * 0.6
    for b in range(n // beat + 1):
        bar, beat_in_bar = divmod(b, 4)
        if beat_in_bar in (0, 2) or (beat_in_bar == 3 and bar % 2):
            i = b * beat
            j = min(n, i + len(k))
            if i < n:
                out[i:j] += k[:j - i]
    return out


def make_input(kind, loop):
    """(input channels for the plugin, the stereo 'before' signal the viewer hears)."""
    n = len(loop)
    rng = np.random.default_rng(7)
    x = loop.copy()
    if kind == "hum":
        t = np.arange(n) / SR
        hum = sum(a * np.sin(2 * np.pi * 50 * h * t) for h, a in ((1, 0.06), (2, 0.03), (3, 0.02), (5, 0.008)))
        x = x * 0.8 + hum[:, None]
    elif kind == "noise":
        hiss = rng.standard_normal((n, 2))
        hiss = np.diff(np.vstack([np.zeros((1, 2)), hiss]), axis=0) * 0.5 + hiss * 0.3
        x = x * 0.8 + hiss * 0.012
    elif kind == "plosives":  # P and B pops: low thumps a close mic picks up
        t = np.arange(int(0.12 * SR)) / SR
        pop = np.sin(2 * np.pi * np.cumsum(55 + 90 * np.exp(-t / 0.015)) / SR) * \
            (1 - np.exp(-t / 0.003)) * np.exp(-t / 0.035) * 0.55
        for i in range(int(1.3 * SR), n - len(pop), int(2.1 * SR)):
            i += int(rng.integers(0, SR // 2))
            x[i:i + len(pop)] += pop[:, None] * np.array([1.0, 0.85])
    elif kind == "clicks":
        for i in rng.integers(0, n - 40, 70):
            x[i:i + 3] += rng.choice([-1, 1]) * np.array([0.5, -0.35, 0.15])[:, None]
    if kind == "sidechain":
        k = kick_track(n)
        main = loop * 0.9
        return np.column_stack([main, k, k]), main
    return x, x


# ---------------------------------------------------------------- MIDI patterns

BEAT = SR // 2  # 120 BPM, like the loop and the host time gfx_video reports
# A minor progression, one bar each: Am, F, C, G (chord, bass note)
PROG = [([57, 60, 64], 45), ([53, 57, 60], 41), ([55, 60, 64], 48), ([55, 59, 62], 43)]
MELODY = [  # (beat in bar, length, note) for the four bars
    [(0, 1.5, 76), (1.5, 0.5, 74), (2, 1, 72), (3, 1, 71)],
    [(0, 1.5, 72), (1.5, 0.5, 69), (2, 2, 72)],
    [(0, 1, 67), (1, 1, 72), (2, 1.5, 76), (3.5, 0.5, 74)],
    [(0, 3, 74), (3, 1, 71)],
]
WRONG = [  # a line with notes from outside any one scale, for Scale
    [(0, 1, 69), (1, 0.5, 70), (1.5, 0.5, 72), (2, 1, 73), (3, 0.5, 71), (3.5, 0.5, 68)],
    [(0, 0.5, 66), (0.5, 0.5, 64), (1, 1, 63), (2, 1, 61), (3, 1, 57)],
    [(0, 0.5, 60), (0.5, 0.5, 62), (1, 0.5, 63), (1.5, 0.5, 65), (2, 1, 66), (3, 1, 68)],
    [(0, 1.5, 70), (1.5, 0.5, 69), (2, 2, 64)],
]
PATTERNS = {  # name: what the video says is played
    "chords": "Held chords: Am, F, C, G",
    "pad": "Held chords over a bass note",
    "keys": "Chords, a bass note and a melody",
    "song": "Held chords and a melody",
    "bass": "A bass line: A, F, C, G",
    "roots": "One key per bar: A, F, C, G",
    "riff": "A held key: C, then G",
    "wrong": "A melody with notes out of key",
    "drums": "A straight drum beat (GM notes)",
}


def midi_pattern(name, seconds):
    """[(sample, status, data1, data2)] of a pattern, looped for the whole video."""
    ev = []

    def note(beat, length, pitch, vel):
        ev.append((int(round(beat * BEAT)), 0x90, pitch, vel))
        ev.append((int(round((beat + length) * BEAT)), 0x80, pitch, 0))

    bars = int(seconds * 2 / 4) + 2
    for bar in range(bars):
        b = bar * 4
        chord, root = PROG[bar % 4]
        if name in ("chords", "pad", "song"):
            for n in chord:
                note(b, 4, n, 80)
            if name != "chords":
                note(b, 4, root, 76)
            if name == "song":
                for at, ln, n in MELODY[bar % 4]:
                    note(b + at, ln * 0.95, n, 92)
        elif name == "keys":
            note(b, 4, root, 70)
            for at, ln, v in ((0, 1.25, 80), (1.5, 0.5, 62), (2.5, 1.4, 74)):
                for n in chord:
                    note(b + at, ln, n, v)
            for at, ln, n in MELODY[bar % 4]:
                note(b + at, ln * 0.95, n + 12 if n < 72 else n, 92)
        elif name == "bass":
            r = [45, 41, 36, 43][bar % 4]
            for at, ln, dn, v in ((0, 0.7, 0, 110), (0.75, 0.2, 0, 70), (1.5, 0.45, 0, 95), (2.5, 0.2, 12, 85),
                                  (3, 0.45, 0, 100), (3.5, 0.4, 7, 90)):
                note(b + at, ln, r + dn, v)
        elif name == "roots":
            note(b, 4, [57, 53, 60, 55][bar % 4], 96)
        elif name == "riff":
            note(b, 4, 48 if bar % 4 < 2 else 43, 100)
        elif name == "wrong":
            for at, ln, n in WRONG[bar % 4]:
                note(b + at, ln * 0.9, n, 90)
        elif name == "drums":
            for step in range(16):
                at = b + step / 4
                if step in (0, 8) or (step == 10 and bar % 2):
                    note(at, 0.2, 36, 110)
                if step in (4, 12):
                    note(at, 0.2, 38, 105)
                note(at, 0.1, 42, (96, 50, 72, 50)[step % 4])
        else:
            raise ValueError(f"unknown MIDI pattern {name}")
    end = int(seconds * SR)
    # note-offs before note-ons at the same sample, so a repeated key starts cleanly
    return sorted([e for e in ev if e[0] < end], key=lambda e: (e[0], e[1] != 0x80, e[2]))


def write_midi(events, path):
    Path(path).write_text("".join(f"{a} {b} {c} {d}\n" for a, b, c, d in events))


def read_midi(path):
    return [tuple(int(x) for x in line.split()) for line in Path(path).read_text().split("\n") if line.strip()]


def note_spans(events):
    """[(start s, end s, note, velocity)] from note on/off events."""
    held, out = {}, []
    for pos, st, d1, d2 in events:
        kind, ch = st & 0xF0, st & 0x0F
        if kind == 0x90 and d2 > 0:
            held.setdefault((ch, d1), []).append((pos, d2))
        elif kind in (0x80, 0x90):
            if held.get((ch, d1)):
                p0, v = held[(ch, d1)].pop(0)
                out.append((p0 / SR, pos / SR, d1, v))
    out += [(p0 / SR, 1e9, n, v) for (ch, n), lst in held.items() for p0, v in lst]
    return out


# ---------------------------------------------------------------- script

def plugin_info(stem):
    head = (PLUGINS / f"{stem}.jsfx").read_text(encoding="utf-8").split("\n")
    c = CONTENT.get(stem, {})
    return {
        "stem": stem,
        "name": c.get("name") or head[0][len("desc:NULL "):],
        "category": head[3].split(": ", 1)[1],
        "desc": c.get("card_desc", ""),
        "sliders": parse_sliders(PLUGINS / f"{stem}.jsfx"),
        "units": units(stem),
        "presets": build_presets.load(stem),
        "mode": {"Instruments": "instrument", "MIDI": "midi"}.get(head[3].split(": ", 1)[1], "fx"),
    }


def units(stem):
    """The unit in brackets at the end of each slider label, "" without one."""
    text = (PLUGINS / f"{stem}.jsfx").read_text(encoding="utf-8")
    out = []
    for m in re.finditer(r"^slider\d+:(.*)$", text, re.M):
        if ">" not in m.group(1):  # a file slider (/folder:default:Label): no unit
            out.append("")
            continue
        u = re.search(r"\(([^)]*)\)\s*$", m.group(1).split(">", 1)[1])
        out.append(u.group(1) if u else "")
    return out


def is_log(s):
    name, _, lo, hi, enum = s
    return enum is None and lo > 0 and hi / lo >= 50


def pick_sweep(info, cfg):
    """(slider index, from, to) of the control the video sweeps."""
    if "sweep" in cfg:
        if cfg["sweep"] is None:
            return None
        name, a, b = cfg["sweep"]
        i = [s[0] for s in info["sliders"]].index(name)
        return i, a, b
    skip = ("mix", "output", "input", "dry", "wet", "gain", "ceiling", "oversampling", "quality", "latency")
    for i, s in enumerate(info["sliders"]):
        name, default, lo, hi, enum = s
        if enum is None and hi > lo and not any(k in name.lower() for k in skip):
            break
    else:
        return None
    name, default, lo, hi, enum = info["sliders"][i]
    if is_log(info["sliders"][i]):
        return i, lo * (hi / lo) ** 0.12, lo * (hi / lo) ** 0.88
    return i, lo + (hi - lo) * 0.05, lo + (hi - lo) * 0.9


def build_script(info, cfg):
    """[(seconds, kind, label, values, bypass, sweep)]."""
    presets = info["presets"]
    names = cfg.get("presets")
    if names:
        chosen = [next(p for p in presets if p[0] == n) for n in names]
    else:  # first, middle and last factory preset: a spread from gentle to strong
        rest = presets[1:]
        chosen = [rest[i] for i in sorted({0, len(rest) // 2, len(rest) - 1})] if rest else []
    if not chosen:
        chosen = [presets[0]]
    if info["mode"] == "instrument":  # nothing to compare with: the presets play from the start
        seg = [(3.0, "title", "", chosen[0][1], False, None)]
    else:
        before = cfg.get("input_label", "The loop, untouched" if info["mode"] == "fx" else
                         PATTERNS.get(cfg.get("midi"), "The notes going in"))
        seg = [(3.0, "title", "", chosen[0][1], True, None),
               (4.0, "before", before, chosen[0][1], True, None)]
    for k, (name, vals) in enumerate(chosen):
        seg.append((6.0 if k else 6.5, "preset", name, vals, False, None))
        if k == 0 and info["mode"] != "instrument":
            seg.append((2.0, "ab", name, vals, True, None))
            seg.append((2.5, "ab", name, vals, False, None))
    sw = pick_sweep(info, cfg)
    if sw:
        base = next(p[1] for p in presets if p[0] == cfg["sweep_base"]) if "sweep_base" in cfg else chosen[-1][1]
        seg.append((8.0, "sweep", info["sliders"][sw[0]][0], list(base), False, sw))
    seg.append((4.5, "end", "", seg[-1][3], False, None))
    return seg


def slider_value_text(s, v, unit):
    name, _, lo, hi, enum = s
    if enum:
        return enum[int(round(v - lo))] if 0 <= int(round(v - lo)) < len(enum) else str(v)
    if unit == "Hz" and abs(v) >= 1000:
        return f"{v / 1000:.1f} kHz"
    num = f"{v:.0f}" if abs(v) >= 100 or float(v).is_integer() else f"{v:.1f}"
    if not unit:
        return num
    return num + ("%" if unit == "%" else " " + unit)


# ---------------------------------------------------------------- drawing helpers

def logo(d, x, y, sz):
    """assets/img/logo.svg: the rounded square with the stroke."""
    d.rounded_rectangle([x, y, x + sz, y + sz], radius=int(sz * .15), fill=(45, 45, 45), outline=(74, 74, 74),
                        width=max(1, sz // 48))
    k = sz / 80
    pts = [(x + a * k, y + b * k) for a, b in [(24, 20), (24, 60), (56, 20), (56, 60)]]
    d.line(pts, fill=(160, 160, 160), width=round(6 * k), joint="curve")
    for p in pts:
        r = 3 * k
        d.ellipse([p[0] - r, p[1] - r, p[0] + r, p[1] + r], fill=(160, 160, 160))


def spaced(d, xy, s, f, fill, spacing):
    """Text with letter spacing, like the logo's NULL JSFX."""
    x, y = xy
    for ch in s:
        d.text((x, y), ch, font=f, fill=fill)
        x += d.textlength(ch, font=f) + spacing
    return x


def ctext(d, y, s, f, c, cx=W // 2):
    d.text((cx - d.textlength(s, font=f) / 2, y), s, font=f, fill=c)


def background():
    y, x = np.mgrid[0:H, 0:W]
    a = np.clip(1 - np.sqrt((x - W * 0.32) ** 2 + (y - H * 0.5) ** 2) / 1300, 0, 1) ** 2
    arr = np.array(BG, float) * (1 - a[..., None]) + np.array((40, 40, 42), float) * a[..., None]
    arr += np.random.default_rng(1).uniform(-0.5, 0.5, arr.shape)  # dither: no rings in the gradient
    return Image.fromarray(np.clip(np.round(arr), 0, 255).astype("uint8"))


def header(d):
    logo(d, 64, 40, 52)
    spaced(d, (134, 42), "NULL JSFX", font(True, 26), (212, 212, 212), 5)
    spaced(d, (135, 78), "AUDIO WITHOUT NOISE", font(False, 12), MUTED, 3.2)
    s = "nulljsfx.tech"
    d.text((W - 64 - d.textlength(s, font=font(False, 22)), 54), s, font=font(False, 22), fill=MUTED)


def wrap(d, s, f, width):
    words, lines, cur = s.split(), [], ""
    for w_ in words:
        t = (cur + " " + w_).strip()
        if d.textlength(t, font=f) <= width:
            cur = t
        else:
            lines.append(cur)
            cur = w_
    return lines + ([cur] if cur else [])


def pill(d, x, y, label, on, wpx=150):
    d.rounded_rectangle([x, y, x + wpx, y + 46], radius=23, fill=SILVER if on else PANEL,
                        outline=SILVER if on else BORDER, width=2)
    f = font(True, 20)
    d.text((x + (wpx - d.textlength(label, font=f)) / 2, y + 10), label, font=f, fill=(24, 24, 24) if on else MUTED)


# ---------------------------------------------------------------- analysis

NB = 120
EDGES = np.geomspace(SR_HZ[0], SR_HZ[1], NB + 1)
FFT_N = 4096
BIN_HZ = np.fft.rfftfreq(FFT_N, 1 / SR)
BAND_OF = np.clip(np.searchsorted(EDGES, BIN_HZ) - 1, -1, NB)
WINDOW = np.hanning(FFT_N)


def spectrum(x, end):
    seg = x[max(0, end - FFT_N):end]
    if len(seg) < FFT_N:
        seg = np.vstack([np.zeros((FFT_N - len(seg), 2)), seg])
    m = (np.abs(np.fft.rfft(seg.mean(axis=1) * WINDOW)) ** 2)
    bands = np.zeros(NB)
    cnt = np.zeros(NB)
    ok = (BAND_OF >= 0) & (BAND_OF < NB)
    np.add.at(bands, BAND_OF[ok], m[ok])
    np.add.at(cnt, BAND_OF[ok], 1)
    # empty low bands borrow the nearest bin
    for i in np.where(cnt == 0)[0]:
        f = math.sqrt(EDGES[i] * EDGES[i + 1])
        bands[i] = m[int(round(f / (SR / FFT_N)))]
        cnt[i] = 1
    bands /= cnt
    bands = np.convolve(np.pad(bands, 3, mode="edge"), np.hanning(9)[1:-1] / np.hanning(9)[1:-1].sum(), "valid")
    # +3 dB/oct tilt so music reads flat-ish
    centers = np.sqrt(EDGES[:-1] * EDGES[1:])
    return 10 * np.log10(bands + 1e-12) + 3 * np.log2(centers / 1000) - 10 * math.log10(FFT_N / 4)


def smooth_db(prev, new):
    """Falls slowly, rises fast, so the curve reads calmly at 30 fps."""
    return np.where(new > prev, prev + (new - prev) * 0.6, prev + (new - prev) * 0.18)


def master(mix, path):
    """One gain for the whole video to about -16 LUFS (so before and after keep their real
    difference), a soft ceiling at -1 dBFS, written as float32 for the encoder."""
    mix.astype(np.float32).tofile(path)
    r = subprocess.run(["ffmpeg", "-hide_banner", "-f", "f32le", "-ar", str(SR), "-ac", "2", "-i", str(path),
                        "-af", "ebur128", "-f", "null", "-"], capture_output=True, text=True).stderr
    lufs = float(re.findall(r"I:\s+(-?[\d.]+) LUFS", r)[-1])
    y = mix * 10 ** ((-16 - lufs) / 20)
    c = 10 ** (-1 / 20)
    y = np.where(np.abs(y) > 0.8 * c, np.sign(y) * (0.8 * c + 0.2 * c * np.tanh((np.abs(y) - 0.8 * c) / (0.2 * c))), y)
    y.astype(np.float32).tofile(path)


def encoder(stem, audio, hq_dir):
    """ffmpeg reading RGB24 1920x1080 frames on stdin: the 720p file for the repo and, with
    hq_dir, the 1080p one for Ko-fi."""
    cmd = ["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{W}x{H}",
           "-r", str(FPS), "-i", "-", "-f", "f32le", "-ar", str(SR), "-ac", "2", "-i", str(audio)]
    if hq_dir:
        cmd += ["-filter_complex", "[0:v]split=2[a][b];[b]scale=1280:720:flags=lanczos[s]",
                "-map", "[a]", "-map", "1:a", "-c:v", "libx264", "-preset", "slow", "-crf", "19", "-tune",
                "animation", "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "192k", "-movflags", "+faststart",
                "-shortest", str(hq_dir / f"{stem}.mp4")]
    else:
        cmd += ["-filter_complex", "[0:v]scale=1280:720:flags=lanczos[s]"]
    cmd += ["-map", "[s]", "-map", "1:a", "-c:v", "libx264", "-preset", "slow", "-crf", "28", "-tune",
            "animation", "-g", "60", "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "96k", "-ac", "2",
            "-movflags", "+faststart", "-shortest", str(OUT / f"{stem}.mp4")]
    return subprocess.Popen(cmd, stdin=subprocess.PIPE)


def poster(arr, stem):
    Image.fromarray(arr).resize((1280, 720), Image.LANCZOS).save(OUT / f"{stem}.jpg", quality=82, optimize=True,
                                                                  progressive=True)


def stage(info, gw, gh):
    """The static frame around an interface of gw x gh: header, the interface's place and shadow,
    name and description, the empty spectrum and level panels."""
    box_w, box_h = 1120, 830
    sc = min(box_w / gw, box_h / gh)
    uw, uh = int(gw * sc) // 2 * 2, int(gh * sc) // 2 * 2
    ux, uy = 64 + (box_w - uw) // 2, 150 + (box_h - uh) // 2
    bg = background()
    d = ImageDraw.Draw(bg)
    header(d)
    sh = Image.new("L", (uw + 80, uh + 80), 0)
    ImageDraw.Draw(sh).rounded_rectangle([40, 40, uw + 40, uh + 40], radius=14, fill=170)
    bg.paste(Image.new("RGB", sh.size, (0, 0, 0)), (ux - 40, uy - 26), sh.filter(ImageFilter.GaussianBlur(20)))
    rx, rw = 1240, 616
    d.text((rx, 150), info["category"].upper(), font=font(False, 20), fill=MUTED)
    nf = font(True, 60)
    while d.textlength(info["name"], font=nf) > rw:
        nf = font(True, nf.size - 4)
    d.text((rx, 176), info["name"], font=nf, fill=TEXT)
    for j, line in enumerate(wrap(d, info["desc"], font(False, 26), rw)[:2]):
        d.text((rx, 262 + j * 36), line, font=font(False, 26), fill=SILVER)
    sp = (rx, 600, rw, 200)
    lv = (rx, 830, rw, 150)
    # what the two panels show: effects compare the spectrum and level before and after; MIDI
    # effects show the notes in and out, instruments their output and the notes they get
    mode = info["mode"]
    kinds = {"fx": ("spectrum", "level"), "midi": ("notes", "level"),
             "instrument": ("spectrum", "notes" if info.get("midi") else "level")}[mode]
    for (x0, y0, w0, h0), kind in zip((sp, lv), kinds):
        d.rounded_rectangle([x0, y0 - 30, x0 + w0, y0 + h0], radius=10, fill=(24, 24, 23), outline=BORDER, width=1)
        d.text((x0 + 14, y0 - 24), kind.upper(), font=font(True, 14), fill=MUTED)
        lx = x0 + w0 - 14
        legend = [("BEFORE", (110, 110, 108)), ("AFTER", SILVER)]
        if kind == "notes":
            legend = [("IN", (110, 110, 108)), ("OUT", SILVER)] if mode == "midi" else [("MIDI IN", SILVER)]
        elif mode == "instrument":
            legend = [("OUTPUT", SILVER)]
        for name, col in reversed(legend):
            lx -= d.textlength(name, font=font(False, 14))
            d.text((lx, y0 - 24), name, font=font(False, 14), fill=col)
            lx -= 22
            d.rounded_rectangle([lx, y0 - 18, lx + 14, y0 - 12], radius=3, fill=col)
            lx -= 14
    for hz, lab in ((100, "100"), (1000, "1k"), (10000, "10k")) if kinds[0] == "spectrum" else ():
        xx = sp[0] + sp[2] * math.log(hz / SR_HZ[0]) / math.log(SR_HZ[1] / SR_HZ[0])
        d.line([xx, sp[1] + 4, xx, sp[1] + sp[3] - 4], fill=(40, 40, 40), width=1)
        d.text((xx + 4, sp[1] + sp[3] - 22), lab, font=font(False, 13), fill=DIM)
    if mode != "fx":  # what plays: the pattern into an instrument, the instrument after a MIDI effect
        x = rx if mode == "instrument" else rx + 340
        d.text((x, 368), "PLAYED BY" if mode == "midi" else "PLAYING", font=font(True, 14), fill=MUTED)
        line = info.get("played_by") or ""
        f = font(False, 22)
        while d.textlength(line, font=f) > rx + rw - x and f.size > 14:
            f = font(False, f.size - 1)
        d.text((x, 388), line, font=f, fill=SILVER)
    umask = Image.new("L", (uw, uh), 0)
    ImageDraw.Draw(umask).rounded_rectangle([0, 0, uw - 1, uh - 1], radius=12, fill=255)
    return {"base": np.asarray(bg).copy(), "mask": umask, "box": (ux, uy, uw, uh), "sp": sp, "lv": lv,
            "rpos": (rx, rw), "kinds": kinds, "mode": mode}


def film(stem, info, script, st, ui_at, dry, wet, ctl, audio, hq_dir, title, end):
    """Draws and encodes every frame: the title card, the interface (ui_at(frame) gives it) with
    the captions and the before/after panels, the end card. dry and wet are what the panels
    analyse; the soundtrack is already in audio."""
    total = sum(s[0] for s in script)
    starts = np.cumsum([0] + [s[0] for s in script])
    n = len(wet)
    enc = encoder(stem, audio, hq_dir)
    spec_b = spec_a = None
    still = None
    poster_t = starts[2] + 4.0
    title, end = np.asarray(title), np.asarray(end)

    def draw(seg, p, f, t, end_s, bypass):
        return np.asarray(frame(st, ui, info, seg, p, ctl[f] if ctl is not None else None, spec_b, spec_a, dry,
                                dry if bypass else wet, end_s, t, total))

    for f in range(int(round(total * FPS))):
        t = f / FPS
        i = min(len(script) - 1, int(np.searchsorted(starts, t, side="right") - 1))
        p = (t - starts[i]) / script[i][0]
        dur, kind, label, vals, bypass, sw = script[i]
        ui = ui_at(f)
        end_s = min(n, int(t * SR) + SR // FPS)
        sb = spectrum(dry, end_s)
        sa = spectrum(dry if bypass else wet, end_s)
        spec_b = sb if spec_b is None else smooth_db(spec_b, sb)
        spec_a = sa if spec_a is None else smooth_db(spec_a, sa)
        if kind == "title":
            arr = title
            if t > starts[1] - 0.5:
                a = (t - (starts[1] - 0.5)) / 0.5
                arr = (title * (1 - a) + draw(script[1], 0, f, t, end_s, script[1][4]) * a).astype(np.uint8)
        elif kind == "end":
            arr = end
            if p * dur < 0.5:
                a = p * dur / 0.5
                arr = (draw(script[i - 1], 1, f, t, end_s, False) * (1 - a) + end * a).astype(np.uint8)
        else:
            arr = draw(script[i], p, f, t, end_s, bypass)
        if t < 0.6:
            arr = (arr * (t / 0.6)).astype(np.uint8)
        if t > total - 0.8:
            arr = (arr * max(0, (total - t) / 0.8)).astype(np.uint8)
        if still is None and t >= poster_t:
            still = arr.copy()
        enc.stdin.write(np.ascontiguousarray(arr).tobytes())
    enc.stdin.close()
    if enc.wait():
        raise RuntimeError(f"{stem}: ffmpeg failed")
    poster(still, stem)


# ---------------------------------------------------------------- render one plugin

def render(stem, hq_dir, tool, loop):
    info = plugin_info(stem)
    cfg = CONFIG.get(stem, {})
    script = build_script(info, cfg)
    total = sum(s[0] for s in script)
    nframes = int(round(total * FPS))
    nsl = len(info["sliders"])
    starts = np.cumsum([0] + [s[0] for s in script])

    def seg_at(t):
        i = min(len(script) - 1, int(np.searchsorted(starts, t, side="right") - 1))
        return i, (t - starts[i]) / script[i][0]

    # per-frame slider values and bypass flags
    ctl = np.zeros((nframes, nsl), np.float32)
    byp = np.zeros(nframes)
    for f in range(nframes):
        i, p = seg_at(f / FPS)
        _, kind, _, vals, bypass, sw = script[i]
        v = list(vals)
        if sw:
            k, a, b = sw
            q = 0.5 - 0.5 * math.cos(math.pi * min(1, max(0, (p - 0.08) / 0.84)) * 2)  # there and back
            s = info["sliders"][k]
            v[k] = a * (b / a) ** q if is_log(s) else a + (b - a) * q
            lo, hi = s[2], s[3]
            step = 1 if (hi - lo) >= 20 and not is_log(s) else 0
            if step:
                v[k] = round(v[k])
        if "pulse" in cfg and kind not in ("title", "before") and not bypass:
            # a switch (Tape Stop's Trigger, Spectral Freeze's Freeze) held on for a while every few seconds
            name, every, length = cfg["pulse"]
            k = [s[0] for s in info["sliders"]].index(name)
            v[k] = 1 if 1.0 <= (f / FPS - starts[i]) % every < 1.0 + length else 0
        ctl[f] = v
        byp[f] = 1.0 if bypass else 0.0

    mode = info["mode"]
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        midi_args, events = [], []
        if mode == "fx":
            ins, before = make_input(cfg.get("input", "loop"), loop)
        else:  # instruments and MIDI effects: silence in, the MIDI pattern as what is played
            ins = before = np.zeros((SR, 2))
            if cfg.get("midi"):
                events = midi_pattern(cfg["midi"], total + 1)
                write_midi(events, tmp / "midi_in.txt")
                midi_args = ["-m", str(tmp / "midi_in.txt")]
        n_in = ins.shape[1]
        ins.astype(np.float32).tofile(tmp / "in.f32")
        ctl.tofile(tmp / "ctl.f32")
        plugin = GFX / f"{stem}_gfx.jsfx"
        out_args = ["-M", str(tmp / "midi_out.txt")] if mode == "midi" else []
        r = subprocess.run([tool] + midi_args + out_args + [str(plugin), str(tmp / "in.f32"), str(n_in),
                           str(tmp / "ctl.f32"), str(nsl), str(FPS), str(tmp / "out.f32")],
                           capture_output=True, text=True)
        if r.returncode:
            raise RuntimeError(f"{stem}: gfx_video failed: {r.stderr}")
        pins = int(r.stderr.strip().split("\n")[-1])
        wet_all = np.fromfile(tmp / "out.f32", np.float32).reshape(-1, pins).astype(np.float64)
        n = len(wet_all)
        out_pair = cfg.get("output", "first")
        if out_pair == "sum":
            wet = sum(wet_all[:, c:c + 2] for c in range(0, pins, 2))
        else:
            wet = wet_all[:, 0:2]
        dry = np.vstack([before] * (n // len(before) + 1))[:n]
        if mode == "midi":  # the instrument plays the notes going in (before) and coming out (after)
            out_events = read_midi(tmp / "midi_out.txt")
            inst, preset = cfg["play"]
            dry = play_instrument(tool, inst, preset, tmp / "midi_in.txt", nframes, n, tmp)
            wet = play_instrument(tool, inst, preset, tmp / "midi_out.txt", nframes, n, tmp)
            info["played_by"] = f"{plugin_info(inst)['name']} · {preset}"
            info["notes"] = (note_spans(events), note_spans(out_events))
        elif mode == "instrument":
            info["played_by"] = PATTERNS[cfg["midi"]] if cfg.get("midi") else "Its own groove, in time with the host"
            info["notes"] = (note_spans(events), [])
        info["midi"] = cfg.get("midi")

        # dry/wet by bypass, 25 ms crossfades
        g = np.repeat(1 - byp, SR // FPS)[:n]
        if len(g) < n:
            g = np.concatenate([g, np.full(n - len(g), g[-1])])
        k = int(0.025 * SR)
        g = np.convolve(np.concatenate([np.full(k, g[0]), g]), np.ones(k) / k, "valid")[:n]
        mix = dry * (1 - g[:, None]) + wet * g[:, None]
        t = np.arange(n) / SR
        fade = np.clip(t / 1.0, 0, 1) * np.clip((total - t) / 2.0, 0, 1)
        mix *= fade[:, None]
        master(mix, tmp / "mix.f32")

        gw, gh = [int(v) for v in next(l for l in plugin.read_text(encoding="utf-8").split("\n")
                                       if l.startswith("@gfx")).split()[1:3]]
        st = stage(info, gw, gh)
        if "notes" in info:
            st["notes_in"], st["notes_out"] = info["notes"]
            ns = [x[2] for x in st["notes_in"] + st["notes_out"]] or [60]
            lo, hi = min(ns) - 1, max(ns) + 1
            while hi - lo < 14:
                lo, hi = lo - 1, hi + 1
            st["note_range"] = (lo, hi)
        uw, uh = st["box"][2], st["box"][3]
        gfxp = subprocess.Popen([tool] + midi_args + [str(plugin), str(tmp / "in.f32"), str(n_in), str(tmp / "ctl.f32"), str(nsl),
                                 str(FPS), str(tmp / "x.f32"), str(uw), str(uh)], stdout=subprocess.PIPE)
        film(stem, info, script, st, lambda f: np.frombuffer(gfxp.stdout.read(uw * uh * 3), np.uint8).reshape(uh, uw, 3),
             dry, wet, ctl, tmp / "mix.f32", hq_dir, title_card(info), end_card(info))
        gfxp.wait()
    return stem, total


def play_instrument(tool, stem, preset, midi, nframes, n, tmp):
    """The stereo output of a NULL instrument at one of its factory presets playing a MIDI file,
    n samples long (a MIDI effect's video: what its notes sound like)."""
    vals = next(v for name, v in build_presets.load(stem) if name == preset)
    np.tile(np.asarray(vals, np.float32), (nframes, 1)).tofile(tmp / "inst_ctl.f32")
    r = subprocess.run([tool, "-m", str(midi), str(GFX / f"{stem}_gfx.jsfx"), str(tmp / "in.f32"), "2",
                        str(tmp / "inst_ctl.f32"), str(len(vals)), str(FPS), str(tmp / "inst.f32")],
                       capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError(f"{stem}: gfx_video failed: {r.stderr}")
    pins = int(r.stderr.strip().split("\n")[-1])
    out = np.fromfile(tmp / "inst.f32", np.float32).reshape(-1, pins)[:, 0:2].astype(np.float64)
    return out[:n] if len(out) >= n else np.vstack([out, np.zeros((n - len(out), 2))])


def frame(st, ui, info, seg, p, vals, spec_b, spec_a, dry, wet, end_s, t, total):
    dur, kind, label, _, bypass, sw = seg
    ux, uy, uw, uh = st["box"]
    umask, sp, lv, rpos = st["mask"], st["sp"], st["lv"], st["rpos"]
    hist_n, hop = 300, SR // 60  # the level history: 5 s
    im = Image.fromarray(st["base"].copy())
    uimg = Image.fromarray(ui)
    if uimg.size != (uw, uh):
        uimg = uimg.resize((uw, uh), Image.LANCZOS)
    if bypass:
        uimg = Image.blend(uimg, Image.new("RGB", uimg.size, (12, 12, 12)), 0.5)
    im.paste(uimg, (ux, uy), umask)
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([ux - 1, uy - 1, ux + uw, uy + uh], radius=12, outline=(70, 70, 72), width=2)
    if bypass:
        s = "BYPASSED"
        f = font(True, 30)
        tw = d.textlength(s, font=f)
        d.rounded_rectangle([ux + uw / 2 - tw / 2 - 24, uy + uh / 2 - 30, ux + uw / 2 + tw / 2 + 24, uy + uh / 2 + 30],
                            radius=30, fill=(20, 20, 20), outline=BORDER, width=2)
        d.text((ux + uw / 2 - tw / 2, uy + uh / 2 - 21), s, font=f, fill=SILVER)
    rx, rw = rpos
    if st["mode"] != "instrument":
        pill(d, rx, 370, "BEFORE", bypass)
        pill(d, rx + 166, 370, "AFTER", not bypass)
    # caption
    if isinstance(label, tuple):
        top, big = label
    elif kind == "before":
        top, big = "INPUT", label
    elif kind in ("preset", "ab"):
        top, big = "FACTORY PRESET", label
    elif kind == "sweep":
        k = sw[0]
        s = info["sliders"][k]
        top, big = "TURNING " + s[0].upper(), slider_value_text(s, vals[k], info["units"][k])
    else:
        top, big = "", ""
    d.text((rx, 446), top, font=font(True, 18), fill=MUTED)
    d.text((rx, 470), big, font=font(True, 40), fill=TEXT)
    if kind == "sweep":
        k = sw[0]
        s = info["sliders"][k]
        lo, hi = s[2], s[3]
        q = (math.log(vals[k] / lo) / math.log(hi / lo)) if is_log(s) else (vals[k] - lo) / (hi - lo)
        y = 536
        d.rounded_rectangle([rx, y, rx + rw, y + 8], radius=4, fill=(45, 45, 45))
        d.rounded_rectangle([rx, y, rx + max(8, rw * q), y + 8], radius=4, fill=SILVER)
    show_before = st["mode"] != "instrument"
    for rect, kind in zip((sp, lv), st["kinds"]):
        if kind == "spectrum":
            draw_spectrum(d, rect, spec_b, spec_a, show_before)
        elif kind == "level":
            draw_level(d, rect, dry, wet, end_s, hist_n, hop, show_before)
        else:
            draw_notes(d, rect, st, end_s / SR, bypass)
    # progress
    d.rounded_rectangle([64, H - 30, W - 64, H - 26], radius=2, fill=(40, 40, 40))
    d.rounded_rectangle([64, H - 30, 64 + (W - 128) * t / total, H - 26], radius=2, fill=(120, 120, 120))
    return im


def draw_spectrum(d, rect, spec_b, spec_a, show_before):
    x0, y0, w0, h0 = rect

    def curve(spec):
        lo_db, hi_db = -80, 0
        ys = y0 + h0 - 6 - np.clip((spec - lo_db) / (hi_db - lo_db), 0, 1) * (h0 - 12)
        xs = x0 + 2 + np.linspace(0, w0 - 4, len(spec))
        return list(zip(xs, ys))

    if show_before:
        pb = curve(spec_b)
        d.polygon(pb + [(x0 + w0 - 1, y0 + h0 - 2), (x0 + 1, y0 + h0 - 2)], fill=(44, 44, 43))
        d.line(pb, fill=(110, 110, 108), width=2, joint="curve")
    else:
        pa = curve(spec_a)
        d.polygon(pa + [(x0 + w0 - 1, y0 + h0 - 2), (x0 + 1, y0 + h0 - 2)], fill=(44, 44, 43))
    d.line(curve(spec_a), fill=SILVER, width=3, joint="curve")


def draw_level(d, rect, dry, wet, end_s, cols, hop, show_before):
    """The level history: 5 s of peaks, before as a grey area, after as silver lines."""
    x0, y0, w0, h0 = rect
    mid = y0 + h0 / 2
    ends = end_s - np.arange(cols)[::-1] * hop

    def peaks(x):
        out = np.zeros(cols)
        for j, e in enumerate(ends):
            if e > hop:
                out[j] = np.abs(x[e - hop:e]).max()
        return np.clip(out / 0.8, 0, 1) ** 0.6
    pak = peaks(wet)
    pbk = peaks(dry) if show_before else pak
    xs = x0 + 8 + np.arange(cols) * (w0 - 16) / (cols - 1)
    top_b = [(x, mid - v * (h0 / 2 - 8)) for x, v in zip(xs, pbk)]
    bot_b = [(x, mid + v * (h0 / 2 - 8)) for x, v in zip(xs[::-1], pbk[::-1])]
    d.polygon(top_b + bot_b, fill=(52, 52, 52) if show_before else (44, 44, 43))
    d.line([(x, mid - v * (h0 / 2 - 8)) for x, v in zip(xs, pak)], fill=SILVER, width=2)
    d.line([(x, mid + v * (h0 / 2 - 8)) for x, v in zip(xs, pak)], fill=SILVER, width=2)


def draw_notes(d, rect, st, now, bypass, span=4.0):
    """A piano roll of the last `span` seconds, now at the right edge: the notes going in (grey;
    silver for an instrument or a bypassed MIDI effect) and, for a MIDI effect, the notes coming
    out (silver) over them."""
    x0, y0, w0, h0 = rect
    lo, hi = st["note_range"]
    row = (h0 - 16) / (hi - lo + 1)
    xl, xr = x0 + 8, x0 + w0 - 8

    def bars(notes, fill, lit):
        for t0, t1, n, v in notes:
            if t1 < now - span or t0 > now:
                continue
            a = xl + (xr - xl) * max(0.0, 1 - (now - t0) / span)
            b = xl + (xr - xl) * (1 - (now - min(t1, now)) / span)
            y = y0 + 8 + (hi - n) * row
            on = t0 <= now < t1
            d.rounded_rectangle([a, y + 1, max(a + 3, b), y + max(3, row - 1)], radius=min(3, row / 3),
                                fill=lit if on else fill)

    if st["mode"] == "midi" and not bypass:
        bars(st["notes_in"], (84, 84, 82), (120, 120, 118))
        bars(st["notes_out"], SILVER, TEXT)
    else:
        bars(st["notes_in"], SILVER, TEXT)


def title_card(info):
    im = background()
    d = ImageDraw.Draw(im)
    logo(d, W // 2 - 50, 250, 100)
    ctext(d, 380, info["category"].upper(), font(False, 28), MUTED)
    f = font(True, 120)
    while d.textlength(info["name"], font=f) > W - 200:
        f = font(True, f.size - 6)
    ctext(d, 420, info["name"], f, TEXT)
    ctext(d, 590, info["desc"], font(False, 40), SILVER)
    ctext(d, 760, "NULL JSFX  ·  AUDIO WITHOUT NOISE", font(False, 24), MUTED)
    return im


def end_card(info):
    im = background()
    d = ImageDraw.Draw(im)
    logo(d, W // 2 - 50, 200, 100)
    ctext(d, 330, info["name"], font(True, 84), TEXT)
    ctext(d, 460, "Free and open source  ·  JSFX  ·  GFX  ·  VST3  ·  CLAP", font(False, 34), SILVER)
    ctext(d, 520, "Windows  ·  Linux  ·  macOS", font(False, 30), MUTED)
    ctext(d, 640, "nulljsfx.tech", font(True, 56), TEXT)
    ctext(d, 740, "Support the work: ko-fi.com/koulaxizis", font(False, 30), MUTED)
    return im


def all_plugins():
    return sorted(p.name[:-len("_gfx.jsfx")] for p in GFX.glob("*_gfx.jsfx"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("plugins", nargs="*")
    ap.add_argument("--hq", type=Path)
    ap.add_argument("--jobs", type=int, default=1)
    a = ap.parse_args()
    tool = subprocess.run([str(HERE / "build.sh")], check=True, capture_output=True, text=True).stdout.strip()
    OUT.mkdir(parents=True, exist_ok=True)
    if a.hq:
        a.hq.mkdir(parents=True, exist_ok=True)
    loop = load_loop()
    names = a.plugins or all_plugins()
    if a.jobs > 1:
        with ProcessPoolExecutor(a.jobs) as ex:
            for stem, total in ex.map(render, names, [a.hq] * len(names), [tool] * len(names), [loop] * len(names)):
                print(f"{stem}: {total:.1f} s, {(OUT / (stem + '.mp4')).stat().st_size / 1e6:.2f} MB", flush=True)
    else:
        for s in names:
            stem, total = render(s, a.hq, tool, loop)
            print(f"{stem}: {total:.1f} s, {(OUT / (stem + '.mp4')).stat().st_size / 1e6:.2f} MB", flush=True)


if __name__ == "__main__":
    main()
