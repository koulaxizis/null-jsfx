#!/usr/bin/env python3
"""Generates the plugin list of the tablet remote (remote/reaper_www_root/null_jsfx.html).

The page recognises NULL JSFX plugins on a track and draws their controls from this list:
name, category, formats and every slider (label, unit, range, step, default, options,
log shape, hidden), all read from the .jsfx headers, so the remote can never drift from
the plugins.

Usage: build_remote.py           rewrite the generated block
       build_remote.py --check   exit 1 if the page is out of date (used by CI)
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PLUGINS = ROOT / "DATA/Effects/null_jsfx"
GFX = PLUGINS / "gfx"
NATIVE = ROOT / "native/plugins"
PAGE = ROOT / "remote/reaper_www_root/null_jsfx.html"
BEGIN, END = "/* BEGIN GENERATED PLUGINS (tools/build_remote.py) */", "/* END GENERATED PLUGINS */"
CATEGORIES = {"One Slider": "one", "Essentials": "ess", "Advanced": "adv", "Assisted": "ast", "Instruments": "ins", "MIDI": "midi"}


def num(v):
    v = float(v)
    return int(v) if v == int(v) else v


def slider(body):
    """[label, unit, min, max, step, default, options, flags] of one slider line."""
    if body.startswith("/"):  # file slider: /folder:default:Label
        return [body.split(":")[-1], "", 0, 0, 0, 0, None, "f"]
    default, rng, label = re.match(r"([^<]*)<([^>]*)>(.*)", body).groups()
    flags = ""
    if label.startswith("-"):
        flags += "h"
        label = label[1:]
    unit = re.search(r"\(([^)]*)\)\s*$", label)
    name = re.sub(r"\s*\([^)]*\)\s*$", "", label).strip()
    options = None
    m = re.search(r"\{(.*)\}", rng)
    if m:
        options = [o.strip() for o in m.group(1).split(",")]
        rng = rng[: m.start()]
    parts = [p.strip() for p in rng.split(",")]
    lo, hi = num(parts[0]), num(parts[1])
    step = 0
    if len(parts) > 2 and parts[2]:
        shape = parts[2].split(":")
        step = num(shape[0]) if shape[0] else 0
        if len(shape) > 1 and shape[1].startswith("log"):
            flags += "l"
    return [name, unit.group(1) if unit else "", lo, hi, step, num(default), options, flags]


def native_name(stem):
    info = NATIVE / stem / "DistrhoPluginInfo.h"
    if not info.exists():
        return None
    return re.search(r'DISTRHO_PLUGIN_NAME\s+"([^"]*)"', info.read_text(encoding="utf-8")).group(1)


def plugins():
    out = []
    for path in sorted(PLUGINS.glob("*.jsfx")):
        lines = path.read_text(encoding="utf-8").split("\n")
        name = lines[0][len("desc:NULL "):].strip()
        formats = ["jsfx"]
        if (GFX / f"{path.stem}_gfx.jsfx").exists():
            formats.append("gfx")
        native = native_name(path.stem)
        if native:
            formats += ["vst3", "clap"]
        p = {
            "s": path.stem,
            "n": name,
            "c": CATEGORIES[lines[3].split(": ", 1)[1].strip()],
            "t": lines[2].split(":", 1)[1].strip(),
            "f": formats,
            "sl": [slider(m.group(1)) for m in re.finditer(r"^slider\d+:(.*)$", "\n".join(lines), re.M)],
        }
        if native and native != name:
            p["nn"] = native
        out.append(p)
    return out


def main():
    page = PAGE.read_text(encoding="utf-8")
    start, end = page.index(BEGIN) + len(BEGIN), page.index(END)
    data = ",\n".join("  " + json.dumps(p, ensure_ascii=False, separators=(",", ":")) for p in plugins())
    new = page[:start] + "\nconst NULL_PLUGINS = [\n" + data + "\n];\n" + page[end:]
    if "--check" in sys.argv:
        if new != page:
            sys.exit(f"build_remote: {PAGE.relative_to(ROOT)} is out of date, run tools/build_remote.py")
        return
    if new != page:
        PAGE.write_text(new, encoding="utf-8")
        print(f"build_remote: updated {PAGE.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
