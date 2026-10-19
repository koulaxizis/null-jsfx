#!/usr/bin/env python3
"""Draws assets/img/social-preview.jpg (1200 x 630), the image link previews show for the site:
the logo, the plugin count from tools/site_content.json, the formats, and three GFX interfaces.
Run it again when the count changes. Needs Pillow and numpy (as tools/video does).
"""
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/video"))
import make_videos as mv  # noqa: E402

W, H = 1200, 630
SHOTS = ["parametric_eq", "drum_machine", "delay"]  # back to front


def main():
    n = len(json.loads((ROOT / "tools/site_content.json").read_text(encoding="utf-8")))
    y, x = np.mgrid[0:H, 0:W]
    a = np.clip(1 - np.sqrt((x - W * 0.75) ** 2 + (y - H * 0.5) ** 2) / 900, 0, 1) ** 2
    arr = np.array(mv.BG, float) * (1 - a[..., None]) + np.array((44, 44, 46), float) * a[..., None]
    im = Image.fromarray(np.clip(np.round(arr), 0, 255).astype("uint8"))

    # the interfaces, fanned out on the right, each with a soft shadow
    for i, stem in enumerate(SHOTS):
        shot = Image.open(ROOT / f"assets/img/gfx/{stem}.png").convert("RGB")
        w = 520
        shot = shot.resize((w, round(shot.height * w / shot.width)), Image.LANCZOS)
        px, py = 640 + i * 70, 70 + i * 95
        shadow = Image.new("L", (shot.width + 60, shot.height + 60), 0)
        ImageDraw.Draw(shadow).rectangle([30, 30, 30 + shot.width, 30 + shot.height], fill=150)
        im.paste((0, 0, 0), (px - 30 + 8, py - 30 + 12), shadow.filter(ImageFilter.GaussianBlur(14)))
        im.paste(shot, (px, py))
        ImageDraw.Draw(im).rectangle([px, py, px + shot.width - 1, py + shot.height - 1], outline=mv.BORDER)

    d = ImageDraw.Draw(im)
    mv.logo(d, 64, 64, 64)
    mv.spaced(d, (148, 66), "NULL JSFX", mv.font(True, 32), (220, 220, 220), 6)
    mv.spaced(d, (150, 110), "AUDIO WITHOUT NOISE", mv.font(False, 14), mv.MUTED, 3.6)
    d.text((64, 210), f"{n} free", font=mv.font(True, 76), fill=mv.TEXT)
    d.text((64, 300), "audio plugins", font=mv.font(True, 76), fill=mv.TEXT)
    d.text((66, 410), "Effects · Instruments · MIDI tools", font=mv.font(False, 28), fill=mv.SILVER)
    d.text((66, 456), "REAPER (JSFX) · VST3 · CLAP", font=mv.font(False, 24), fill=mv.MUTED)
    d.text((66, 492), "Windows · macOS · Linux · open source", font=mv.font(False, 24), fill=mv.MUTED)
    d.text((66, 560), "nulljsfx.tech", font=mv.font(True, 22), fill=mv.SILVER)
    out = ROOT / "assets/img/social-preview.jpg"
    im.save(out, quality=88, optimize=True, progressive=True)
    print(f"social preview: {out.relative_to(ROOT)} ({n} plugins)")


if __name__ == "__main__":
    main()
