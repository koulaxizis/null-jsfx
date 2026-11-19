#!/usr/bin/env python3
"""Puts the Rack and Hub shots into the site for the /rack/ and /hub/ pages (tools/product_pages.py).

Reads what rack_shots.py and hub_shots.cjs wrote (rack-<name>.png, hub-<name>.png and, with
--clips, rack-<name>.mp4 and hub-<name>.mp4) and writes only what the pages show:
  assets/img/{rack,hub}/<name>.webp      the shot as taken (2x)
  assets/img/{rack,hub}/<name>-1x.webp   half size, for 1x screens
  assets/video/site/<kind>-<name>.mp4    the loop, copied as it is (already H.264, no audio)
  assets/video/site/<kind>-<name>.webp   its first frame, the poster
Usage: publish.py <rack shots dir> <hub shots dir>
Then run tools/build_site.py. Needs Pillow with WebP and ffmpeg.
"""
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
IMG = ROOT / "assets/img"
CLIPS = ROOT / "assets/video/site"
QUALITY = 80
PICTURES = {"rack": ["mastering", "bands", "lanes"], "hub": ["plugins", "detail", "extras", "settings"]}
LOOPS = {"rack": ["macro", "lfo", "sequencer"], "hub": ["search", "install", "profile"]}


def webp(src, out, half=False):
    from PIL import Image
    im = Image.open(src).convert("RGB")
    if half:
        im = im.resize((im.width // 2, im.height // 2), Image.LANCZOS)
    im.save(out, "WEBP", quality=QUALITY, method=6)


def main(args):
    if len(args) != 2:
        sys.exit(__doc__)
    dirs = dict(zip(("rack", "hub"), map(Path, args)))
    CLIPS.mkdir(parents=True, exist_ok=True)
    for kind, names in PICTURES.items():
        (IMG / kind).mkdir(parents=True, exist_ok=True)
        for name in names:
            src = dirs[kind] / f"{kind}-{name}.png"
            webp(src, IMG / kind / f"{name}.webp")
            webp(src, IMG / kind / f"{name}-1x.webp", half=True)
            print(f"assets/img/{kind}/{name}.webp")
    for kind, names in LOOPS.items():
        for name in names:
            src = dirs[kind] / f"{kind}-{name}.mp4"
            shutil.copyfile(src, CLIPS / f"{kind}-{name}.mp4")
            with tempfile.TemporaryDirectory() as tmp:
                png = Path(tmp) / "first.png"
                subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(src), "-frames:v", "1", str(png)], check=True)
                webp(png, CLIPS / f"{kind}-{name}.webp")
            print(f"assets/video/site/{kind}-{name}.mp4")


if __name__ == "__main__":
    main(sys.argv[1:])
