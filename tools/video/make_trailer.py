#!/usr/bin/env python3
"""The suite trailer, assets/video/trailer.mp4 (about 40 s) and its poster trailer.jpg, cut from the
presentation videos that tools/video already makes, so it never shows anything that is not real:

  title card -> eight plugins, each at its "after" moment -> the Rack -> the Hub -> the remotes
  (a slow zoom on a screen the app tests took, assets/img/remotes/) -> end card

Each plugin keeps its own sound; the cards and the remotes play the site's loop. The whole is then
brought to about -16 LUFS like the other videos (tools/video/README.md). 1280x720, 30 fps, H.264 +
AAC, faststart. media.yml puts it on the "media" release with the other videos.
Usage: make_trailer.py [--hq <dir>]   (--hq also writes a 1920x1080 copy there, not checked in)
Run tools/build_site.py afterwards. Needs ffmpeg with drawtext, zoompan and loudnorm.
"""
import argparse
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
VIDEO = ROOT / "assets/video"
LOOP = ROOT / "assets/audio/null-jsfx-loop.flac"
FONT = ROOT / "native/common/fonts/NotoSans-Bold.ttf"
FONT_R = ROOT / "native/common/fonts/NotoSans-Regular.ttf"
BG = "0x1b1a18"
SILVER = "0xc4c4c4"
MUTED = "0x999999"
FPS = 30

# (video stem, start s, length s): each plugin video shows its first factory preset "after" from about 13 to 17.5 s
PLUGINS = [("delay", 14.5, 2.4), ("saturation", 14.5, 2.4), ("shimmer_reverb", 14.5, 2.4), ("parametric_eq", 14.5, 2.4),
           ("drum_machine", 14.5, 2.4), ("ott", 14.5, 2.4), ("spectral_freeze", 14.5, 2.4), ("vocoder", 14.5, 2.4)]
RACK = ("rack", 19.5, 4.0)  # Mastering Chain, after
HUB = ("hub", 5.0, 4.0)  # every plugin, one place
REMOTE = ROOT / "assets/img/remotes/mixers-desk.webp"


def site_plugins():
    """{stem: name} of the plugins on the site (tools/build_site.py): the end card counts them."""
    import sys
    sys.path.insert(0, str(ROOT / "tools"))
    import build_site
    return {p["stem"]: p["name"] for p in build_site.load()}


def run(*args):
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", *map(str, args)], check=True)


def esc(text):
    return text.replace("\\", "\\\\").replace(":", "\\:").replace("'", "’")


def card(out, seconds, lines, loop_at):
    """A dark card with centred lines [(text, size, colour, font, y offset)], fading in and out, over the loop."""
    draws = ",".join(f"drawtext=fontfile={f}:text='{esc(t)}':fontsize={s}:fontcolor={c}:x=(w-text_w)/2:y=(h-text_h)/2+{y}"
                     for t, s, c, f, y in lines)
    fade = f"fade=in:st=0:d=0.4,fade=out:st={seconds - 0.4}:d=0.4"
    run("-f", "lavfi", "-i", f"color=c={BG}:s=1280x720:r={FPS}:d={seconds}", "-ss", loop_at, "-t", seconds, "-i", LOOP,
        "-vf", f"{draws},{fade},format=yuv420p", "-af", f"afade=in:d=0.3,afade=out:st={seconds - 0.4}:d=0.4,aformat=sample_rates=48000:channel_layouts=stereo",
        "-c:v", "libx264", "-preset", "slow", "-crf", "20", "-c:a", "aac", "-b:a", "160k", "-shortest", out)


def cut(out, stem, start, seconds):
    """seconds of a presentation video from start (it names itself on screen), with short fades."""
    run("-ss", start, "-t", seconds, "-i", VIDEO / f"{stem}.mp4",
        "-vf", f"scale=1280:720,fps={FPS},fade=in:st=0:d=0.15,fade=out:st={seconds - 0.15}:d=0.15,format=yuv420p",
        "-af", f"afade=in:d=0.05,afade=out:st={seconds - 0.08}:d=0.08,aformat=sample_rates=48000:channel_layouts=stereo",
        "-c:v", "libx264", "-preset", "slow", "-crf", "20", "-c:a", "aac", "-b:a", "160k", out)


def still(out, picture, seconds, label, loop_at):
    """A slow zoom on a picture, over the loop."""
    frames = int(seconds * FPS)
    caption = (f"drawbox=x=0:y=ih-56:w=iw:h=56:color=black@0.55:t=fill,"
               f"drawtext=fontfile={FONT}:text='{esc(label)}':fontsize=22:fontcolor={SILVER}:x=32:y=h-40")
    run("-loop", "1", "-i", picture, "-ss", loop_at, "-t", seconds, "-i", LOOP,
        "-vf", f"scale=2400:1350:force_original_aspect_ratio=decrease,pad=2560:1440:(ow-iw)/2:(oh-ih)/2:color={BG},"
               f"zoompan=z='1+0.08*on/{frames}':x='iw/2-iw/zoom/2':y='ih/2-ih/zoom/2':d={frames}:s=1280x720:fps={FPS},"
               f"{caption},fade=in:st=0:d=0.15,fade=out:st={seconds - 0.2}:d=0.2,format=yuv420p",
        "-af", f"afade=in:d=0.1,afade=out:st={seconds - 0.2}:d=0.2,aformat=sample_rates=48000:channel_layouts=stereo",
        "-t", seconds, "-c:v", "libx264", "-preset", "slow", "-crf", "20", "-c:a", "aac", "-b:a", "160k", out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hq")
    a = ap.parse_args()
    shown = site_plugins()
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        parts = []

        def part():
            p = tmp / f"{len(parts):02d}.mp4"
            parts.append(p)
            return p
        card(part(), 3.0, [("NULL JSFX", 96, SILVER, FONT, -30), ("AUDIO WITHOUT NOISE", 22, MUTED, FONT_R, 50)], 0)
        for stem, start, sec in PLUGINS:
            cut(part(), stem, start, sec)
        cut(part(), *RACK)
        cut(part(), *HUB)
        still(part(), REMOTE, 3.0, "Remotes for your DAW and your mixer", 8)
        card(part(), 4.0, [(f"{len(shown)} free plugins", 64, SILVER, FONT, -60),
                           ("JSFX · GFX · VST3 · CLAP · LV2 · Rack · Hub · Remotes", 26, MUTED, FONT_R, 20),
                           ("nulljsfx.tech", 34, SILVER, FONT, 80)], 11)
        lst = tmp / "list.txt"
        lst.write_text("".join(f"file '{p}'\n" for p in parts))
        joined = tmp / "joined.mp4"
        run("-f", "concat", "-safe", "0", "-i", lst, "-c", "copy", joined)
        out = VIDEO / "trailer.mp4"
        run("-i", joined, "-c:v", "libx264", "-preset", "slow", "-crf", "24", "-movflags", "+faststart",
            "-af", "loudnorm=I=-16:TP=-1.5:LRA=11", "-c:a", "aac", "-b:a", "160k", "-ar", "48000", out)
        run("-ss", "1.5", "-i", out, "-frames:v", "1", "-q:v", "3", VIDEO / "trailer.jpg")
        if a.hq:
            Path(a.hq).mkdir(parents=True, exist_ok=True)
            run("-i", joined, "-vf", "scale=1920:1080:flags=lanczos", "-c:v", "libx264", "-preset", "slow", "-crf", "18",
                "-movflags", "+faststart", "-af", "loudnorm=I=-16:TP=-1.5:LRA=11", "-c:a", "aac", "-b:a", "192k",
                Path(a.hq) / "trailer.mp4")
    print(f"assets/video/trailer.mp4 ({len(parts)} parts)")


if __name__ == "__main__":
    main()
