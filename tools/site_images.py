#!/usr/bin/env python3
"""Makes the lighter copies of the large pictures on the home page (index.html): WebP, at most
960 px wide, so a phone downloads a fraction of the bytes. The sources stay as they are (other
tools make them: the 3D renders on Christos's PC, the video posters in tools/video/, the remote
pictures by hand) and keep serving the other pages, the social cards and the 2x screens.

Run it again whenever a source below changes.

Usage: site_images.py           write the copies
       site_images.py --check   exit 1 if a copy is missing or its source changed since (CI; needs no Pillow)

tools/site_images.json keeps the SHA-256 of each source the copies were made from.
"""
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MANIFEST = ROOT / "tools/site_images.json"
WIDTH = 960
QUALITY = 80

# (source, copy)
IMAGES = [
    ("assets/hardware/family.webp", "assets/hardware/family-960.webp"),
    ("assets/video/hub.jpg", "assets/video/hub-960.webp"),
    ("assets/video/rack.jpg", "assets/video/rack-960.webp"),
    ("assets/video/trailer.jpg", "assets/video/trailer-960.webp"),
    ("assets/img/remote/remote-for-daws.jpg", "assets/img/remote/remote-for-daws-960.webp"),
    ("assets/img/remote/remote-for-mixers.jpg", "assets/img/remote/remote-for-mixers-960.webp"),
    ("assets/img/remote/oros-studio-remote.jpg", "assets/img/remote/oros-studio-remote-960.webp"),
]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def make(src, dst):
    from PIL import Image  # only to write the copies; --check runs without it
    with Image.open(src) as im:
        im = im.convert("RGB")
        if im.width > WIDTH:
            im = im.resize((WIDTH, round(im.height * WIDTH / im.width)), Image.LANCZOS)
        im.save(dst, "WEBP", quality=QUALITY, method=6)


def main(check):
    if check:
        made = json.loads(MANIFEST.read_text(encoding="utf-8")) if MANIFEST.exists() else {}
        stale = [d for s, d in IMAGES if not (ROOT / d).exists() or made.get(d) != sha(ROOT / s)]
        if stale:
            sys.exit(f"site_images: {', '.join(stale)} out of date; run tools/site_images.py")
        print(f"site_images: all {len(IMAGES)} copies up to date")
        return
    made = {}
    for s, d in IMAGES:
        make(ROOT / s, ROOT / d)
        made[d] = sha(ROOT / s)
        print(f"site_images: {d} {(ROOT / d).stat().st_size // 1024} KB (source {(ROOT / s).stat().st_size // 1024} KB)")
    MANIFEST.write_text(json.dumps(made, indent=1) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main("--check" in sys.argv[1:])
