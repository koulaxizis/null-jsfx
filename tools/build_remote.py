#!/usr/bin/env python3
"""Generates the NULL plugin catalog that every remote shares.

One list, read from the .jsfx headers so no remote can drift from the plugins: per plugin its
file stem, name, native name when it differs, category, tags, formats and every slider (label,
unit, range, step, default, options, log shape, hidden). It goes to two places:
  remote/null_plugins.json                the catalog file; the Android apps carry it as
                                          core/null_plugins.json (android/app/build.gradle.kts),
                                          and their DAW tabs recognise NULL plugins by it
  remote/reaper_www_root/null_jsfx.html   the same list inside the tablet page (NULL_PLUGINS),
                                          which REAPER serves as one file

Usage: build_remote.py           rewrite both
       build_remote.py --check   exit 1 if either is out of date (used by CI)
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
CATALOG = ROOT / "remote/null_plugins.json"
# what the short keys of an entry mean, written into the catalog file
KEYS = {
    "s": "file stem (DATA/Effects/null_jsfx/<s>.jsfx; web/fx/<s>.js)",
    "n": "name (JSFX: NULL <n>; VST3, CLAP, LV2: <n>)",
    "nn": "native name, only when it differs from n",
    "c": "category: one, ess, adv, ast, ins, midi",
    "t": "tags",
    "f": "formats: jsfx, gfx, vst3, clap",
    "sl": "sliders: [label, unit, min, max, step, default, options or null, flags (h hidden, l log, f file)]",
}
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
    entries = [json.dumps(p, ensure_ascii=False, separators=(",", ":")) for p in plugins()]
    data = ",\n".join("  " + e for e in entries)
    page = PAGE.read_text(encoding="utf-8")
    start, end = page.index(BEGIN) + len(BEGIN), page.index(END)
    outputs = {
        PAGE: page[:start] + "\nconst NULL_PLUGINS = [\n" + data + "\n];\n" + page[end:],
        CATALOG: '{\n "generated": "tools/build_remote.py, from the .jsfx headers; do not edit",\n'
                 ' "keys": ' + json.dumps(KEYS, ensure_ascii=False) + ',\n'
                 ' "plugins": [\n' + ",\n".join("  " + e for e in entries) + '\n ]\n}\n',
    }
    stale = [f for f, new in outputs.items() if not f.exists() or f.read_text(encoding="utf-8") != new]
    if "--check" in sys.argv:
        if stale:
            sys.exit("build_remote: out of date, run tools/build_remote.py: " + ", ".join(str(f.relative_to(ROOT)) for f in stale))
        return
    for f in stale:
        f.write_text(outputs[f], encoding="utf-8")
        print(f"build_remote: updated {f.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
