#!/usr/bin/env python3
"""Flags presentation media that no longer shows a plugin's current interface.

For every plugin on the site (tools/site_content.json) it compares, in the git history, the last
change to its GFX file (DATA/Effects/null_jsfx/gfx/<stem>_gfx.jsfx) with the last change to its
video (assets/video/<stem>.mp4), the video's poster (.jpg) and its screenshot
(assets/img/gfx/<stem>.png). A change that only touches the version footer (line 5, Bible VI.1)
does not count. Media older than the GFX is listed: render it again (Bible IV, step 5).

The shared library null_gfx.jsfx-inc is reported on its own line: a change there can change every
interface, so after one, look at the videos, but they are not all listed.

    python3 tools/media_check.py            list stale and missing media, as GitHub warnings in CI
    python3 tools/media_check.py --strict   the same, exit code 1 when any media is stale

Needs the full git history (actions/checkout with fetch-depth: 0).
"""
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CONTENT = ROOT / "tools" / "site_content.json"
GFX = "DATA/Effects/null_jsfx/gfx"
FOOTER = re.compile(r"^[+-]//NULL JSFX v[0-9.]+ ")


def git(*args):
    return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True, check=True).stdout


def last_change(path, visual=False):
    """Unix time of the newest commit that changed path (with visual: not only its footer), or 0."""
    for line in git("log", "--format=%H %ct", "--", path).splitlines():
        sha, when = line.split()
        if not visual:
            return int(when)
        diff = git("show", "--format=", "-U0", sha, "--", path).splitlines()
        changed = [d for d in diff if d[:1] in "+-" and not d.startswith(("+++", "---"))]
        if any(not FOOTER.match(d) for d in changed):
            return int(when)
    return 0


def day(t):
    return time.strftime("%Y-%m-%d", time.gmtime(t)) if t else "never"


def main(strict):
    if git("rev-parse", "--is-shallow-repository").strip() == "true":
        sys.exit("media_check: the clone is shallow, the dates would be wrong (fetch-depth: 0)")
    ci = bool(os.environ.get("GITHUB_ACTIONS"))
    stems = sorted(json.loads(CONTENT.read_text(encoding="utf-8")))
    stale, missing, newest_media = [], [], 0
    for stem in stems:
        gfx = f"{GFX}/{stem}_gfx.jsfx"
        if not (ROOT / gfx).exists():
            continue
        media = [m for m in (f"assets/video/{stem}.mp4", f"assets/video/{stem}.jpg", f"assets/img/gfx/{stem}.png")]
        absent = [m for m in media if not (ROOT / m).exists()]
        if absent:
            missing.append((stem, absent))
        present = [m for m in media if (ROOT / m).exists()]
        if not present:
            continue
        changed = last_change(gfx, visual=True)
        times = {m: last_change(m) for m in present}
        newest_media = max(newest_media, *times.values())
        old = [m for m, t in times.items() if t < changed]
        if old:
            stale.append((stem, changed, [(m, times[m]) for m in old]))
    for stem, changed, old in stale:
        what = ", ".join(f"{m} ({day(t)})" for m, t in old)
        msg = f"{stem}: interface changed {day(changed)}, older media: {what}"
        print(f"::warning title=media_check::{msg}" if ci else f"stale   {msg}")
    for stem, absent in missing:
        print(f"missing {stem}: {', '.join(absent)}")
    shared = last_change(f"{GFX}/null_gfx.jsfx-inc", visual=True)
    print(f"null_gfx.jsfx-inc last changed {day(shared)}"
          + (": newer than every video and screenshot, look at them" if shared > newest_media else ""))
    print(f"media_check: {len(stems)} plugins, {len(stale)} with stale media, {len(missing)} with missing media")
    if stale and strict:
        sys.exit(1)


if __name__ == "__main__":
    main("--strict" in sys.argv[1:])
