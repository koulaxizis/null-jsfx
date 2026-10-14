#!/usr/bin/env python3
"""Renders the presentation video of every plugin from its GFX version and the demo loop.

Each video plays the first 32 s (16 bars) of assets/audio/null-jsfx-loop.flac through the real plugin (the GFX JSFX, run by
gfx_video with ysfx, the same DSP as every format) and films its own interface live, next to
a spectrum and a level history of the signal before (grey) and after (silver). The script
is: title, the loop bypassed, up to three factory presets with a quick before/after, a sweep
of the main control, end card. tools/video/videos.json can change a plugin's script:
  "presets": [names]          which factory presets (default: first, middle and last)
  "sweep": [slider, from, to] the control to turn (null: none), "sweep_base": preset to turn it on
  "input": hum|noise|clicks|sidechain  what goes in (default: the loop)
  "pulse": [slider, every, on] switch a 0/1 slider on for `on` seconds every `every` seconds

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
    elif kind == "clicks":
        for i in rng.integers(0, n - 40, 70):
            x[i:i + 3] += rng.choice([-1, 1]) * np.array([0.5, -0.35, 0.15])[:, None]
    if kind == "sidechain":
        k = kick_track(n)
        main = loop * 0.9
        return np.column_stack([main, k, k]), main
    return x, x


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
    }


def units(stem):
    """The unit in brackets at the end of each slider label, "" without one."""
    text = (PLUGINS / f"{stem}.jsfx").read_text(encoding="utf-8")
    out = []
    for m in re.finditer(r"^slider\d+:[^>\n]*>(.*)$", text, re.M):
        u = re.search(r"\(([^)]*)\)\s*$", m.group(1))
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
    seg = [(3.0, "title", "", chosen[0][1], True, None),
           (4.0, "before", "The loop, untouched", chosen[0][1], True, None)]
    for k, (name, vals) in enumerate(chosen):
        seg.append((6.0 if k else 6.5, "preset", name, vals, False, None))
        if k == 0:
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
    for (x0, y0, w0, h0), lab in ((sp, "SPECTRUM"), (lv, "LEVEL")):
        d.rounded_rectangle([x0, y0 - 30, x0 + w0, y0 + h0], radius=10, fill=(24, 24, 23), outline=BORDER, width=1)
        d.text((x0 + 14, y0 - 24), lab, font=font(True, 14), fill=MUTED)
        lx = x0 + w0 - 14
        for name, col in reversed([("BEFORE", (110, 110, 108)), ("AFTER", SILVER)]):
            lx -= d.textlength(name, font=font(False, 14))
            d.text((lx, y0 - 24), name, font=font(False, 14), fill=col)
            lx -= 22
            d.rounded_rectangle([lx, y0 - 18, lx + 14, y0 - 12], radius=3, fill=col)
            lx -= 14
    for hz, lab in ((100, "100"), (1000, "1k"), (10000, "10k")):
        xx = sp[0] + sp[2] * math.log(hz / SR_HZ[0]) / math.log(SR_HZ[1] / SR_HZ[0])
        d.line([xx, sp[1] + 4, xx, sp[1] + sp[3] - 4], fill=(40, 40, 40), width=1)
        d.text((xx + 4, sp[1] + sp[3] - 22), lab, font=font(False, 13), fill=DIM)
    umask = Image.new("L", (uw, uh), 0)
    ImageDraw.Draw(umask).rounded_rectangle([0, 0, uw - 1, uh - 1], radius=12, fill=255)
    return {"base": np.asarray(bg).copy(), "mask": umask, "box": (ux, uy, uw, uh), "sp": sp, "lv": lv,
            "rpos": (rx, rw)}


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
                arr = (title * (1 - a) + draw(script[1], 0, f, t, end_s, True) * a).astype(np.uint8)
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

    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        ins, before = make_input(cfg.get("input", "loop"), loop)
        n_in = ins.shape[1]
        ins.astype(np.float32).tofile(tmp / "in.f32")
        ctl.tofile(tmp / "ctl.f32")
        plugin = GFX / f"{stem}_gfx.jsfx"
        r = subprocess.run([tool, str(plugin), str(tmp / "in.f32"), str(n_in), str(tmp / "ctl.f32"), str(nsl),
                            str(FPS), str(tmp / "out.f32")], capture_output=True, text=True)
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
        uw, uh = st["box"][2], st["box"][3]
        gfxp = subprocess.Popen([tool, str(plugin), str(tmp / "in.f32"), str(n_in), str(tmp / "ctl.f32"), str(nsl),
                                 str(FPS), str(tmp / "x.f32"), str(uw), str(uh)], stdout=subprocess.PIPE)
        film(stem, info, script, st, lambda f: np.frombuffer(gfxp.stdout.read(uw * uh * 3), np.uint8).reshape(uh, uw, 3),
             dry, wet, ctl, tmp / "mix.f32", hq_dir, title_card(info), end_card(info))
        gfxp.wait()
    return stem, total


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
    # spectrum
    x0, y0, w0, h0 = sp

    def curve(spec):
        lo_db, hi_db = -80, 0
        ys = y0 + h0 - 6 - np.clip((spec - lo_db) / (hi_db - lo_db), 0, 1) * (h0 - 12)
        xs = x0 + 2 + np.linspace(0, w0 - 4, len(spec))
        return list(zip(xs, ys))

    pb = curve(spec_b)
    d.polygon(pb + [(x0 + w0 - 1, y0 + h0 - 2), (x0 + 1, y0 + h0 - 2)], fill=(44, 44, 43))
    d.line(pb, fill=(110, 110, 108), width=2, joint="curve")
    d.line(curve(spec_a), fill=SILVER, width=3, joint="curve")
    # level history
    x0, y0, w0, h0 = lv
    mid = y0 + h0 / 2
    cols = hist_n
    ends = end_s - np.arange(cols)[::-1] * hop
    def peaks(x):
        out = np.zeros(cols)
        for j, e in enumerate(ends):
            if e > hop:
                out[j] = np.abs(x[e - hop:e]).max()
        return np.clip(out / 0.8, 0, 1) ** 0.6
    pbk, pak = peaks(dry), peaks(wet)
    xs = x0 + 8 + np.arange(cols) * (w0 - 16) / (cols - 1)
    top_b = [(x, mid - v * (h0 / 2 - 8)) for x, v in zip(xs, pbk)]
    bot_b = [(x, mid + v * (h0 / 2 - 8)) for x, v in zip(xs[::-1], pbk[::-1])]
    d.polygon(top_b + bot_b, fill=(52, 52, 52))
    d.line([(x, mid - v * (h0 / 2 - 8)) for x, v in zip(xs, pak)], fill=SILVER, width=2)
    d.line([(x, mid + v * (h0 / 2 - 8)) for x, v in zip(xs, pak)], fill=SILVER, width=2)
    # progress
    d.rounded_rectangle([64, H - 30, W - 64, H - 26], radius=2, fill=(40, 40, 40))
    d.rounded_rectangle([64, H - 30, 64 + (W - 128) * t / total, H - 26], radius=2, fill=(120, 120, 120))
    return im


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
