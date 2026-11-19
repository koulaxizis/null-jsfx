#!/usr/bin/env python3
"""The pictures of the chain window on the FX chain pages (tools/chain_pages.py): every plugin of
every chain, in the order REAPER opens it, drawn by tools/gfx_render with the slider values the
generated GFX chain gives it, at twice its @gfx size, as WebP.

Writes assets/img/chains/<slug>/<n>.webp (n from 1) and assets/img/chains/chains.json (what each
chain's pictures were made from, for --check), and removes pictures of chains that are gone.
Usage: chain_shots.py            render every chain
       chain_shots.py --check    exit 1 if a chain page has no picture for one of its plugins, or
                                 the chain's plugins or settings changed since its pictures
       chain_shots.py <slug>...  render only these chains
Run it after tools/build_chains.py changes a chain or a GFX interface changes, then
tools/build_site.py. Needs Pillow with WebP and tools/gfx_render (built if missing).
"""
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import chain_pages  # noqa: E402

RENDER = ROOT / "tests/.build/gfx_render"
GFX = ROOT / "DATA/Effects/null_jsfx/gfx"
QUALITY = 80
MANIFEST = chain_pages.SHOTS / "chains.json"  # {slug: hash of what its pictures show}


def digest(c):
    """What a chain's pictures depend on: its plugins in order and their values."""
    return hashlib.sha256(json.dumps(c["items"]).encode()).hexdigest()[:16]


def render(job):
    slug, n, stem, values = job
    from PIL import Image
    w, h = chain_pages.shot_size(stem)
    out = chain_pages.SHOTS / slug / f"{n}.webp"
    with tempfile.TemporaryDirectory() as tmp:
        png = Path(tmp) / "shot.png"
        subprocess.run([str(RENDER), str(GFX / f"{stem}_gfx.jsfx"), str(png), str(w), str(h),
                        *[f"{v:g}" for v in values]], check=True, stdout=subprocess.DEVNULL)
        Image.open(png).convert("RGB").save(out, "WEBP", quality=QUALITY, method=6)
    return out


def main(args):
    chains = chain_pages.load()
    if "--check" in args:
        missing = [f"{c['slug']}/{n}.webp" for c in chains for n in range(1, len(c["items"]) + 1)
                   if not (chain_pages.SHOTS / c["slug"] / f"{n}.webp").exists()]
        extra = [d.name for d in chain_pages.SHOTS.glob("*") if d.is_dir() and d.name not in {c["slug"] for c in chains}]
        extra += [f"{c['slug']}/{f.name}" for c in chains for f in (chain_pages.SHOTS / c["slug"]).glob("*.webp")
                  if int(f.stem) > len(c["items"])]
        made = json.loads(MANIFEST.read_text(encoding="utf-8")) if MANIFEST.exists() else {}
        missing += [f"{c['slug']} (its settings changed)" for c in chains if made.get(c["slug"]) != digest(c)]
        if missing or extra:
            sys.exit(f"chain_shots: missing {missing[:10]}{' …' if len(missing) > 10 else ''}, extra {extra[:10]}; "
                     "run tools/site_visuals/chain_shots.py")
        print(f"chain_shots: pictures for all {len(chains)} chains")
        return
    if not RENDER.exists():
        subprocess.run([str(ROOT / "tools/gfx_render/build.sh")], check=True)
    only = set(args)
    jobs = []
    for c in chains:
        if only and c["slug"] not in only:
            continue
        d = chain_pages.SHOTS / c["slug"]
        if d.exists():
            shutil.rmtree(d)
        d.mkdir(parents=True)
        jobs += [(c["slug"], n, stem, values) for n, (stem, values) in enumerate(c["items"], 1)]
    if not only:
        for d in chain_pages.SHOTS.glob("*"):
            if d.is_dir() and d.name not in {c["slug"] for c in chains}:
                shutil.rmtree(d)
    with ThreadPoolExecutor() as pool:
        done = list(pool.map(render, jobs))
    made = json.loads(MANIFEST.read_text(encoding="utf-8")) if MANIFEST.exists() and only else {}
    made.update({c["slug"]: digest(c) for c in chains if not only or c["slug"] in only})
    MANIFEST.write_text(json.dumps(dict(sorted(made.items())), indent=1) + "\n", encoding="utf-8")
    size = sum(f.stat().st_size for f in done)
    print(f"chain_shots: {len(done)} pictures for {len({j[0] for j in jobs})} chains, {size / 1e6:.1f} MB")


if __name__ == "__main__":
    main(sys.argv[1:])
