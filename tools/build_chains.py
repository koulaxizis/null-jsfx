#!/usr/bin/env python3
"""Generates the REAPER FX chains in DATA/FXChains/ from the CHAINS list below.

Every chain is a serial chain of NULL JSFX plugins with chosen slider values. It is written
twice: with the plain JSFX (FXChains/NULL JSFX/<Chain>.RfxChain) and with the GFX versions
(FXChains/NULL JSFX GFX/<Chain>.RfxChain). Values are given by slider name (the label without
its unit), enum sliders by option label; anything not given keeps the plugin's default. Names,
ranges and options are checked against the .jsfx headers, so a chain can never point at a
slider that does not exist or hold a value out of range.

Usage: build_chains.py           write the chains
       build_chains.py --check   exit 1 if the chain files are out of date (used by CI)
"""
import re
import sys
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PLUGINS = ROOT / "DATA/Effects/null_jsfx"
OUT = ROOT / "DATA/FXChains"
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


def main():
    files = render()
    existing = set(OUT.glob("*/*.RfxChain"))
    if "--check" in sys.argv:
        stale = [p for p, text in files.items() if not p.exists() or p.read_text(encoding="utf-8") != text]
        stale += sorted(existing - set(files))
        if stale:
            sys.exit("build_chains: out of date (run tools/build_chains.py): "
                     + " ".join(str(p.relative_to(ROOT)) for p in stale))
        print(f"build_chains: up to date for all {len(CHAINS)} chains")
        return
    for p in existing - set(files):
        p.unlink()
    for p, text in files.items():
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text, encoding="utf-8", newline="\n")
    print(f"build_chains: wrote {len(files)} files ({len(CHAINS)} chains, plain and GFX)")


if __name__ == "__main__":
    main()
