#!/usr/bin/env python3
"""Puts the shots of the Rack, the Hub and the remotes into the site (tools/product_pages.py: /rack/, /hub/, /remotes/ and the homepage section "One plugin, every form").

Reads what rack_shots.py and hub_shots.cjs wrote (rack-<name>.png, hub-<name>.png and, with
--clips, rack-<name>.mp4 and hub-<name>.mp4) and writes only what the pages show:
  assets/img/{rack,hub}/<name>.webp      the shot as taken (2x)
  assets/img/{rack,hub}/<name>-1x.webp   half size, for 1x screens
  assets/video/site/<kind>-<name>.mp4    the loop, copied as it is (already H.264, no audio)
  assets/video/site/<kind>-<name>.webp   its first frame, the poster
  assets/img/remotes/<name>.webp         the remotes' screens (/remotes/), as the app tests took them
  assets/img/forms/<name>.webp (+ -1x)   Delay in each of its forms (the homepage)
Usage: publish.py <rack shots dir> <hub shots dir> [<remote shots dir>]
The remote shots dir holds daws/ and mixers/ from `node tests/daws/ui_test.cjs <dir>/daws` and
`node tests/mixers/ui_test.cjs <dir>/mixers`, and delay-native.png from
`native/scripts/screenshot.sh delay <dir>/delay-native.png`.
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
# assets/img/remotes/<name>.webp: (shot in the remote shots dir)
REMOTES = {"daws-desk": "daws/ardour-desk.png", "daws-plugin": "daws/ardour-delay.png", "daws-phone": "daws/bitwig-desk-phone.png",
           "daws-mackie": "daws/mackie-desk.png", "daws-sends": "daws/bitwig-sends.png",
           "mixers-desk": "mixers/mixers-desk-tablet.png", "mixers-mymix": "mixers/mixers-mymix-phone.png",
           "mixers-connect": "mixers/mixers-connect-phone.png", "mixers-wing": "mixers/mixers-wing-tablet.png",
           "mixers-sq": "mixers/mixers-sq-tablet.png", "mixers-yamaha": "mixers/mixers-yamaha-tablet.png"}
LOOPS = {"rack": ["macro", "lfo", "sequencer"], "hub": ["search", "install", "profile"]}


def webp(src, out, half=False):
    from PIL import Image
    im = Image.open(src).convert("RGB")
    if half:
        im = im.resize((im.width // 2, im.height // 2), Image.LANCZOS)
    im.save(out, "WEBP", quality=QUALITY, method=6)


def main(args):
    if len(args) not in (2, 3):
        sys.exit(__doc__)
    dirs = dict(zip(("rack", "hub", "remote"), map(Path, args)))
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
    if "remote" in dirs:
        for folder in ("remotes", "forms"):
            (IMG / folder).mkdir(parents=True, exist_ok=True)
        for name, shot in REMOTES.items():
            webp(dirs["remote"] / shot, IMG / "remotes" / f"{name}.webp")
        # Delay in every form: the GFX picture is assets/img/gfx/delay.png, the remote one daws-plugin
        webp(dirs["remote"] / "delay-native.png", IMG / "forms" / "delay-native.webp")
        webp(dirs["rack"] / "rack-delay.png", IMG / "forms" / "delay-rack.webp")
        webp(dirs["rack"] / "rack-delay.png", IMG / "forms" / "delay-rack-1x.webp", half=True)
        print(f"assets/img/remotes/ ({len(REMOTES)}), assets/img/forms/")


if __name__ == "__main__":
    main(sys.argv[1:])
