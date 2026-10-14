#!/usr/bin/env python3
"""Renders the NULL JSFX Hub video: a filmed tour of the Hub's page (tools/video/hub_tour.cjs,
with the test stand-in, so nothing is really installed) in a window, the chapter titles beside
it, and the demo loop underneath as a music bed.

Writes assets/video/hub.mp4 and hub.jpg (and <hq dir>/hub.mp4 with --hq), like make_videos.py.
Usage: make_hub_video.py [--hq DIR]      (needs hub/node_modules: cd hub && npm ci)
"""
import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import make_videos as mv  # noqa: E402
from make_videos import FPS, SR, W, font  # noqa: E402

TITLE, END = 4.0, 5.0
WIN = (64, 150, 1344, 840)  # x, y, width, height of the page in the frame
BAR = 34


def window(base):
    """The frame with an empty app window (title bar, shadow) and the static side panel."""
    im = base.copy()
    x, y, w, h = WIN
    sh = Image.new("L", (w + 100, h + BAR + 100), 0)
    ImageDraw.Draw(sh).rounded_rectangle([50, 50, w + 50, h + BAR + 50], radius=12, fill=190)
    im.paste(Image.new("RGB", sh.size, (0, 0, 0)), (x - 50, y - BAR - 34), sh.filter(ImageFilter.GaussianBlur(24)))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([x - 1, y - BAR - 1, x + w, y + h], radius=10, fill=(20, 20, 19), outline=(64, 64, 62), width=2)
    s = "NULL JSFX Hub"
    d.text((x + (w - d.textlength(s, font=font(False, 16))) / 2, y - BAR + 7), s, font=font(False, 16), fill=mv.MUTED)
    for i in range(3):
        cx = x + w - 24 - i * 26
        d.ellipse([cx - 6, y - BAR / 2 - 6, cx + 6, y - BAR / 2 + 6], outline=(90, 90, 88), width=2)
    return im


def side(d, chapter, p):
    x, w = 1460, 396
    d.text((x, 160), "NULL JSFX HUB", font=font(True, 18), fill=mv.MUTED)
    if not chapter:
        return
    title, text = chapter
    a = min(1, p / 0.25)
    col = lambda c: tuple(int(v * a + mv.BG[i] * (1 - a)) for i, v in enumerate(c))  # noqa: E731
    y = 200
    for line in mv.wrap(d, title, font(True, 40), w):
        d.text((x, y), line, font=font(True, 40), fill=col(mv.TEXT))
        y += 52
    y += 16
    for line in mv.wrap(d, text, font(False, 26), w):
        d.text((x, y), line, font=font(False, 26), fill=col(mv.SILVER))
        y += 38


def title_card():
    im = mv.background()
    d = ImageDraw.Draw(im)
    mv.logo(d, W // 2 - 50, 250, 100)
    mv.ctext(d, 380, "NULL JSFX", font(False, 28), mv.MUTED)
    mv.ctext(d, 420, "Hub", font(True, 120), mv.TEXT)
    mv.ctext(d, 590, "Install, update and remove every plugin, in every format", font(False, 40), mv.SILVER)
    mv.ctext(d, 660, "Windows  ·  Linux  ·  macOS", font(False, 30), mv.MUTED)
    return np.asarray(im)


def end_card():
    im = mv.background()
    d = ImageDraw.Draw(im)
    mv.logo(d, W // 2 - 50, 200, 100)
    mv.ctext(d, 330, "NULL JSFX Hub", font(True, 84), mv.TEXT)
    mv.ctext(d, 460, "Free and open source  ·  no account  ·  no tracking", font(False, 34), mv.SILVER)
    mv.ctext(d, 520, "Download it from the latest release", font(False, 30), mv.MUTED)
    mv.ctext(d, 640, "nulljsfx.tech", font(True, 56), mv.TEXT)
    mv.ctext(d, 740, "Support the work: ko-fi.com/koulaxizis", font(False, 30), mv.MUTED)
    return np.asarray(im)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hq", type=Path)
    a = ap.parse_args()
    mv.OUT.mkdir(parents=True, exist_ok=True)
    if a.hq:
        a.hq.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        subprocess.run(["node", str(HERE / "hub_tour.cjs"), str(tmp / "tour")], check=True)
        tour = json.loads((tmp / "tour" / "tour.json").read_text())
        frames, chapters = tour["frames"], tour["chapters"]
        length = frames[-1][1] + 0.5
        total = TITLE + length + END
        n = int(total * SR)

        # the loop as a bed, lower than the plugin videos, faded in and out
        loop = mv.load_loop()
        bed = np.vstack([loop] * (n // len(loop) + 1))[:n]
        t = np.arange(n) / SR
        bed *= (np.clip(t / 2.0, 0, 1) * np.clip((total - t) / 3.0, 0, 1))[:, None]
        mv.master(bed * 0.5, tmp / "bed.f32")
        y = np.fromfile(tmp / "bed.f32", np.float32) * 10 ** (-4 / 20)  # -20 LUFS: a bed, not the show
        y.tofile(tmp / "bed.f32")

        base = mv.background()
        mv.header(ImageDraw.Draw(base))
        win = window(base)
        title, end = title_card(), end_card()
        x, y0, w, h = WIN
        mask = Image.new("L", (w, h), 0)
        ImageDraw.Draw(mask).rounded_rectangle([0, -20, w - 1, h - 1], radius=10, fill=255)
        enc = mv.encoder("hub", tmp / "bed.f32", a.hq)
        times = np.array([f[1] for f in frames])
        still = None
        cache = {}

        def tour_frame(tt):
            k = max(0, int(np.searchsorted(times, tt, side="right") - 1))
            if k not in cache:
                cache.clear()
                cache[k] = Image.open(tmp / "tour" / frames[k][0]).convert("RGB").resize((w, h), Image.LANCZOS)
            im = win.copy()
            im.paste(cache[k], (x, y0), mask)
            c = [c for c in chapters if c["t"] <= tt]
            c = c[-1] if c else None
            side(ImageDraw.Draw(im), (c["title"], c["text"]) if c and c["title"] else None,
                 (tt - c["t"]) if c else 1)
            return np.asarray(im)

        for f in range(int(total * FPS)):
            tt = f / FPS
            if tt < TITLE:
                fr = title
                if tt > TITLE - 0.5:
                    al = (tt - (TITLE - 0.5)) / 0.5
                    fr = (title * (1 - al) + tour_frame(0) * al).astype(np.uint8)
            elif tt < TITLE + length:
                fr = tour_frame(tt - TITLE)
            else:
                al = min(1, (tt - TITLE - length) / 0.5)
                fr = (tour_frame(length) * (1 - al) + end * al).astype(np.uint8)
            if tt < 0.6:
                fr = (fr * (tt / 0.6)).astype(np.uint8)
            if tt > total - 0.8:
                fr = (fr * max(0, (total - tt) / 0.8)).astype(np.uint8)
            if still is None and tt >= TITLE + 22:
                still = fr.copy()
            enc.stdin.write(np.ascontiguousarray(fr).tobytes())
        enc.stdin.close()
        if enc.wait():
            sys.exit("make_hub_video: ffmpeg failed")
        mv.poster(still, "hub")
    print(f"hub: {total:.1f} s, {(mv.OUT / 'hub.mp4').stat().st_size / 1e6:.2f} MB")


if __name__ == "__main__":
    main()
