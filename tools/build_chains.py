#!/usr/bin/env python3
"""Generates the REAPER FX chains in DATA/FXChains/ from the CHAINS list below, and the Rack's
factory presets (native/plugins/rack/RackPresets.hpp) from the same CHAINS plus RACK_PRESETS.

Every chain is a serial chain of NULL JSFX plugins with chosen slider values. It is written
twice: with the plain JSFX (FXChains/NULL JSFX/<Chain>.RfxChain) and with the GFX versions
(FXChains/NULL JSFX GFX/<Chain>.RfxChain). Values are given by slider name (the label without
its unit), enum sliders by option label; anything not given keeps the plugin's default. Names,
ranges and options are checked against the .jsfx headers, so a chain can never point at a
slider that does not exist or hold a value out of range.

The Rack (the VST3/CLAP host of the suite's modules) gets every chain as a preset with the same
modules and settings, plus the RACK_PRESETS that use what REAPER chains cannot: per-slot bands
and channels, LFOs, the envelope and macros. Slider names resolve to indices here, from the
.jsfx headers (the modules keep the JSFX slider order), so RackPresets.hpp only holds numbers.

Usage: build_chains.py           write the chains and RackPresets.hpp
       build_chains.py --check   exit 1 if any of them is out of date (used by CI)
"""
import re
import sys
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PLUGINS = ROOT / "DATA/Effects/null_jsfx"
OUT = ROOT / "DATA/FXChains"
RACK = ROOT / "native/plugins/rack"
RACK_PRESETS_H = RACK / "RackPresets.hpp"
FOLDERS = {"plain": "NULL JSFX", "gfx": "NULL JSFX GFX"}

# (chain name, [(plugin file stem, {slider name: value}), ...])
CHAINS = [
    ("Tube EQ", [
        ("parametric_eq", {"Low Freq": 60, "Low Gain": 3, "Low Type": "Low Shelf",
                           "Low Mid Freq": 400, "Low Mid Gain": -1.5, "Low Mid Q": 0.8,
                           "High Freq": 10000, "High Gain": 2.5, "High Type": "High Shelf"}),
        ("saturation", {"Drive": 25, "Tone": 0, "Mix": 60, "Mode": "Tube"}),
    ]),
    ("Reverse Reverb", [
        ("reverb", {"Room Size": 85, "Decay": 80, "Damping": 40, "Mix": 50}),
        ("reverser", {"Buffer Length": 2, "Mix": 30}),
    ]),
    ("Lo-Fi", [
        ("grit", {"Grit Intensity": 35}),
        ("vinyl_age", {"Wow": 25, "Flutter": 15, "Crackle": 30, "Noise Level": 10, "Bit Depth": 12,
                       "Sample Rate": 22050, "Lowpass Freq": 7000, "Mix": 85}),
        ("filter", {"Cutoff": 6500, "Resonance": 0.8, "Mode": "Low Pass"}),
    ]),
    ("Cassette", [
        ("tape", {"Drive": 6, "Speed": "7.5 ips", "Head Bump": 4, "Wow": 40, "Flutter": 35, "Hiss": 25}),
        ("filter", {"Cutoff": 9000, "Resonance": 0.7, "Mode": "Low Pass"}),
    ]),
    ("Lo-Fi Echo", [
        ("delay", {"Time": 375, "Feedback": 45, "Mix": 35, "Low Cut": 300, "High Cut": 4000, "Drive": 40}),
        ("grit", {"Grit Intensity": 40}),
    ]),
    ("Bass Enhancer", [
        ("sub_bloom", {"Source Frequency": 90, "Sub Level": 40, "Sub Octave": "-1 Octave", "Original Mix": 90}),
        ("saturation", {"Drive": 20, "Mix": 40, "Mode": "Tape"}),
        ("mono_maker", {"Amount": 60}),
    ]),
    ("Drum Bus", [
        ("transient_shaper", {"Attack": 4, "Sustain": -2, "Mix": 100}),
        ("saturation", {"Drive": 25, "Mix": 50, "Mode": "Tape"}),
        ("dynamics", {"Mode": "Compressor", "Threshold": -18, "Ratio": 2.5, "Knee": 6, "Attack": 30,
                      "Release": 150, "Makeup": 2}),
    ]),
    ("Ensemble", [
        ("chorus", {"Rate": 0.8, "Depth": 4, "Mix": 40}),
        ("doubler", {"Amount": 40}),
        ("spread", {"Amount": 50}),
    ]),
    ("Guitar Amp", [
        ("gate", {"Threshold": -55, "Attack": 2, "Release": 120, "Range": -60}),
        ("amp_simulator", {"Model": "Crunch", "Gain": 6, "Bass": 5, "Mid": 6, "Treble": 6, "Presence": 5, "Output": -6}),
        ("cabinet_simulator", {"Cabinet": "4x12 Closed Back", "Mic Position": 30, "Low Cut": 80}),
        ("reverb", {"Room Size": 30, "Decay": 30, "Mix": 12}),
    ]),
    ("Bass Amp", [
        ("amp_simulator", {"Model": "Bass", "Gain": 4, "Bass": 6, "Mid": 5, "Treble": 4, "Presence": 4}),
        ("cabinet_simulator", {"Cabinet": "1x15 Bass", "Mic Position": 25, "Low Cut": 40}),
        ("dynamics", {"Mode": "Compressor", "Threshold": -20, "Ratio": 4, "Attack": 15, "Release": 120,
                      "Makeup": 3}),
    ]),
    ("Vocal Chain", [
        ("gate", {"Threshold": -50, "Attack": 5, "Release": 200, "Range": -20}),
        ("breath_control", {"Threshold": -40, "Range": -10}),
        ("deesser", {"Target": 6500, "Reduction": -6}),
        ("dynamics", {"Mode": "Compressor", "Threshold": -22, "Ratio": 3, "Knee": 6, "Attack": 5,
                      "Release": 120, "Makeup": 7}),
        ("parametric_eq", {"Low Freq": 80, "Low Type": "High Pass", "Low Q": 0.7,
                           "Low Mid Freq": 350, "Low Mid Gain": -2, "Low Mid Q": 1,
                           "Mid High Freq": 3000, "Mid High Gain": 1.5, "Mid High Q": 1,
                           "High Freq": 12000, "High Gain": 2, "High Type": "High Shelf"}),
        ("doubler", {"Amount": 25}),
        ("reverb", {"Room Size": 50, "Decay": 45, "Damping": 40, "Mix": 15}),
    ]),
    ("Dialogue Cleanup", [
        ("de_hum", {"Frequency": "50 Hz", "Harmonics": 4, "Depth": 30, "Low Cut": "40 Hz"}),
        ("noise_reduction", {"Auto": "On", "Reduction": 12, "Threshold": 3}),
        ("deesser", {"Target": 6000, "Reduction": -4}),
        ("auto_leveler", {"Target": -18, "Speed": 1000, "Max Boost": 9, "Max Cut": 9}),
        ("limiter", {"Mode": "Limiter", "Ceiling": -1}),
    ]),
    ("Mastering Chain", [
        ("parametric_eq", {"Low Freq": 30, "Low Type": "High Pass", "Low Q": 0.7,
                           "High Freq": 12000, "High Gain": 1, "High Type": "High Shelf"}),
        ("multiband_compressor", {"Low Threshold": -18, "Low Ratio": 2, "LowMid Threshold": -18,
                                  "LowMid Ratio": 1.5, "HighMid Threshold": -18, "HighMid Ratio": 1.5,
                                  "High Threshold": -18, "High Ratio": 2, "Attack": 20, "Release": 150,
                                  "Knee": 50}),
        ("stereo_width", {"Width": 110, "Crossover": 150}),
        ("maximizer", {"Input Gain": 3, "Ceiling": -1, "Release": 80}),
        ("loudness_meter", {"Target": "-14 LUFS"}),
    ]),
]


# The Rack's own presets: what a REAPER chain cannot do. A slot is (module, {slider: value}) plus
# band (Full, Low, Mid, High: the slot's own crossovers at the global frequencies), channel
# (Stereo, Mid, Side, Left, Right) and mix (%). A route is (source, slot number from 1, slider,
# depth -1..1 of the slider's range); sources are macro1..macro8, lfo1, lfo2 and env. Globals are
# the Rack's parameters by symbol (RackParams.hpp), enum ones by option label.
def slot(stem, values=None, band="Full", channel="Stereo", mix=100, on=True):
    return {"id": stem, "values": values or {}, "band": band, "channel": channel, "mix": mix, "on": on}


RACK_PRESETS = [
    ("Multiband Distortion", "Routing", [
        slot("saturation", {"Drive": 40, "Tone": -2, "Mix": 100, "Mode": "Tube"}, band="Low"),
        slot("saturation", {"Drive": 35, "Mix": 80, "Mode": "Overdrive"}, band="Mid"),
        slot("saturation", {"Drive": 25, "Tone": 2, "Mix": 60, "Mode": "Tape"}, band="High"),
    ], [], {"xover_low": 200, "xover_high": 3000}),
    ("Multiband Width", "Routing", [
        slot("mono_maker", {"Amount": 100}, band="Low"),
        slot("stereo_width", {"Width": 125}, band="Mid"),
        slot("stereo_width", {"Width": 160}, band="High"),
    ], [], {"xover_low": 180, "xover_high": 4000}),
    ("Dynamic Width", "Routing", [
        slot("stereo_width", {"Width": 90}),
    ], [("env", 1, "Width", 0.45)], {"env_attack": 5, "env_release": 250, "env_gain": 12}),
    ("Mid/Side EQ", "Routing", [
        slot("parametric_eq", {"Low Freq": 120, "Low Gain": 2, "Low Type": "Low Shelf",
                               "Low Mid Freq": 400, "Low Mid Gain": -2, "Low Mid Q": 1,
                               "Mid High Freq": 2500, "Mid High Gain": 1.5, "Mid High Q": 0.8}, channel="Mid"),
        slot("parametric_eq", {"Low Freq": 150, "Low Type": "High Pass", "Low Q": 0.7,
                               "High Freq": 8000, "High Gain": 3, "High Type": "High Shelf"}, channel="Side"),
    ], [], {}),
    ("Auto Filter", "Routing", [
        slot("filter", {"Cutoff": 1200, "Resonance": 3, "Mode": "Low Pass"}),
    ], [("lfo1", 1, "Cutoff", 0.3)], {"lfo1_sync": "1/2", "lfo1_shape": "Sine"}),
    ("Ducked Delay", "Routing", [
        slot("delay", {"Tempo Sync": "ON", "Subdivision": "1/8", "Feedback": 40, "Mix": 40,
                       "Mode": "Ping-Pong", "Low Cut": 200, "High Cut": 6000}),
    ], [("env", 1, "Mix", -0.4)], {"env_attack": 2, "env_release": 300, "env_gain": 12}),
    ("Build-Up", "Macro", [
        slot("riser", {"Intensity": 0}),
        slot("sweeper", {"Amount": 50}),
        slot("pumper", {"Amount": 0}),
    ], [("macro1", 1, "Intensity", 1.0), ("macro1", 2, "Amount", 0.4), ("macro1", 3, "Amount", 0.8)], {}),
    ("Lo-Fi Fader", "Macro", [
        slot("grit", {"Grit Intensity": 0}),
        slot("vinyl_age", {"Mix": 0}),
        slot("filter", {"Cutoff": 20000, "Resonance": 0.9, "Mode": "Low Pass"}),
    ], [("macro1", 1, "Grit Intensity", 0.6), ("macro1", 2, "Mix", 0.85), ("macro1", 3, "Cutoff", -0.45)], {}),
]


def parse_sliders(path):
    """[(name, default, lo, hi, options or None)] for slider1..sliderN of a .jsfx (no gaps)."""
    sliders = []
    for line in path.read_text(encoding="utf-8").split("\n"):
        m = re.match(r"slider(\d+):([^<]*)<([^>]*)>(.*)", line)
        if not m:
            continue
        idx, default, rng, label = int(m.group(1)), float(m.group(2)), m.group(3), m.group(4).lstrip("-")
        if idx != len(sliders) + 1:
            sys.exit(f"build_chains: {path.name}: slider{idx} is not consecutive")
        lo, hi = (float(x) for x in rng.split(",")[:2])
        opts = re.search(r"\{(.*)\}", rng)
        name = re.sub(r"\s*\([^)]*\)\s*$", "", label).strip()
        sliders.append((name, default, lo, hi, [o.strip() for o in opts.group(1).split(",")] if opts else None))
    return sliders


def log_sliders(path):
    """Indices (from 0) of the sliders REAPER shows on a log scale (`:log` in the range)."""
    return [int(m.group(1)) - 1 for m in re.finditer(r"^slider(\d+):[^<]*<[^>]*:log[^>]*>", path.read_text(encoding="utf-8"), re.M)]


def fmt(v):
    return str(int(v)) if v == int(v) else repr(round(v, 6))


def fx_block(chain, stem, values, gfx):
    sliders = parse_sliders(PLUGINS / f"{stem}.jsfx")
    vals = [s[1] for s in sliders]
    for key, v in values.items():
        hits = [i for i, s in enumerate(sliders) if s[0] == key]
        if len(hits) != 1:
            sys.exit(f"build_chains: {chain}: {stem} has no slider named {key!r}")
        i = hits[0]
        name, _, lo, hi, opts = sliders[i]
        if isinstance(v, str):
            if not opts or v not in opts:
                sys.exit(f"build_chains: {chain}: {stem} {key}: {v!r} is not one of {opts}")
            v = opts.index(v)
        if not lo <= v <= hi:
            sys.exit(f"build_chains: {chain}: {stem} {key}: {v} is outside {fmt(lo)}..{fmt(hi)}")
        vals[i] = v
    line = " ".join([fmt(v) for v in vals] + ["-"] * (64 - len(vals)))
    path = f"null_jsfx/gfx/{stem}_gfx.jsfx" if gfx else f"null_jsfx/{stem}.jsfx"
    guid = str(uuid.uuid5(uuid.NAMESPACE_URL, f"https://nulljsfx.tech/chains/{chain}/{path}")).upper()
    return f'BYPASS 0 0\n<JS {path} ""\n  {line}\n>\nFXID {{{guid}}}\nWAK 0 0\n'


def render():
    files = {}
    for kind, folder in FOLDERS.items():
        for chain, fxs in CHAINS:
            files[OUT / folder / f"{chain}.RfxChain"] = "".join(
                fx_block(chain, stem, values, kind == "gfx") for stem, values in fxs)
    return files


def resolve(where, stem, values):
    """[(index, plain value, name)] for {slider name: value} of a plugin, checked like fx_block."""
    sliders = parse_sliders(PLUGINS / f"{stem}.jsfx")
    out = []
    for key, v in values.items():
        hits = [i for i, s in enumerate(sliders) if s[0] == key]
        if len(hits) != 1:
            sys.exit(f"build_chains: {where}: {stem} has no slider named {key!r}")
        i = hits[0]
        _, _, lo, hi, opts = sliders[i]
        if isinstance(v, str):
            if not opts or v not in opts:
                sys.exit(f"build_chains: {where}: {stem} {key}: {v!r} is not one of {opts}")
            v = opts.index(v)
        if not lo <= v <= hi:
            sys.exit(f"build_chains: {where}: {stem} {key}: {v} is outside {fmt(lo)}..{fmt(hi)}")
        out.append((i, v, key))
    return out


def cpp_options(name):
    """The labels of `static const char* const <name>[] = { ... };` in RackParams.hpp."""
    m = re.search(name + r"\[\]\s*=\s*\{([^}]*)\}", (RACK / "RackParams.hpp").read_text(encoding="utf-8"))
    if not m:
        sys.exit(f"build_chains: RackParams.hpp has no {name}")
    return re.findall(r'"([^"]*)"', m.group(1))


def cstr(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def cfloat(v):
    return fmt(float(v)) + ".0f" if float(v) == int(v) else repr(round(float(v), 6)) + "f"


def render_rack():
    """RackPresets.hpp: Init, the CHAINS (modules the Rack has, in order) and RACK_PRESETS."""
    bands, channels = cpp_options("kBandOptions"), cpp_options("kChannelOptions")
    presets = [("Init", "Init", [], [], {})]
    for chain, fxs in CHAINS:
        # a chain plugin that is not a Rack module (a meter) has nothing to do in the Rack
        presets.append((chain, "Chain", [slot(stem, values) for stem, values in fxs
                                         if (RACK / "modules" / f"{stem}.hpp").exists()], [], {}))
    presets += RACK_PRESETS
    out = ["// generated by tools/build_chains.py from its CHAINS and RACK_PRESETS - do not edit",
           "// SPDX-License-Identifier: MIT", "#pragma once", "", '#include "RackParams.hpp"', "",
           "namespace rack {", ""]
    table = []
    for n, (name, cat, slots, routes, globs) in enumerate(presets):
        if len(slots) > 8:
            sys.exit(f"build_chains: {name}: more than 8 slots")
        srows = []
        for k, sl in enumerate(slots):
            vals = resolve(name, sl["id"], sl["values"])
            arr = "nullptr"
            if vals:
                arr = f"kP{n}S{k}"
                out.append(f"static const PresetValue {arr}[] = {{")
                out += [f"    {{ {i}, {cfloat(v)}, {cstr(key)} }}," for i, v, key in vals]
                out.append("};")
            for what, opts in (("band", bands), ("channel", channels)):
                if sl[what] not in opts:
                    sys.exit(f"build_chains: {name}: {what} {sl[what]!r} is not one of {opts}")
            srows.append(f"    {{ {cstr(sl['id'])}, {arr}, {len(vals)}, {bands.index(sl['band'])}, "
                         f"{channels.index(sl['channel'])}, {cfloat(sl['mix'])}, {'true' if sl['on'] else 'false'} }},")
        sarr = rarr = garr = "nullptr"
        if srows:
            sarr = f"kP{n}Slots"
            out += [f"static const PresetSlot {sarr}[] = {{"] + srows + ["};"]
        if routes:
            rarr = f"kP{n}Routes"
            out.append(f"static const PresetRoute {rarr}[] = {{")
            for src, sn, key, depth in routes:
                if not 1 <= sn <= len(slots):
                    sys.exit(f"build_chains: {name}: route to slot {sn}, which is empty")
                i = slider_index(name, slots[sn - 1]["id"], key)
                if not -1 <= depth <= 1:
                    sys.exit(f"build_chains: {name}: depth {depth} is outside -1..1")
                out.append(f"    {{ {cstr(src)}, {sn - 1}, {i}, {cfloat(depth)}, {cstr(key)} }},")
            out.append("};")
        if globs:
            garr = f"kP{n}Globals"
            out.append(f"static const PresetGlobal {garr}[] = {{")
            for sym, v in globs.items():
                if isinstance(v, str):
                    out.append(f"    {{ {cstr(sym)}, 0.0f, {cstr(v)} }},")
                else:
                    out.append(f"    {{ {cstr(sym)}, {cfloat(v)}, nullptr }},")
            out.append("};")
        table.append(f"    {{ {cstr(name)}, {cstr(cat)}, {sarr}, {len(slots)}, {rarr}, {len(routes)}, {garr}, {len(globs)} }},")
        if srows or routes or globs:
            out.append("")
    out += ["static const Preset kPresets[] = {"] + table + ["};",
            "static constexpr uint32_t kNumPresets = sizeof(kPresets) / sizeof(kPresets[0]);", "",
            "// the module sliders REAPER shows on a log scale (`:log` in the JSFX): knobs and modulation",
            "// move them in log space", "static const LogSlider kLogSliders[] = {"]
    for path in sorted(PLUGINS.glob("*.jsfx")):
        for i in log_sliders(path):
            out.append(f"    {{ {cstr(path.stem)}, {i} }},")
    out += ["};", "static constexpr uint32_t kNumLogSliders = sizeof(kLogSliders) / sizeof(kLogSliders[0]);", "",
            "} // namespace rack", ""]
    return "\n".join(out)


def slider_index(where, stem, key):
    """The index (from 0) of the slider named key in a plugin."""
    hits = [i for i, sl in enumerate(parse_sliders(PLUGINS / f"{stem}.jsfx")) if sl[0] == key]
    if len(hits) != 1:
        sys.exit(f"build_chains: {where}: {stem} has no slider named {key!r}")
    return hits[0]

def main():
    files = render()
    files[RACK_PRESETS_H] = render_rack()
    existing = set(OUT.glob("*/*.RfxChain"))
    if "--check" in sys.argv:
        stale = [p for p, text in files.items() if not p.exists() or p.read_text(encoding="utf-8") != text]
        stale += sorted(existing - set(files))
        if stale:
            sys.exit("build_chains: out of date (run tools/build_chains.py): "
                     + " ".join(str(p.relative_to(ROOT)) for p in stale))
        print(f"build_chains: up to date for all {len(CHAINS)} chains and the Rack presets")
        return
    for p in existing - set(files):  # chain files only (the glob never sees RackPresets.hpp)
        p.unlink()
    for p, text in files.items():
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text, encoding="utf-8", newline="\n")
    print(f"build_chains: wrote {len(files) - 1} files ({len(CHAINS)} chains, plain and GFX) and RackPresets.hpp")


if __name__ == "__main__":
    main()
