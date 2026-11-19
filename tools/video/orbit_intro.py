#!/usr/bin/env python3
"""Puts the 4 s orbit of a plugin's 3D render (assets/hardware/<plugin>.mp4) in front of its
presentation video (assets/video/<plugin>.mp4), fading into the title card over 0.5 s.

The orbit has no sound: the video's own sound starts with the title card, as before. The result
is tagged (comment=orbit-intro), so running this again leaves a video that has its intro alone;
make_videos.py calls it after every render, so new and re-rendered videos get the intro too.
A plugin without an orbit is left as it is. The poster (<plugin>.jpg) does not change.

Usage: orbit_intro.py [--jobs N] [plugin ...]     (default: every video that has an orbit)
Needs ffmpeg and ffprobe.
"""
import argparse
import json
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
VIDEOS = ROOT / "assets/video"
ORBITS = ROOT / "assets/hardware"
TAG = "orbit-intro"
ORBIT_S, FADE_S = 4.0, 0.5


def has_intro(path):
    r = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format_tags=comment", "-of", "json", str(path)],
                       capture_output=True, text=True, check=True)
    return json.loads(r.stdout).get("format", {}).get("tags", {}).get("comment") == TAG


def prepend(stem):
    """Adds the intro to one video; returns what happened, for the log."""
    video, orbit = VIDEOS / f"{stem}.mp4", ORBITS / f"{stem}.mp4"
    if not video.is_file():
        return f"{stem}: no presentation video"
    if not orbit.is_file():
        return f"{stem}: no orbit, left as is"
    if has_intro(video):
        return f"{stem}: has its intro already"
    offset = ORBIT_S - FADE_S
    tmp = video.with_suffix(".intro.mp4")
    v = "fps=30,scale=1280:720:flags=lanczos,setsar=1,format=yuv420p"
    graph = (f"[1:v]trim=0:{ORBIT_S},setpts=PTS-STARTPTS,{v}[o];[0:v]{v}[m];"
             f"[o][m]xfade=transition=fade:duration={FADE_S}:offset={offset}[v];"
             f"[0:a]adelay={int(offset * 1000)}:all=1[a]")
    # the same encoder settings as make_videos.py's 720p file
    cmd = ["ffmpeg", "-y", "-loglevel", "error", "-i", str(video), "-i", str(orbit), "-filter_complex", graph,
           "-map", "[v]", "-map", "[a]", "-c:v", "libx264", "-preset", "slow", "-crf", "28", "-tune", "animation",
           "-g", "60", "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "96k", "-ac", "2", "-ar", "48000",
           "-metadata", f"comment={TAG}", "-movflags", "+faststart", str(tmp)]
    try:
        subprocess.run(cmd, check=True)
        os.replace(tmp, video)
    finally:
        tmp.unlink(missing_ok=True)
    return f"{stem}: intro added, {video.stat().st_size / 1e6:.2f} MB"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("plugins", nargs="*")
    ap.add_argument("--jobs", type=int, default=2)
    a = ap.parse_args()
    names = a.plugins or sorted(p.stem for p in ORBITS.glob("*.mp4") if (VIDEOS / p.name).is_file())
    with ThreadPoolExecutor(a.jobs) as ex:
        for line in ex.map(prepend, names):
            print(line, flush=True)


if __name__ == "__main__":
    sys.exit(main())
