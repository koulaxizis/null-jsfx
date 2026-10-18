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
modules and settings, plus the RACK_PRESETS that use per-slot bands and channels, LFOs, the
envelope and macros. Those become REAPER chains too, built with REAPER's own routing: bands
through Band Splitter and pin mapping, Mid/Side through Mid/Side Matrix, the LFO and the
envelope as parameter modulation, macros as parameter links (see reaper_preset). Slider names resolve to indices here, from the
.jsfx headers (the modules keep the JSFX slider order), so RackPresets.hpp only holds numbers.

Usage: build_chains.py           write the chains and RackPresets.hpp
       build_chains.py --check   exit 1 if any of them is out of date (used by CI)
"""
import base64
import math
import re
import struct
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
# Rack-only features (presets that use them get no .RfxChain, like RACK_ONLY, without being listed):
# - lane="B" on a slot: lanes A and B each run the input through their slots, blended by the
#   global "lane_mix" (0 = A, 100 = B);
# - the feedback loop: globals "feedback" (0..95 %), "fb_from" and "fb_to" (slot numbers from 1);
# - the step sequencer: routes from source "seq"; globals "seq_steps" (1..16), "seq_sync" (an LFO
#   sync label, "Off" = free at "seq_rate" Hz), "seq_glide" (%) and "seq" (up to 16 step values
#   0..1, written as seq_step1..; the steps not given keep their defaults).
# RACK_PRESETS that need what a REAPER chain cannot do (a slot's own mix, a macro on many
# modules that REAPER would need a web of links for): Rack presets only, no .RfxChain
RACK_ONLY = {"Beat Smasher", "Parallel Punch", "Bass Edge", "DJ Macro FX", "Octave Bloom",
             "Dirty Reverb", "Glitch Band"}


def slot(stem, values=None, band="Full", channel="Stereo", mix=100, on=True, lane="A"):
    return {"id": stem, "values": values or {}, "band": band, "channel": channel, "mix": mix, "on": on,
            "lane": lane}


def rack_features(slots, routes, globs):
    """The Rack-only features a RACK_PRESETS entry uses (lanes, feedback, step sequencer)."""
    used = []
    if any(sl["lane"] != "A" for sl in slots):
        used.append("lanes")
    if globs.get("feedback", 0) > 0:
        used.append("feedback")
    if any(src == "seq" for src, *_ in routes):
        used.append("step sequencer")
    return used


def rack_only(name, slots, routes, globs):
    return name in RACK_ONLY or bool(rack_features(slots, routes, globs))


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
    # ---- inspired by the chain logic of commercial all-in-one plugins (generic names; our own
    # settings): research/multi-module-presets.md. The Rack's preset menu has three groups
    # (Chain, Routing, Macro), so these use those: serial ones Chain, ones with bands, Mid/Side,
    # an LFO or the envelope Routing, macro-only Rack presets Macro.
    ("Six-Fader Vocal", "Chain", [
        slot("parametric_eq", {"Low Freq": 120, "Low Type": "Low Shelf",
                               "High Freq": 10000, "High Type": "High Shelf"}),
        slot("dynamics", {"Mode": "Compressor", "Threshold": 0, "Ratio": 3, "Knee": 6, "Attack": 15,
                          "Release": 120}),
        slot("deesser", {"Target": 6500, "Reduction": -4}),
        slot("doubler", {"Amount": 0}),
        slot("delay", {"Tempo Sync": "ON", "Subdivision": "1/16", "Feedback": 20, "Mix": 0,
                       "Low Cut": 200, "High Cut": 6000}),
        slot("reverb", {"Room Size": 40, "Decay": 40, "Damping": 50, "Mix": 0}),
    ], [("macro1", 1, "Low Gain", 0.25), ("macro2", 1, "High Gain", 0.25),
        ("macro3", 2, "Threshold", -0.5), ("macro3", 2, "Makeup", 0.4),
        ("macro4", 6, "Mix", 0.35), ("macro5", 5, "Mix", 0.35), ("macro6", 4, "Amount", 0.6)], {}),
    ("Vocal Leveler", "Routing", [
        slot("breath_control", {"Threshold": -45, "Range": -10}),
        slot("auto_leveler", {"Target": -16, "Speed": 2000, "Max Boost": 6, "Max Cut": 6}),
        slot("deesser", {"Target": 6500, "Reduction": -5}),
        slot("resonance_suppressor", {"Depth": 4, "Low Freq": 1000, "High Freq": 5000}),
        slot("dynamic_eq", {"B1 Freq": 300, "B1 Gain": -4, "B1 Q": 1.2, "B1 Threshold": -24}),
        slot("brighter", {"Amount": 30}, band="High"),
    ], [], {"xover_high": 6000}),
    ("Harmony Stack", "Chain", [
        slot("pitch_correction", {"Scale": "Major", "Retune Speed": 60, "Humanize": 30}),
        slot("harmonizer", {"Voice 1 Interval": 4, "Voice 2 Interval": 7, "Voice 1 Level": -6,
                            "Voice 2 Level": -8, "Mix": 25}),
        slot("formant_shift", {"Formant Position": 54}),
        slot("doubler", {"Amount": 20}),
        slot("stereo_width", {"Width": 140, "Mix": 50}),
        slot("reverb", {"Room Size": 60, "Decay": 55, "Damping": 40, "Mix": 20}),
    ], [("macro1", 2, "Mix", 0.5), ("macro1", 4, "Amount", 0.5)], {}),
    ("Robot Radio Voice", "Routing", [
        slot("pitch_correction", {"Retune Speed": 0, "Humanize": 0}),
        slot("robotizer", {"Pitch": 120, "Metallic": 30, "Mix": 70}),
        slot("formant_filter", {"Vowel": "I", "Q": 8}),
        slot("filter", {"Cutoff": 1000, "Resonance": 0.5, "Mode": "Band Pass"}),
        slot("saturation", {"Drive": 60, "Mix": 100, "Mode": "Tube"}),
        slot("spring_reverb", {"Mix": 15}),
    ], [("lfo1", 3, "Vowel", 0.5)], {"lfo1_rate": 0.2, "lfo1_shape": "Sine"}),
    ("VHS Tape", "Routing", [
        slot("tape", {"Drive": 2, "Speed": "15 ips", "Head Bump": 2, "Wow": 0, "Flutter": 0, "Hiss": 0}),
        slot("chorus_one", {"Chorus Amount": 0}),
        slot("vibrato", {"Rate": 0.6, "Depth": 0, "Mix": 100}),
        slot("grit", {"Grit Intensity": 0}),
        slot("reverb", {"Room Size": 60, "Decay": 50, "Damping": 70, "Mix": 0}),
        slot("filter", {"Cutoff": 10000, "Resonance": 0.7, "Mode": "Low Pass"}),
    ], [("macro1", 1, "Drive", 0.5), ("macro2", 2, "Chorus Amount", 0.7), ("macro3", 5, "Mix", 0.4),
        ("macro4", 3, "Depth", 0.15), ("macro5", 1, "Hiss", 0.3), ("macro6", 4, "Grit Intensity", 0.5),
        ("lfo2", 3, "Rate", 0.03)], {"lfo2_rate": 0.3, "lfo2_shape": "Random"}),
    ("Old Radio", "Routing", [
        slot("mono_maker", {"Amount": 100}),
        slot("filter", {"Cutoff": 300, "Resonance": 0.7, "Mode": "High Pass"}),
        slot("filter", {"Cutoff": 4000, "Resonance": 0.7, "Mode": "Low Pass"}),
        slot("saturation", {"Drive": 50, "Mix": 100, "Mode": "Tube"}),
        slot("vinyl_age", {"Wow": 5, "Flutter": 5, "Crackle": 30, "Noise Level": 15, "Bit Depth": 16,
                           "Sample Rate": 44100, "Lowpass Freq": 8000, "Mix": 60}),
        slot("spring_reverb", {"Mix": 10}),
    ], [("lfo2", 3, "Cutoff", 0.03)], {"lfo2_rate": 0.5, "lfo2_shape": "Random"}),
    ("Sit-in-Mix", "Chain", [
        slot("dynamics", {"Mode": "Compressor", "Threshold": -16, "Ratio": 2, "Knee": 6, "Attack": 30,
                          "Release": 150, "Makeup": 2}),
        slot("saturation", {"Drive": 15, "Mix": 50, "Mode": "Tape"}),
        slot("tilt", {"Amount": 10}),
        slot("channel_utility", {"Output Gain": 0}),
    ], [("macro1", 1, "Threshold", -0.2), ("macro1", 2, "Drive", 0.3)], {}),
    ("Mix Bus Finisher", "Routing", [
        slot("filter", {"Cutoff": 25, "Resonance": 0.7, "Mode": "High Pass"}),
        slot("dynamics", {"Mode": "Compressor", "Threshold": -18, "Ratio": 2, "Knee": 6, "Attack": 30,
                          "Release": 200, "Makeup": 2}),
        slot("multiband_compressor", {"Low Threshold": -16, "Low Ratio": 1.5, "LowMid Threshold": -16,
                                      "LowMid Ratio": 1.5, "HighMid Threshold": -16, "HighMid Ratio": 1.5,
                                      "High Threshold": -16, "High Ratio": 1.5, "Attack": 20,
                                      "Release": 150, "Knee": 50}),
        slot("parametric_eq", {"Low Freq": 60, "Low Gain": 1.5, "Low Type": "Low Shelf",
                               "High Freq": 10000, "High Gain": 1.5, "High Type": "High Shelf"}),
        slot("stereo_width", {"Width": 120}, band="High"),
        slot("mono_maker", {"Amount": 100}, band="Low"),
        slot("ott", {"Depth": 15}),
        slot("limiter", {"Mode": "Limiter", "Threshold": -2, "Ceiling": -1}),
    ], [("macro1", 2, "Threshold", -0.15), ("macro1", 7, "Depth", 0.4), ("macro2", 5, "Width", 0.2)],
        {"xover_low": 120, "xover_high": 3000}),
    ("Small Speaker Check", "Chain", [
        slot("mono_maker", {"Amount": 100}),
        slot("filter", {"Cutoff": 400, "Resonance": 0.7, "Mode": "High Pass"}),
        slot("filter", {"Cutoff": 3500, "Resonance": 0.7, "Mode": "Low Pass"}),
        slot("parametric_eq", {"Mid High Freq": 1000, "Mid High Gain": 4, "Mid High Q": 1}),
        slot("saturation", {"Drive": 10, "Mix": 30, "Mode": "Tape"}),
    ], [], {}),
    ("Beat Smasher", "Macro", [
        slot("parametric_eq", {"Low Freq": 80, "Low Type": "Low Shelf", "High Freq": 10000,
                               "High Type": "High Shelf"}),
        slot("exciter", {"Amount": 0}, band="High"),
        slot("transient_shaper", {"Attack": 0, "Mix": 100}),
        slot("sub_bloom", {"Source Frequency": 80, "Sub Level": 0, "Original Mix": 100}, band="Low"),
        slot("grit", {"Grit Intensity": 0}),
        slot("saturation", {"Drive": 10, "Mix": 100, "Mode": "Tape"}),
        slot("dynamics", {"Mode": "Compressor", "Threshold": -12, "Ratio": 4, "Knee": 6, "Attack": 5,
                          "Release": 80, "Makeup": 1}),
        slot("limiter", {"Mode": "Limiter", "Ceiling": -1}),
    ], [("macro1", 1, "Low Gain", 0.15), ("macro1", 1, "High Gain", 0.15), ("macro1", 2, "Amount", 0.5),
        ("macro2", 3, "Attack", 0.2), ("macro2", 4, "Sub Level", 0.5), ("macro3", 5, "Grit Intensity", 0.7),
        ("macro4", 6, "Drive", 0.5), ("macro4", 7, "Threshold", -0.25)],
        {"xover_low": 120, "xover_high": 4000}),
    ("Parallel Punch", "Macro", [
        slot("transient_shaper", {"Attack": 2, "Mix": 100}),
        slot("multiband_compressor", {"Low Threshold": -12, "Low Ratio": 2, "LowMid Threshold": -12,
                                      "LowMid Ratio": 2, "HighMid Threshold": -12, "HighMid Ratio": 2,
                                      "High Threshold": -12, "High Ratio": 2, "Attack": 20,
                                      "Release": 120, "Knee": 50}),
        slot("dynamics", {"Mode": "Compressor", "Threshold": -35, "Ratio": 10, "Knee": 0, "Attack": 1,
                          "Release": 60, "Makeup": 12}, mix=35),
        slot("saturation", {"Drive": 15, "Mix": 40, "Mode": "Tape"}),
    ], [("macro1", 1, "Attack", 0.2), ("macro2", 2, "Low Threshold", -0.2),
        ("macro2", 2, "LowMid Threshold", -0.2), ("macro2", 2, "HighMid Threshold", -0.2),
        ("macro2", 2, "High Threshold", -0.2), ("macro3", 3, "Makeup", -0.3)], {}),
    ("Kick Fertilizer", "Routing", [
        slot("kick_shaper", {"Attack Shape": 3, "Thump": 3, "Click": 2, "Decay": 250}),
        slot("sub_bloom", {"Source Frequency": 80, "Sub Level": 30, "Original Mix": 100}),
        slot("noise_generator", {"Type": "White", "Level": -60}, band="High"),
        slot("filter", {"Cutoff": 7000, "Resonance": 0.7, "Mode": "High Pass"}, band="High"),
        slot("transient_shaper", {"Attack": 2, "Mix": 100}),
        slot("limiter", {"Mode": "Limiter", "Ceiling": -1}),
    ], [("env", 3, "Level", 0.6)], {"xover_high": 5000, "env_attack": 1, "env_release": 60, "env_gain": 12}),
    ("Bass Edge", "Routing", [
        slot("dynamics", {"Mode": "Compressor", "Threshold": -20, "Ratio": 4, "Knee": 6, "Attack": 10,
                          "Release": 100, "Makeup": 3}),
        slot("saturation", {"Drive": 30, "Mix": 100, "Mode": "Tube"}, band="Mid"),
        slot("ring_mod", {"Carrier Freq": 200, "Mix": 10}, band="Mid"),
        slot("formant_filter", {"Vowel": "O", "Q": 8}, band="Mid", mix=30),
        slot("dynamic_eq", {"B1 Freq": 250, "B1 Gain": -4, "B1 Q": 1, "B1 Threshold": -24}),
        slot("mono_maker", {"Amount": 100}, band="Low"),
    ], [("macro1", 2, "Drive", 0.5), ("macro1", 3, "Mix", 0.3)], {"xover_low": 150, "xover_high": 4000}),
    ("DJ Macro FX", "Macro", [
        slot("dj_filter", {"Filter": 0}),
        slot("flanger", {"Rate": 0.3, "Depth": 4, "Feedback": 50, "Mix": 0}),
        slot("grit", {"Grit Intensity": 0}),
        slot("gate", {"Threshold": -60, "Attack": 1, "Release": 50, "Range": -40}),
        slot("delay", {"Tempo Sync": "ON", "Subdivision": "1/4", "Feedback": 40, "Mix": 0,
                       "Mode": "Ping-Pong", "Low Cut": 200, "High Cut": 6000}),
        slot("reverb", {"Room Size": 70, "Decay": 60, "Mix": 0}),
        slot("limiter", {"Mode": "Limiter", "Ceiling": -1}),
    ], [("macro1", 1, "Filter", 0.4), ("macro1", 2, "Mix", 0.4), ("macro1", 5, "Mix", 0.35),
        ("macro1", 6, "Mix", 0.35), ("macro1", 4, "Threshold", 0.3), ("macro2", 3, "Grit Intensity", 0.7),
        ("macro3", 1, "Filter", -0.4), ("lfo1", 4, "Threshold", 0.25)],
        {"lfo1_sync": "1/8", "lfo1_shape": "Square"}),
    ("Fragment Gate", "Routing", [
        slot("trance_gate", {"Step Length": "1/16", "Gate Depth": 50}),
        slot("filter", {"Cutoff": 600, "Resonance": 0.5, "Gain": 9, "Mode": "Band Pass"}),
        slot("auto_pan", {"Depth": 50, "Mix": 50, "Tempo Sync": "ON", "Subdivision": "1/8"}),
        slot("pitch_shifter", {"Semitones": 12, "Mix": 0}),
        slot("delay", {"Tempo Sync": "ON", "Subdivision": "1/8", "Feedback": 30, "Mix": 25}),
        slot("reverb", {"Room Size": 50, "Decay": 45, "Mix": 15}),
    ], [("macro1", 1, "Gate Depth", 0.5), ("lfo1", 2, "Cutoff", 0.15), ("lfo2", 4, "Mix", 0.4)],
        {"lfo1_sync": "1/16", "lfo1_shape": "Random", "lfo2_sync": "1/4", "lfo2_shape": "Random"}),
    ("Stepped Autopan", "Routing", [
        slot("pumper", {"Amount": 30}),
        slot("auto_pan", {"Depth": 40, "Mix": 100, "Tempo Sync": "ON", "Subdivision": "1/8"}),
        slot("filter", {"Cutoff": 500, "Resonance": 0.5, "Gain": 9, "Mode": "Band Pass"}),
        slot("stereo_width", {"Width": 120}),
    ], [("macro1", 1, "Amount", 0.5), ("macro1", 2, "Depth", 0.5), ("lfo1", 3, "Cutoff", 0.15)],
        {"lfo1_sync": "1/2", "lfo1_shape": "Square"}),
    ("Funk Rig", "Chain", [
        slot("envelope_follower", {"Target": "Lowpass", "Sensitivity": 6, "Low Freq": 150,
                                   "High Freq": 2000, "Resonance": 4, "Direction": "Up", "Mix": 100}),
        slot("phaser", {"Rate": 0.5, "Stages": 6, "Mix": 0}),
        slot("amp_simulator", {"Model": "Clean", "Gain": 6, "Bass": 5, "Mid": 6, "Treble": 6, "Output": -4}),
        slot("cabinet_simulator", {"Cabinet": "2x12 Combo", "Mic Position": 30, "Low Cut": 80}),
        slot("spring_reverb", {"Mix": 15}),
    ], [("macro1", 1, "Mix", -0.8), ("macro1", 2, "Mix", 0.6)], {}),
    ("Organ Rotary", "Chain", [
        slot("saturation", {"Drive": 25, "Mix": 70, "Mode": "Tube"}),
        slot("rotary_speaker", {"Speed": "Slow (Chorale)", "Acceleration": 100, "Drive": 20}),
        slot("reverb", {"Room Size": 30, "Decay": 30, "Mix": 15}),
    ], [("macro1", 2, "Speed", 0.5)], {}),
    ("Octave Bloom", "Macro", [
        slot("granular_delay", {"Delay Time": 250, "Pitch": 12, "Feedback": 30, "Mix": 0}),
        slot("chorus", {"Rate": 0.8, "Depth": 3, "Mix": 0}),
        slot("filter", {"Cutoff": 20, "Resonance": 0.7, "Mode": "High Pass"}),
        slot("reverb", {"Room Size": 85, "Decay": 80, "Damping": 40, "Mix": 0}),
    ], [("macro1", 1, "Mix", 0.35), ("macro1", 2, "Mix", 0.4), ("macro1", 3, "Cutoff", 0.33),
        ("macro1", 4, "Mix", 0.4)], {}),
    ("Dirty Reverb", "Routing", [
        slot("reverb", {"Room Size": 80, "Decay": 70, "Damping": 30, "Mix": 100}),
        slot("saturation", {"Drive": 40, "Mix": 100, "Mode": "Tube"}),
        slot("filter", {"Cutoff": 6000, "Resonance": 0.7, "Mode": "Low Pass"}),
        slot("dynamics", {"Mode": "Compressor", "Threshold": -24, "Ratio": 4, "Knee": 6, "Attack": 20,
                          "Release": 200, "Makeup": 4}),
    ], [("env", 2, "Drive", 0.4)], {"mix": 35, "env_attack": 5, "env_release": 200, "env_gain": 12}),
    ("Broken Media", "Chain", [
        slot("grit", {"Grit Intensity": 0}),
        slot("glitch_fx", {"Rate": 2, "Stutter": "ON", "Reverse Chance": 20, "Reverse": "ON", "Mix": 0}),
        slot("tape", {"Drive": 2, "Wow": 0, "Flutter": 0, "Hiss": 5}),
        slot("vinyl_age", {"Crackle": 40, "Noise Level": 20, "Mix": 0}),
        slot("dynamics", {"Mode": "Compressor", "Threshold": -18, "Ratio": 3, "Knee": 6, "Attack": 10,
                          "Release": 120, "Makeup": 2}),
    ], [("macro1", 1, "Grit Intensity", 0.6), ("macro1", 2, "Mix", 0.5), ("macro2", 3, "Wow", 0.5),
        ("macro2", 3, "Flutter", 0.4), ("macro2", 4, "Mix", 0.7)], {}),
    ("Mid/Side Drive", "Routing", [
        slot("comb_filter", {"Frequency": 500, "Feedback": 0.2}),
        slot("saturation", {"Drive": 35, "Mix": 100, "Mode": "Tube"}, channel="Mid"),
        slot("crunch", {"Amount": 40}, channel="Side"),
        slot("parametric_eq", {"Low Freq": 80, "Low Type": "High Pass", "Low Q": 0.7,
                               "High Freq": 9000, "High Type": "Low Pass", "High Q": 0.7}),
        slot("dynamics", {"Mode": "Compressor", "Threshold": -18, "Ratio": 3, "Knee": 6, "Attack": 10,
                          "Release": 120, "Makeup": 2}),
    ], [("lfo1", 1, "Frequency", 0.1), ("env", 3, "Amount", 0.3)],
        {"lfo1_rate": 0.1, "lfo1_shape": "Triangle", "env_attack": 5, "env_release": 150, "env_gain": 12}),
    ("Color Drive", "Routing", [
        slot("parametric_eq", {"Mid High Freq": 1000, "Mid High Gain": 4, "Mid High Q": 0.8}),
        slot("saturation", {"Drive": 25, "Mix": 100, "Mode": "Tube"}),
        slot("crunch", {"Amount": 20}, band="High"),
        slot("tilt", {"Amount": -10}),
        slot("panner", {"Mode": "Haas", "Haas Delay": 5, "Output": -4}),
    ], [("macro1", 1, "Mid High Freq", 1.0), ("macro2", 2, "Drive", 0.5)], {"xover_high": 3000}),
    ("Tube Smash", "Chain", [
        slot("dynamics", {"Mode": "Compressor", "Threshold": -24, "Ratio": 4, "Knee": 6, "Attack": 2,
                          "Release": 80, "Makeup": 3}),
        slot("saturation", {"Drive": 30, "Mix": 100, "Mode": "Tube"}),
        slot("phatter", {"Amount": 0}),
        slot("tilt", {"Amount": 0}),
    ], [("macro1", 3, "Amount", 0.4), ("macro1", 4, "Amount", -0.1), ("macro2", 2, "Drive", 0.5)], {}),
    ("Shift Mod", "Routing", [
        slot("frequency_shifter", {"Shift": 5, "Mix": 50}, channel="Side"),
        slot("ring_mod", {"Carrier Freq": 300, "Mix": 0}),
        slot("filter", {"Cutoff": 700, "Resonance": 0.3, "Gain": 3, "Mode": "Band Pass"}),
        slot("stereo_width", {"Width": 130}),
    ], [("lfo1", 1, "Shift", 0.002), ("macro1", 2, "Mix", 0.6)], {"lfo1_rate": 0.1, "lfo1_shape": "Sine"}),
    ("Triple Mod", "Routing", [
        slot("phaser", {"Rate": 0.3, "Stages": 4, "Mix": 15}),
        slot("flanger", {"Rate": 0.2, "Depth": 3, "Feedback": 30, "Mix": 15}, channel="Side"),
        slot("chorus", {"Rate": 0.6, "Depth": 3, "Mix": 15}),
        slot("auto_pan", {"Rate": 0.2, "Depth": 30, "Mix": 20}),
    ], [("macro1", 1, "Mix", 0.45), ("macro1", 2, "Mix", 0.45), ("macro1", 3, "Mix", 0.45),
        ("lfo1", 1, "Rate", 0.1), ("lfo2", 2, "Depth", 0.2)],
        {"lfo1_rate": 0.05, "lfo1_shape": "Triangle", "lfo2_rate": 0.2, "lfo2_shape": "Random"}),
    ("Glitch Band", "Macro", [
        slot("glitch_fx", {"Rate": 4, "Stutter": "ON", "Reverse Chance": 30, "Reverse": "ON", "Mix": 0},
             band="Mid"),
        slot("reverser", {"Buffer Length": 1, "Mix": 0}, band="Mid"),
        slot("tape_stop", {"Trigger": "Idle", "Stop Time": 500}, band="Mid"),
        slot("volume_shaper", {"Shape": "Trance 1/8", "Rate": "1/8", "Depth": 0}),
        slot("filter", {"Cutoff": 20000, "Resonance": 0.7, "Mode": "Low Pass"}, band="Mid"),
    ], [("macro1", 1, "Mix", 0.8), ("macro1", 2, "Mix", 0.5), ("macro1", 4, "Depth", 0.6),
        ("macro1", 5, "Cutoff", -0.3), ("macro2", 3, "Trigger", 1.0)], {"xover_low": 150, "xover_high": 16000}),
    ("Distorted Throw", "Routing", [
        slot("delay", {"Tempo Sync": "ON", "Subdivision": "1/4", "Feedback": 45, "Mix": 40,
                       "Mode": "Ping-Pong", "Low Cut": 200, "High Cut": 8000, "Drive": 30}),
        slot("saturation", {"Drive": 35, "Mix": 50, "Mode": "Tape"}),
        slot("ladder_filter", {"Cutoff": 4000, "Resonance": 1.2, "Mode": "Lowpass"}),
        slot("stereo_width", {"Width": 130}),
    ], [("lfo1", 3, "Cutoff", 0.15)], {"lfo1_sync": "2/1", "lfo1_shape": "Sine"}),
    ("Harshness Tamer", "Chain", [
        slot("resonance_suppressor", {"Depth": 4, "Low Freq": 1000, "High Freq": 12000}),
        slot("dynamic_eq", {"B3 Freq": 3500, "B3 Gain": -4, "B3 Q": 1, "B3 Threshold": -24}),
        slot("deesser", {"Target": 8000, "Reduction": -4}),
        slot("tilt", {"Amount": -5}),
    ], [("macro1", 1, "Depth", 0.25), ("macro1", 2, "B3 Gain", -0.15)], {}),
    ("Cosmic Space", "Routing", [
        slot("comb_filter", {"Frequency": 400, "Feedback": 0.4, "Mode": "Feed-Backward", "Gain": 4}),
        slot("granular_delay", {"Delay Time": 400, "Feedback": 40, "Spray": 50, "Mix": 30}),
        slot("reverb", {"Room Size": 95, "Decay": 90, "Damping": 30, "Mix": 30}),
        slot("spectral_freeze", {"Freeze": "Off", "Mix": 70}),
        slot("filter", {"Cutoff": 150, "Resonance": 0.7, "Gain": 3, "Mode": "High Pass"}),
    ], [("lfo1", 1, "Frequency", 0.05), ("macro1", 4, "Freeze", 1.0)], {"lfo1_rate": 0.05, "lfo1_shape": "Sine"}),
    # ---- the Rack-only features: lanes, the feedback loop, the step sequencer
    ("Parallel Drum Smash", "Routing", [
        # lane A has no slot: the dry drums; lane B is the smashed copy
        slot("dynamics", {"Threshold": -36, "Ratio": 12, "Knee": 2, "Attack": 1, "Release": 60, "Makeup": 14,
                          "Style": "FET"}, lane="B"),
        slot("saturation", {"Drive": 40, "Mix": 100, "Mode": "Tube"}, lane="B"),
        slot("ott", {"Depth": 30, "Output": -3}, lane="B"),
        slot("filter", {"Cutoff": 10000, "Resonance": 0.7, "Mode": "Low Pass"}, lane="B"),
    ], [("macro1", 1, "Threshold", -0.2), ("macro2", 2, "Drive", 0.4), ("macro2", 3, "Depth", 0.4)],
        {"lane_mix": 35}),
    ("Psychedelic Echo", "Routing", [
        slot("delay", {"Tempo Sync": "ON", "Subdivision": "1/4", "Feedback": 0, "Mix": 100, "Mode": "Ping-Pong",
                       "Low Cut": 150, "High Cut": 7000, "Modulation": 20}),
        slot("phaser", {"Rate": 0.3, "Stages": 6, "Mix": 50}),
        slot("pitch_shifter", {"Semitones": 12, "Mix": 0}),
        slot("ladder_filter", {"Cutoff": 2500, "Resonance": 1.5, "Mode": "Lowpass"}),
        slot("reverb", {"Room Size": 70, "Decay": 60, "Damping": 40, "Mix": 20}),
    ], [("lfo1", 4, "Cutoff", 0.25), ("macro1", 5, "Mix", 0.3), ("macro1", 2, "Depth", 0.3),
        ("macro2", 3, "Mix", 0.4), ("macro2", 2, "Feedback", 0.3)],
        {"feedback": 60, "fb_from": 1, "fb_to": 4, "mix": 45, "lfo1_rate": 0.15}),
    ("Rhythm Engine", "Routing", [
        slot("volume_shaper", {"Shape": "Pump", "Rate": "1/8", "Depth": 30, "Smooth": 3}),
        slot("ladder_filter", {"Cutoff": 500, "Resonance": 2, "Drive": 20, "Mode": "Lowpass"}),
        slot("saturation", {"Drive": 20, "Mix": 40, "Mode": "Tape"}),
        slot("delay", {"Tempo Sync": "ON", "Subdivision": "1/8D", "Feedback": 30, "Mix": 15, "Mode": "Ping-Pong",
                       "Low Cut": 300, "High Cut": 6000}),
    ], [("seq", 2, "Cutoff", 0.5), ("seq", 3, "Drive", 0.3), ("macro1", 1, "Depth", 0.5),
        ("macro1", 4, "Mix", 0.2), ("macro2", 1, "Shape", 0.55)],
        {"seq_steps": 16, "seq_sync": "1/16", "seq_glide": 20,
         "seq": [1, 0.2, 0.6, 0.1, 0.9, 0.3, 0.5, 0.2, 1, 0.1, 0.7, 0.2, 0.8, 0.4, 0.6, 0]}),
    # ---- multi-module presets (research plan, sections 2.2 and 7.4): Macro 1 is the amount of
    # the effect, Macro 2 a second character control (named in tools/site_chains.json)
    ("Wide Lift", "Chain", [
        slot("exciter", {"Amount": 20}),
        slot("sub_bloom", {"Source Frequency": 90, "Sub Level": 15, "Sub Octave": "-1 Octave", "Original Mix": 100}),
        slot("stereo_width", {"Width": 120, "Crossover": 150}),
        slot("louder", {"Amount": 20}),
    ], [("macro1", 4, "Amount", 0.3), ("macro1", 2, "Sub Level", 0.3), ("macro1", 3, "Width", 0.2),
        ("macro2", 1, "Amount", 0.5)], {}),
    ("Motion Pad", "Routing", [
        slot("chorus", {"Rate": 0.6, "Depth": 4, "Mix": 25, "Voices": 3, "Stereo Spread": 60}),
        slot("filter", {"Cutoff": 3000, "Resonance": 1.5, "Gain": 4, "Mode": "Low Pass"}),
        slot("auto_pan", {"Depth": 30, "Mix": 40, "Tempo Sync": "ON", "Subdivision": "1/4"}),
        slot("shimmer_reverb", {"Size": 70, "Decay": 5, "Shimmer": 35, "Mix": 20}),
    ], [("lfo1", 2, "Cutoff", 0.25), ("macro1", 4, "Mix", 0.25), ("macro1", 1, "Mix", 0.2),
        ("macro1", 3, "Depth", 0.4), ("macro2", 3, "Subdivision", 0.67), ("macro2", 1, "Rate", 0.15)],
        {"lfo1_sync": "1/1", "lfo1_shape": "Sine"}),
    ("Retro Bus", "Chain", [
        slot("chorus_one", {"Chorus Amount": 15}),
        slot("tape", {"Drive": 4, "Speed": "15 ips", "Head Bump": 3, "Wow": 10, "Flutter": 10, "Hiss": 5, "Output": 2}),
        slot("delay", {"Tempo Sync": "ON", "Subdivision": "1/8D", "Feedback": 30, "Mix": 15, "Mode": "Ping-Pong",
                       "Low Cut": 250, "High Cut": 4500, "Drive": 20}),
        slot("reverb", {"Room Size": 50, "Decay": 45, "Damping": 60, "Mix": 12}),
    ], [("macro1", 3, "Mix", 0.2), ("macro1", 4, "Mix", 0.15), ("macro1", 1, "Chorus Amount", 0.3),
        ("macro2", 2, "Wow", 0.4), ("macro2", 2, "Flutter", 0.3), ("macro2", 2, "Hiss", 0.2),
        ("macro2", 3, "High Cut", -0.3)], {}),
    ("Micro Grains", "Chain", [
        slot("granular_delay", {"Delay Time": 120, "Grain Size": 40, "Density": 30, "Spray": 30, "Feedback": 30,
                                "Mix": 20}),
        slot("reverser", {"Buffer Length": 1, "Mix": 8}),
        slot("glitch_fx", {"Rate": 4, "Stutter Length": 16, "Stutter": "ON", "Reverse Chance": 15,
                           "Reverse": "ON", "Mix": 8}),
    ], [("macro1", 1, "Mix", 0.3), ("macro1", 2, "Mix", 0.2), ("macro1", 3, "Mix", 0.3),
        ("macro2", 1, "Spray", 0.5), ("macro2", 3, "Reverse Chance", 0.5), ("macro2", 3, "Rate", 0.3)], {}),
    ("Desert Twang", "Chain", [
        slot("tremolo", {"Rate": 4.5, "Depth": 45, "Mix": 60, "Mode": "Harmonic"}),
        slot("amp_simulator", {"Model": "Clean", "Gain": 3, "Bass": 4, "Mid": 5, "Treble": 7, "Presence": 6,
                               "Bright": "On", "Output": 5}),
        slot("cabinet_simulator", {"Cabinet": "1x12 Open Back", "Mic Position": 35, "Low Cut": 80}),
        slot("spring_reverb", {"Decay": 2.5, "Tension": 60, "Drip": 50, "Mix": 25}),
        slot("delay", {"Time": 330, "Feedback": 25, "Mix": 18, "Low Cut": 200, "High Cut": 5000, "Modulation": 15}),
    ], [("macro1", 4, "Mix", 0.2), ("macro1", 5, "Mix", 0.2), ("macro1", 1, "Depth", 0.4),
        ("macro2", 2, "Gain", 0.4), ("macro2", 2, "Master", -0.2)], {}),
    ("Wall of Sound", "Chain", [
        slot("saturation", {"Drive": 30, "Tone": -2, "Mix": 40, "Mode": "Fuzz"}),
        slot("chorus", {"Rate": 0.5, "Depth": 5, "Mix": 40, "Voices": 3, "Stereo Spread": 80}),
        slot("reverb", {"Room Size": 85, "Decay": 80, "Damping": 50, "Mix": 35, "Pre-Delay": 30, "Width": 150,
                        "Modulation": 30, "Character": "Hall"}),
        slot("spectral_freeze", {"Freeze": "Off", "Smear": 60, "Mix": 100}),
        slot("channel_utility", {"Output Gain": -4}),
    ], [("macro1", 1, "Drive", 0.4), ("macro1", 3, "Mix", 0.3), ("macro1", 2, "Mix", 0.3),
        ("macro2", 3, "Decay", 0.2), ("macro2", 3, "Room Size", 0.1), ("macro2", 3, "Pre-Delay", 0.2)], {}),
    ("Mutant Space", "Routing", [
        slot("reverb", {"Room Size": 80, "Decay": 70, "Damping": 30, "Mix": 22, "Modulation": 40}),
        slot("pitch_shifter", {"Octave": 1, "Mix": 10}),
        slot("ring_mod", {"Carrier Freq": 300, "Depth": 50, "Mix": 8}),
    ], [("lfo1", 3, "Carrier Freq", 0.1), ("macro1", 1, "Mix", 0.35), ("macro1", 1, "Decay", 0.2),
        ("macro2", 2, "Mix", 0.3), ("macro2", 3, "Mix", 0.25)], {"lfo1_rate": 0.15, "lfo1_shape": "Triangle"}),
    ("Noise Layer", "Routing", [
        # lane A has no slot: the dry signal; lane B is the noise, its level following the input
        slot("noise_generator", {"Type": "Pink", "Level": -40}, lane="B"),
        slot("filter", {"Cutoff": 1500, "Resonance": 1, "Mode": "Band Pass"}, lane="B"),
        slot("reverb", {"Room Size": 70, "Decay": 60, "Damping": 40, "Mix": 50}, lane="B"),
    ], [("env", 1, "Level", 0.5), ("macro1", 1, "Level", 0.2), ("macro2", 2, "Cutoff", 0.4)],
        {"lane_mix": 12, "env_attack": 2, "env_release": 120, "env_gain": 12}),
    ("Pop Vocal", "Chain", [
        slot("de_plosive", {"Sensitivity": 50, "Max Reduction": 12}),
        slot("deesser", {"Target": 6500, "Reduction": -5}),
        slot("dynamics", {"Threshold": -20, "Ratio": 3, "Knee": 6, "Attack": 8, "Release": 120, "Makeup": 5}),
        slot("parametric_eq", {"Low Freq": 90, "Low Type": "High Pass", "Low Q": 0.7,
                               "Low Mid Freq": 300, "Low Mid Gain": -2, "Low Mid Q": 1,
                               "Mid High Freq": 3500, "Mid High Gain": 1.5, "Mid High Q": 0.8,
                               "High Freq": 11000, "High Gain": 3, "High Type": "High Shelf"}),
        slot("doubler", {"Amount": 20}),
        slot("reverb", {"Room Size": 45, "Decay": 45, "Damping": 45, "Mix": 15, "Pre-Delay": 30, "Low Cut": 200,
                        "Character": "Plate"}),
    ], [("macro1", 3, "Threshold", -0.15), ("macro1", 3, "Makeup", 0.15),
        ("macro2", 5, "Amount", 0.4), ("macro2", 6, "Mix", 0.2)], {}),
    ("Rap Vocal Upfront", "Chain", [
        slot("gate", {"Threshold": -50, "Attack": 2, "Release": 150, "Range": -20}),
        slot("dynamics", {"Threshold": -24, "Ratio": 6, "Knee": 3, "Attack": 3, "Release": 60, "Makeup": 8,
                          "Style": "FET"}),
        slot("saturation", {"Drive": 20, "Mix": 40, "Mode": "Tube"}),
        slot("deesser", {"Target": 7000, "Reduction": -6}),
        slot("delay", {"Tempo Sync": "ON", "Subdivision": "1/8", "Feedback": 15, "Mix": 10, "Low Cut": 300,
                       "High Cut": 5000, "Ducking": 50}),
    ], [("macro1", 2, "Threshold", -0.15), ("macro1", 2, "Makeup", 0.1),
        ("macro2", 5, "Mix", 0.3), ("macro2", 5, "Feedback", 0.25)], {}),
    ("Airy Ballad", "Chain", [
        slot("dynamics", {"Threshold": -22, "Ratio": 3, "Knee": 10, "Attack": 20, "Release": 300, "Makeup": 5,
                          "Style": "Opto"}),
        slot("exciter", {"Amount": 25}),
        slot("delay", {"Tempo Sync": "ON", "Subdivision": "1/4", "Feedback": 30, "Mix": 15, "Mode": "Ping-Pong",
                       "Low Cut": 300, "High Cut": 6000, "Diffusion": 30, "Ducking": 40}),
        slot("shimmer_reverb", {"Size": 75, "Decay": 5, "Shimmer": 25, "Damping": 7000, "Mix": 20}),
    ], [("macro1", 4, "Mix", 0.25), ("macro1", 3, "Mix", 0.2),
        ("macro2", 2, "Amount", 0.5), ("macro2", 4, "Shimmer", 0.4)], {}),
    ("Choir Pad Vocal", "Chain", [
        # a fifth above and an octave below fit any key (no scale-aware harmonizer yet)
        slot("harmonizer", {"Voice 1 Interval": 7, "Voice 2 Interval": -12, "Voice 1 Level": -3,
                            "Voice 2 Level": -6, "Detune": 15, "Delay": 20, "Spread": 90, "Mix": 30}),
        slot("formant_shift", {"Formant Position": 45}),
        slot("chorus", {"Rate": 0.4, "Depth": 5, "Mix": 20, "Voices": 4, "Stereo Spread": 100, "Mode": "Dimension"}),
        slot("shimmer_reverb", {"Size": 85, "Decay": 8, "Shimmer": 40, "Damping": 6000, "Mix": 20}),
    ], [("macro1", 1, "Mix", 0.3), ("macro1", 4, "Mix", 0.2), ("macro2", 2, "Formant Position", -0.2)], {}),
    ("Podcast Ready", "Chain", [
        slot("noise_reduction", {"Auto": "On", "Reduction": 10, "Threshold": 3}),
        slot("de_hum", {"Frequency": "50 Hz", "Harmonics": 4, "Depth": 30, "Low Cut": "40 Hz"}),
        slot("de_plosive", {"Sensitivity": 50, "Max Reduction": 15}),
        slot("deesser", {"Target": 6000, "Reduction": -5}),
        slot("auto_leveler", {"Target": -17, "Speed": 1000, "Max Boost": 12, "Max Cut": 9}),
        slot("limiter", {"Mode": "Limiter", "Threshold": -2, "Ceiling": -1}),
        slot("loudness_meter", {"Target": "-16 LUFS"}),
    ], [("macro1", 1, "Reduction", 0.2), ("macro1", 4, "Reduction", -0.2),
        ("macro2", 5, "Target", 0.15)], {}),
    ("Snare Crack", "Chain", [
        slot("transient_shaper", {"Attack": 5, "Sustain": -1}),
        slot("parametric_eq", {"Low Freq": 200, "Low Gain": 2, "Low Q": 1.2, "Low Type": "Peaking",
                               "Low Mid Freq": 450, "Low Mid Gain": -2.5, "Low Mid Q": 1.5,
                               "Mid High Freq": 4000, "Mid High Gain": 2.5, "Mid High Q": 0.8,
                               "High Freq": 10000, "High Gain": 2, "High Type": "High Shelf"}),
        slot("saturation", {"Drive": 25, "Mix": 50, "Mode": "Tape"}),
        slot("reverb", {"Room Size": 25, "Decay": 30, "Damping": 40, "Mix": 15, "Pre-Delay": 10}),
    ], [("macro1", 1, "Attack", 0.15), ("macro1", 3, "Drive", 0.3),
        ("macro2", 4, "Mix", 0.25), ("macro2", 4, "Decay", 0.2)], {}),
    ("Hat Shine", "Chain", [
        slot("filter", {"Cutoff": 300, "Resonance": 0.7, "Mode": "High Pass"}),
        slot("exciter", {"Amount": 30}),
        slot("transient_shaper", {"Attack": 2, "Sustain": -4}),
        slot("stereo_width", {"Width": 130, "Crossover": 300}),
    ], [("macro1", 2, "Amount", 0.4), ("macro1", 3, "Sustain", -0.15), ("macro2", 4, "Width", 0.25)], {}),
    ("808 Glue", "Chain", [
        slot("sub_bloom", {"Source Frequency": 80, "Sub Level": 20, "Sub Octave": "-1 Octave", "Original Mix": 100}),
        slot("saturation", {"Drive": 30, "Mix": 60, "Mode": "Tube"}),
        slot("clipper", {"Input": 2, "Ceiling": -1, "Knee": 30, "Oversampling": "4x", "Output": -3}),
        slot("mono_maker", {"Amount": 60}),
    ], [("macro1", 2, "Drive", 0.4), ("macro1", 3, "Input", 0.15), ("macro1", 3, "Output", -0.12),
        ("macro2", 1, "Sub Level", 0.4)], {}),
    ("Reese Bass", "Routing", [
        slot("mono_maker", {"Amount": 100}, band="Low"),
        slot("saturation", {"Drive": 45, "Mix": 80, "Mode": "Tube"}, band="Mid"),
        slot("chorus", {"Rate": 0.3, "Depth": 6, "Mix": 50, "Voices": 3, "Stereo Spread": 80}, band="Mid"),
        slot("phaser", {"Rate": 0.2, "Stages": 6, "Mix": 40, "Depth": 60, "Feedback": 40}, band="Mid"),
    ], [("lfo1", 4, "Center", 0.2), ("macro1", 2, "Drive", 0.4), ("macro1", 3, "Mix", 0.3),
        ("macro2", 4, "Mix", 0.4)], {"xover_low": 120, "xover_high": 6000, "lfo1_sync": "2/1"}),
    ("Low End Focus", "Routing", [
        slot("mono_maker", {"Amount": 100}, band="Low"),
        slot("dynamic_eq", {"B1 Freq": 100, "B1 Gain": -4, "B1 Q": 1.2, "B1 Threshold": -20}, band="Low"),
        slot("sub_bloom", {"Source Frequency": 70, "Sub Level": 15, "Original Mix": 100}, band="Low"),
    ], [("macro1", 2, "B1 Gain", -0.25), ("macro2", 3, "Sub Level", 0.4)], {"xover_low": 150}),
    ("Ambient Guitar", "Chain", [
        slot("transient_shaper", {"Attack": -12, "Sustain": 3}),
        slot("delay", {"Tempo Sync": "ON", "Subdivision": "1/4D", "Feedback": 45, "Mix": 25, "Mode": "Ping-Pong",
                       "Low Cut": 250, "High Cut": 5000, "Modulation": 25, "Diffusion": 40}),
        slot("shimmer_reverb", {"Size": 85, "Decay": 8, "Shimmer": 45, "Mix": 30}),
    ], [("macro1", 2, "Mix", 0.2), ("macro1", 3, "Mix", 0.2), ("macro2", 1, "Attack", -0.2)], {}),
    ("Classic Wah Rig", "Chain", [
        slot("wah", {"Mode": "Auto", "Model": "Classic", "Resonance": 60, "Sensitivity": 50, "Output": 8}),
        slot("amp_simulator", {"Model": "Crunch", "Gain": 5, "Bass": 5, "Mid": 6, "Treble": 6, "Presence": 5,
                               "Output": -1}),
        slot("cabinet_simulator", {"Cabinet": "2x12 Combo", "Mic Position": 30, "Low Cut": 80}),
        slot("spring_reverb", {"Mix": 15}),
    ], [("macro1", 1, "Sensitivity", 0.4), ("macro1", 1, "Resonance", 0.3), ("macro2", 2, "Gain", 0.3)], {}),
    ("Jet Flange", "Chain", [
        slot("flanger", {"Depth": 6, "Feedback": 70, "Mix": 50, "Manual": 1, "Tempo Sync": "2/1",
                         "Stereo Phase": 90}),
        slot("saturation", {"Drive": 25, "Mix": 50, "Mode": "Tape"}),
    ], [("macro1", 1, "Mix", 0.3), ("macro1", 1, "Feedback", 0.2), ("macro2", 1, "Manual", 0.3)], {}),
    ("Frozen Hall", "Chain", [
        slot("reverb", {"Room Size": 85, "Decay": 75, "Damping": 40, "Mix": 25, "Pre-Delay": 40, "Width": 130,
                        "Modulation": 25, "Character": "Hall"}),
        slot("filter", {"Cutoff": 12000, "Resonance": 0.7, "Mode": "Low Pass"}),
    ], [("macro1", 1, "Mix", 0.4), ("macro2", 1, "Freeze", 1.0)], {}),
    ("Loud Master", "Chain", [
        slot("tilt", {"Amount": 5}),
        slot("multiband_compressor", {"Low Threshold": -16, "Low Ratio": 2, "LowMid Threshold": -16,
                                      "LowMid Ratio": 1.5, "HighMid Threshold": -16, "HighMid Ratio": 1.5,
                                      "High Threshold": -16, "High Ratio": 2, "Attack": 20, "Release": 150,
                                      "Knee": 50}),
        slot("clipper", {"Input": 1, "Ceiling": -0.5, "Knee": 40, "Oversampling": "4x"}),
        slot("maximizer", {"Input Gain": 2, "Ceiling": -1, "Release": 80}),
        slot("loudness_meter", {"Target": "-14 LUFS"}),
    ], [("macro1", 4, "Input Gain", 0.25), ("macro1", 3, "Input", 0.1), ("macro2", 1, "Amount", 0.1)], {}),
    ("Channel Strip", "Chain", [
        slot("gate", {"Threshold": -55, "Attack": 2, "Release": 150, "Range": -30}),
        slot("parametric_eq", {"Low Freq": 60, "Low Type": "High Pass", "Low Q": 0.7,
                               "Low Mid Freq": 400, "Low Mid Gain": -1.5, "Low Mid Q": 1,
                               "Mid High Freq": 3000, "Mid High Gain": 1, "Mid High Q": 1,
                               "High Freq": 12000, "High Gain": 1.5, "High Type": "High Shelf"}),
        slot("dynamics", {"Threshold": -18, "Ratio": 3, "Knee": 6, "Attack": 10, "Release": 120, "Makeup": 3,
                          "Style": "Bus"}),
        slot("deesser", {"Target": 6500, "Reduction": -4}),
        slot("saturation", {"Drive": 15, "Mix": 40, "Mode": "Tape"}),
    ], [("macro1", 3, "Threshold", -0.15), ("macro1", 3, "Makeup", 0.15), ("macro2", 5, "Drive", 0.4)], {}),
    ("Pultec Lift", "Chain", [
        # the low shelf boost with a wide cut just above it, a broad 12 kHz lift and a gentle 20 kHz shelf down
        slot("parametric_eq", {"Low Freq": 60, "Low Gain": 4, "Low Q": 0.6, "Low Type": "Low Shelf",
                               "Low Mid Freq": 300, "Low Mid Gain": -1.5, "Low Mid Q": 0.7,
                               "High Freq": 12000, "High Gain": 3, "High Q": 0.5,
                               "Ultra High Freq": 20000, "Ultra High Gain": -2, "Ultra High Type": "High Shelf"}),
        slot("saturation", {"Drive": 20, "Mix": 50, "Mode": "Tube"}),
    ], [("macro1", 1, "Low Gain", 0.1), ("macro1", 1, "High Gain", 0.08), ("macro2", 2, "Drive", 0.4)], {}),
    ("Abbey Chamber", "Chain", [
        # the reverb's own low and high cut shape only the chamber, like an EQ on the send
        slot("reverb", {"Room Size": 60, "Decay": 55, "Damping": 50, "Mix": 25, "Pre-Delay": 80, "Low Cut": 300,
                        "High Cut": 9000, "Width": 120, "Character": "Plate"}),
        slot("saturation", {"Drive": 15, "Mix": 40, "Mode": "Tape"}),
    ], [("macro1", 1, "Mix", 0.3), ("macro2", 1, "Decay", 0.25)], {}),
    ("Sub Harmonics", "Routing", [
        slot("octaver", {"Dry": 100, "Oct -1": 30, "Oct -2": 0, "Up": 0, "Tone": 400, "Tracking": 150},
             band="Low"),
        slot("sub_bloom", {"Source Frequency": 80, "Sub Level": 15, "Original Mix": 100}, band="Low"),
        slot("mono_maker", {"Amount": 100}, band="Low"),
    ], [("macro1", 1, "Oct -1", 0.4), ("macro1", 2, "Sub Level", 0.3), ("macro2", 1, "Oct -2", 0.4)],
        {"xover_low": 120}),
    ("Stepped Filter", "Routing", [
        slot("filter", {"Cutoff": 400, "Resonance": 3, "Mode": "Low Pass"}),
    ], [("seq", 1, "Cutoff", 0.5), ("macro1", 1, "Resonance", 0.3), ("macro2", 1, "Cutoff", 0.15)],
        {"seq_steps": 16, "seq_sync": "1/16", "seq_glide": 10,
         "seq": [0.9, 0.1, 0.5, 0.1, 0.7, 0.1, 0.3, 1, 0.2, 0.6, 0.1, 0.8, 0.4, 0.1, 0.6, 0.3]}),
    ("Pusher", "Chain", [
        slot("transient_shaper", {"Attack": 3, "Sustain": 1}),
        slot("exciter", {"Amount": 20}),
        slot("stereo_width", {"Width": 115, "Crossover": 150}),
        slot("clipper", {"Input": 1, "Ceiling": -1, "Knee": 30, "Oversampling": "4x"}),
    ], [("macro1", 1, "Attack", 0.1), ("macro1", 2, "Amount", 0.3), ("macro1", 4, "Input", 0.15),
        ("macro1", 4, "Output", -0.12),
        ("macro2", 3, "Width", 0.25)], {}),
    # ---- with Step FX and Tape Echo
    ("Stutter Build", "Macro", [
        slot("step_fx", {**{f"Step {k}": "Off" for k in range(1, 9)}, **{f"Step {k}": "Repeat" for k in range(9, 17)},
                         "Step Length": "1/16", "Repeat Size": "1/16", "Mix": 0}),
        slot("filter", {"Cutoff": 20, "Resonance": 1.2, "Mode": "High Pass"}),
        slot("reverb", {"Room Size": 70, "Decay": 60, "Mix": 0}),
    ], [("macro1", 1, "Mix", 1.0), ("macro1", 2, "Cutoff", 0.45), ("macro1", 3, "Mix", 0.3),
        ("macro2", 1, "Repeat Size", 1.0)], {}),
    ("Dub Echo Bus", "Chain", [
        slot("tape_echo", {"Tempo Sync": "1/4D", "Heads": "1+3", "Intensity": 55, "Bass": -3, "Treble": -4,
                           "Saturation": 40, "Spring": 30, "Mix": 25}),
        slot("filter", {"Cutoff": 60, "Resonance": 0.7, "Gain": 2, "Mode": "High Pass"}),
        slot("spring_reverb", {"Decay": 3, "Mix": 12}),
    ], [("macro1", 1, "Mix", 0.3), ("macro1", 1, "Intensity", 0.25), ("macro2", 1, "Wow/Flutter", 0.4)], {}),
]


def parse_sliders(path):
    """[(name, default, lo, hi, options or None)] for slider1..sliderN of a .jsfx (no gaps)."""
    sliders = []
    for line in path.read_text(encoding="utf-8").split("\n"):
        f = re.match(r"slider(\d+):/[^:]*:[^:]*:(.*)", line)
        if f:  # a file slider (e.g. Convolution's IR File): an index into a folder, 0 here
            if int(f.group(1)) != len(sliders) + 1:
                sys.exit(f"build_chains: {path.name}: slider{f.group(1)} is not consecutive")
            sliders.append((f.group(2).lstrip("-").strip(), 0.0, 0.0, 0.0, None))
            continue
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


def module_index(stem, index):
    """A JSFX slider index (from 0) as the Rack module's value index: a module has no value for a
    file slider (Convolution's IR File), so the sliders after one move down."""
    text = (PLUGINS / f"{stem}.jsfx").read_text(encoding="utf-8")
    files = [int(m.group(1)) - 1 for m in re.finditer(r"^slider(\d+):/", text, re.M)]
    if index in files:
        sys.exit(f"build_chains: {stem}: slider {index + 1} is a file slider, not a Rack value")
    return index - sum(1 for f in files if f < index)


def log_sliders(path):
    """Indices (from 0) of the sliders REAPER shows on a log scale (`:log` in the range)."""
    return [int(m.group(1)) - 1 for m in re.finditer(r"^slider(\d+):[^<]*<[^>]*:log[^>]*>", path.read_text(encoding="utf-8"), re.M)]


def fmt(v):
    return str(int(v)) if v == int(v) else repr(round(v, 6))


def fx_block(chain, stem, values, gfx, seed="", pins="", mods=""):
    """One plugin of a chain: optional JS_PINMAP (pins) and PROGRAMENV blocks (mods) go in the
    places REAPER writes them. seed tells apart two copies of one plugin in a chain."""
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
    guid = str(uuid.uuid5(uuid.NAMESPACE_URL, f"https://nulljsfx.tech/chains/{chain}/{path}{seed}")).upper()
    return f'BYPASS 0 0\n<JS {path} ""\n  {line}\n>\n{pins}FXID {{{guid}}}\n{mods}WAK 0 0\n'


def render():
    files = {}
    for kind, folder in FOLDERS.items():
        for chain, fxs in CHAINS:
            files[OUT / folder / f"{chain}.RfxChain"] = "".join(
                fx_block(chain, stem, values, kind == "gfx") for stem, values in fxs)
        for name, _, slots, routes, globs in RACK_PRESETS:
            if rack_only(name, slots, routes, globs):
                continue
            files[OUT / folder / f"{name.replace('/', '-')}.RfxChain"] = reaper_preset(
                name, slots, routes, globs, kind == "gfx")
    return files


def pin_counts(stem):
    text = (PLUGINS / f"{stem}.jsfx").read_text(encoding="utf-8")
    return len(re.findall(r"^in_pin:", text, re.M)) or 2, len(re.findall(r"^out_pin:", text, re.M)) or 2


def pinmap(nch, ins, outs):
    """A JS_PINMAP block: for the input pins, then the output pins, the track channels each pin
    is connected to, as a 64-bit mask per pin (1 = channel 1). Layout as REAPER writes it: 1000,
    then per direction its byte size, 1000, the track channel count, the pin count and the masks."""
    data = struct.pack("<I", 1000)
    for masks in (ins, outs):
        data += struct.pack("<4I", 12 + 8 * len(masks), 1000, nch, len(masks))
        data += b"".join(struct.pack("<Q", m) for m in masks)
    b64 = base64.b64encode(data).decode()
    return "<JS_PINMAP\n" + "".join(f"  {b64[i:i + 128]}\n" for i in range(0, len(b64), 128)) + ">\n"


def normalized(stem, index, v):
    """A slider value as REAPER's 0..1 parameter value (log sliders on their log scale)."""
    _, _, lo, hi, _ = parse_sliders(PLUGINS / f"{stem}.jsfx")[index]
    if index in log_sliders(PLUGINS / f"{stem}.jsfx"):
        return math.log(v / lo) / math.log(hi / lo)
    return (v - lo) / (hi - lo)


def slider_value(where, sl, index):
    """The value a slot sets on slider `index`, or the slider's default."""
    for i, v, _ in resolve(where, sl["id"], sl["values"]):
        if i == index:
            return v
    return parse_sliders(PLUGINS / f"{sl['id']}.jsfx")[index][1]


def programenv(index, base, lines):
    """A parameter modulation block for slider `index` (from 0), around baseline `base` (0..1)."""
    body = [f"PARAMBASE {fmt(round(base, 6))}"] + lines
    return f"<PROGRAMENV {index} 0\n" + "".join(f"  {b}\n" for b in body) + ">\n"


LFO_SHAPES = {"Sine": 0, "Square": 1, "Saw Down": 2, "Saw Up": 3, "Triangle": 4, "Random": 5}
BAND_CHANNEL = {"Low": 0, "Mid": 2, "High": 4}  # the first track channel of each Band Splitter band


def reaper_preset(name, slots, routes, globs, gfx):
    """A RACK_PRESETS entry as a REAPER chain. A run of band slots: Band Splitter (Split, 3 bands at
    the Rack's crossovers), each slot pinned to its band's channel pair (1/2 Low, 3/4 Mid, 5/6
    High), Band Splitter (Merge). A run of Mid/Side slots: Mid/Side Matrix (Encode), each slot
    reads and writes only channel 1 (Mid) or 2 (Side), Mid/Side Matrix (Decode). Envelope routes become an
    audio control signal on the target slider, LFO routes a parameter LFO, and the routes of one
    macro become parameter links to the slider of the route with the largest depth, which then
    works as the macro."""
    defaults = {"xover_low": 200, "xover_high": 2500, "env_attack": 10, "env_release": 150, "env_gain": 6,
                "lfo1_shape": "Sine", "lfo2_shape": "Sine"}
    g = {**defaults, **globs}
    if rack_features(slots, routes, globs):
        sys.exit(f"build_chains: {name}: no REAPER chain for {', '.join(rack_features(slots, routes, globs))}")
    for sl in slots:
        if sl["mix"] != 100 or not sl["on"] or sl["channel"] in ("Left", "Right"):
            sys.exit(f"build_chains: {name}: no REAPER chain for slot mix, off slots or Left/Right "
                     "(add it to RACK_ONLY)")
        if sl["band"] != "Full" and sl["channel"] != "Stereo":
            sys.exit(f"build_chains: {name}: no REAPER chain for a slot on both a band and a channel")
    # runs of slots that share a routing: band slots go between one Band Splitter Split/Merge
    # pair, Mid/Side slots between one Mid/Side Matrix Encode/Decode pair
    kind = ["band" if sl["band"] != "Full" else "ms" if sl["channel"] != "Stereo" else "" for sl in slots]
    split = {"Mode": "Split", "Bands": 3, "Crossover 1": g["xover_low"], "Crossover 2": g["xover_high"]}
    wrap = {"band": (("band_splitter", split), ("band_splitter", {**split, "Mode": "Merge"})),
            "ms": (("mid_side_matrix", {"Mode": "Encode"}), ("mid_side_matrix", {"Mode": "Decode"}))}
    items, pos = [], {}  # chain items (stem, values, slot index or None); chain position of each slot
    for k, sl in enumerate(slots):
        if kind[k] and (k == 0 or kind[k - 1] != kind[k]):
            items.append((*wrap[kind[k]][0], None))
        pos[k] = len(items)
        items.append((sl["id"], sl["values"], k))
        if kind[k] and (k == len(slots) - 1 or kind[k + 1] != kind[k]):
            items.append((*wrap[kind[k]][1], None))
    nch = 6 if "band" in kind else 2
    # modulation per slot: {slot index: [PROGRAMENV blocks]}
    mods = {}
    macros = {}
    for src, sn, key, depth in routes:
        if src.startswith("macro"):
            macros.setdefault(src, []).append((sn - 1, key, depth))
    for src, sn, key, depth in routes:
        k = sn - 1
        stem = slots[k]["id"]
        i = slider_index(name, stem, key)
        base = normalized(stem, i, slider_value(name, slots[k], i))
        sign = 1 if depth > 0 else -1
        if src == "env":
            hi = -g["env_gain"]
            lines = ["LFO 0", "LFOWT 1 1", "AUDIOCTL 1", f"AUDIOCTLWT {fmt(abs(depth))} {sign}",
                     "CHAN 0", "STEREO 1", f"RMS {fmt(g['env_attack'])} {fmt(g['env_release'])}",
                     f"DBLO {fmt(max(-60, hi - 36))}", f"DBHI {fmt(hi)}", "X2 0.5", "Y2 0.5"]
        elif src.startswith("lfo"):
            # REAPER's own default speed: the Rack's rate or sync has no checked REAPER equivalent
            lines = ["LFO 1", f"LFOWT {fmt(min(1, 2 * abs(depth)))} 0", "AUDIOCTL 0", "AUDIOCTLWT 1 1",
                     f"LFOSHAPE {LFO_SHAPES[g[src + '_shape']]}", "LFOSYNC 0 0 0", "LFOSPEED 0.124573 0"]
        else:
            group = macros[src]
            mk, mkey, mdepth = max(group, key=lambda r: abs(r[2]))
            if (mk, mkey) == (k, key):
                continue  # the macro itself: its own slider
            mi = slider_index(name, slots[mk]["id"], mkey)
            mbase = normalized(slots[mk]["id"], mi, slider_value(name, slots[mk], mi))
            scale = depth / mdepth
            lines = ["LFO 0", "LFOWT 1 1", "AUDIOCTL 0", "AUDIOCTLWT 1 1",
                     f"PLINK {fmt(round(scale, 6))} {pos[mk]}:{pos[mk] - pos[k]} {mi} {fmt(round(-scale * mbase, 6))}"]
        if any(b.startswith(f"<PROGRAMENV {i} ") for b in mods.get(k, [])):
            sys.exit(f"build_chains: {name}: two routes on slot {sn} {key}: a REAPER chain holds one "
                     "modulation per slider (add it to RACK_ONLY)")
        mods.setdefault(k, []).append(programenv(i, base, lines))

    blocks = []
    for n, (stem, values, k) in enumerate(items):
        pins = ""
        if k is not None:
            sl = slots[k]
            nin, nout = pin_counts(stem)
            if BAND_CHANNEL.get(sl["band"]):
                c = BAND_CHANNEL[sl["band"]]
                m = [1 << c, 1 << (c + 1)]
                pins = pinmap(nch, m + [0] * (nin - 2), m + [0] * (nout - 2))
            elif sl["channel"] in ("Mid", "Side"):
                c = 1 if sl["channel"] == "Mid" else 2
                pins = pinmap(nch, [c, c] + [0] * (nin - 2), ([c, 0] if c == 1 else [0, c]) + [0] * (nout - 2))
        blocks.append(fx_block(name, stem, values, gfx, seed=f"#{n}", pins=pins,
                               mods="".join(mods.get(k, [])) if k is not None else ""))
    return (f"REQUIRED_CHANNELS {nch}\n" if nch > 2 else "") + "".join(blocks)


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


def rack_slots(name, slots, routes, globs):
    """A RACK_PRESETS entry's slots and routes for the Rack: a plugin that is not a Rack module (a
    meter) stays in the REAPER chain but is left out, and the routes after it move up a slot."""
    keep = [k for k, sl in enumerate(slots) if (RACK / "modules" / f"{sl['id']}.hpp").exists()]
    if len(keep) == len(slots):
        return slots, routes
    if globs.get("feedback", 0) > 0:
        sys.exit(f"build_chains: {name}: a slot that is not a Rack module in a preset with the feedback loop")
    out = []
    for src, sn, key, depth in routes:
        if sn - 1 not in keep:
            sys.exit(f"build_chains: {name}: route to slot {sn} ({slots[sn - 1]['id']}), which is not a Rack module")
        out.append((src, keep.index(sn - 1) + 1, key, depth))
    return [slots[k] for k in keep], out


def render_rack():
    """RackPresets.hpp: Init, the CHAINS (modules the Rack has, in order) and RACK_PRESETS."""
    bands, channels = cpp_options("kBandOptions"), cpp_options("kChannelOptions")
    lanes, sources = cpp_options("kLaneOptions"), cpp_options("kSourceSymbols")
    presets = [("Init", "Init", [], [], {})]
    for chain, fxs in CHAINS:
        # a chain plugin that is not a Rack module (a meter) has nothing to do in the Rack
        presets.append((chain, "Chain", [slot(stem, values) for stem, values in fxs
                                         if (RACK / "modules" / f"{stem}.hpp").exists()], [], {}))
    presets += [(name, cat, *rack_slots(name, slots, routes, globs), globs)
                for name, cat, slots, routes, globs in RACK_PRESETS]
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
                out += [f"    {{ {module_index(sl['id'], i)}, {cfloat(v)}, {cstr(key)} }}," for i, v, key in vals]
                out.append("};")
            for what, opts in (("band", bands), ("channel", channels), ("lane", lanes)):
                if sl[what] not in opts:
                    sys.exit(f"build_chains: {name}: {what} {sl[what]!r} is not one of {opts}")
            lane = f", {lanes.index(sl['lane'])}" if sl["lane"] != lanes[0] else ""  # lane A: left out
            srows.append(f"    {{ {cstr(sl['id'])}, {arr}, {len(vals)}, {bands.index(sl['band'])}, "
                         f"{channels.index(sl['channel'])}, {cfloat(sl['mix'])}, {'true' if sl['on'] else 'false'}{lane} }},")
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
                i = module_index(slots[sn - 1]["id"], slider_index(name, slots[sn - 1]["id"], key))
                if not -1 <= depth <= 1:
                    sys.exit(f"build_chains: {name}: depth {depth} is outside -1..1")
                if src not in sources:
                    sys.exit(f"build_chains: {name}: no modulation source {src!r} (one of {sources})")
                out.append(f"    {{ {cstr(src)}, {sn - 1}, {i}, {cfloat(depth)}, {cstr(key)} }},")
            out.append("};")
        globs = dict(globs)
        steps = globs.pop("seq", [])
        if len(steps) > 16 or any(not 0 <= v <= 1 for v in steps):
            sys.exit(f"build_chains: {name}: seq takes up to 16 step values 0..1")
        globs.update({f"seq_step{k + 1}": v for k, v in enumerate(steps)})
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
            out.append(f"    {{ {cstr(path.stem)}, {module_index(path.stem, i)} }},")
    out += ["};", "static constexpr uint32_t kNumLogSliders = sizeof(kLogSliders) / sizeof(kLogSliders[0]);", "",
            "} // namespace rack", ""]
    return "\n".join(out)


def slider_index(where, stem, key):
    """The index (from 0) of the slider named key in a plugin."""
    hits = [i for i, sl in enumerate(parse_sliders(PLUGINS / f"{stem}.jsfx")) if sl[0] == key]
    if len(hits) != 1:
        sys.exit(f"build_chains: {where}: {stem} has no slider named {key!r}")
    return hits[0]

def num_chains():
    """How many REAPER chains there are: every chain, and the RACK_PRESETS that are not Rack-only."""
    return len(CHAINS) + sum(1 for name, _, slots, routes, globs in RACK_PRESETS
                             if not rack_only(name, slots, routes, globs))


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
        print(f"build_chains: up to date for all {num_chains()} chains and the Rack presets")
        return
    for p in existing - set(files):  # chain files only (the glob never sees RackPresets.hpp)
        p.unlink()
    for p, text in files.items():
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text, encoding="utf-8", newline="\n")
    print(f"build_chains: wrote {len(files) - 1} files ({num_chains()} chains, plain and GFX) and RackPresets.hpp")


if __name__ == "__main__":
    main()
