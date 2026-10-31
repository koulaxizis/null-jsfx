#!/usr/bin/env python3
"""Writes the rule tables of the Assisted plugins from tools/assisted_targets.json.

The numbers (targets, sources, EQ flavours and source references, compressor reductions,
ratios, attack and release rules, loudness targets, reverb decays, sizes, pre-delays, filters,
levels and ducking, delay notes, repeats, filters, levels and ducking) live only in the JSON. This script writes
them into:

  * the generated block of each Assisted JSFX and its GFX version, between the lines
        // BEGIN tables (tools/build_assisted.py, do not edit)
        // END tables
    in @init (EEL2 memory tables: eel_eq, eel_comp, and FX_TABLES / FX_SCALARS for Reverb
    and Delay);
  * native/common/AssistedTargets.hpp, the same numbers for the VST3/CLAP ports.

It also checks that the Target and Source sliders of every Assisted JSFX list the targets and
sources of the JSON, in the same order.

Usage: build_assisted.py [--check]   (--check: exit 1 if a file is not up to date)
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "tools/assisted_targets.json"
JSFX = ROOT / "DATA/Effects/null_jsfx"
HEADER = ROOT / "native/common/AssistedTargets.hpp"
PLUGINS = {"assisted_eq": "eq", "assisted_comp": "comp", "assisted_reverb": "reverb", "assisted_delay": "delay"}

# Assisted Reverb and Delay: (EEL2 table, C++ array, JSON key, "t" per target / "s" per source),
# one table every 8 slots from 0; then (EEL2 scalar, C++ constant, JSON key[, index])
FX_TABLES = {
    "reverb": [("as_dec", "kRvDecay", "decay", "t"), ("as_decm", "kRvDecayMul", "decay_mul", "s"),
               ("as_size", "kRvSize", "size", "t"), ("as_pdm", "kRvPredelayMul", "predelay_mul", "t"),
               ("as_lc", "kRvLowCut", "lowcut", "s"), ("as_lct", "kRvLowCutTarget", "lowcut_target", "t"),
               ("as_hc", "kRvHighCut", "highcut", "t"), ("as_mix", "kRvMix", "mix", "s"),
               ("as_mixm", "kRvMixMul", "mix_mul", "t"), ("as_duck", "kRvDuck", "duck", "s"),
               ("as_duckm", "kRvDuckMul", "duck_mul", "t")],
    "delay": [("as_note", "kDlNote", "note", "t"), ("as_pp", "kDlPingPong", "pingpong", "t"),
              ("as_rep", "kDlRepeats", "repeats", "t"), ("as_lc", "kDlLowCut", "lowcut", "s"),
              ("as_hc", "kDlHighCut", "highcut", "t"), ("as_mix", "kDlMix", "mix", "s"),
              ("as_mixm", "kDlMixMul", "mix_mul", "t"), ("as_duck", "kDlDuck", "duck", "s"),
              ("as_duckm", "kDlDuckMul", "duck_mul", "t")],
}
FX_SCALARS = {
    "reverb": [("as_dec_on", "kRvDecayOnsets", "decay_onsets"), ("as_dec_min", "kRvDecayMin", "decay_min"),
               ("as_pd_share", "kRvPredelayShare", "predelay_share"), ("as_pd_lo", "kRvPredelayMin", "predelay_range", 0),
               ("as_pd_hi", "kRvPredelayMax", "predelay_range", 1), ("as_pd_none", "kRvPredelayNone", "predelay_none"),
               ("as_br_ref", "kRvBrightRef", "bright_ref"), ("as_br_db", "kRvBrightDb", "bright_db"),
               ("as_hc_lo", "kRvHighCutMin", "highcut_range", 0), ("as_hc_hi", "kRvHighCutMax", "highcut_range", 1)],
    "delay": [("as_slap", "kDlSlapMs", "slap_ms"), ("as_br_ref", "kDlBrightRef", "bright_ref"),
              ("as_br_db", "kDlBrightDb", "bright_db"), ("as_hc_lo", "kDlHighCutMin", "highcut_range", 0),
              ("as_hc_hi", "kDlHighCutMax", "highcut_range", 1)],
}


def fx_values(d, kind, key, per):
    return [d[kind][key][k] for k in (d["targets"] if per == "t" else d["sources"])]


def fx_scalar(d, kind, spec):
    v = d[kind][spec[2]]
    return v[spec[3]] if len(spec) > 3 else v


def eel_fx(d, kind):
    tabs = FX_TABLES[kind]
    lines = [" ".join(f"{t[0]} = {8 * i};" for i, t in enumerate(tabs))]
    for name, _, key, per in tabs:
        lines += table(name, fx_values(d, kind, key, per))
    sc = [f"{s[0]} = {num(fx_scalar(d, kind, s))};" for s in FX_SCALARS[kind]]
    for i in range(0, len(sc), 6):
        lines.append(" ".join(sc[i:i + 6]))
    return lines


def cpp_fx(d, kind):
    out = []
    for _, cname, key, per in FX_TABLES[kind]:
        out.append(cpp_array(cname, fx_values(d, kind, key, per)))
    for s in FX_SCALARS[kind]:
        out.append(f"inline constexpr double {s[1]} = {num(fx_scalar(d, kind, s))};")
    return out
BEGIN = "// BEGIN tables (tools/build_assisted.py, do not edit)"
END = "// END tables"


def num(v):
    v = float(v)
    return str(int(v)) if v == int(v) else repr(v)


def table(name, values):
    """One EEL2 line per 8 values: name[i] = v; ..."""
    out, row = [], []
    for i, v in enumerate(values):
        row.append(f"{name}[{i}] = {num(v)};")
        if len(row) == 8:
            out.append(" ".join(row))
            row = []
    if row:
        out.append(" ".join(row))
    return out


def eel_eq(d):
    t, s, e = d["targets"], d["sources"], d["eq"]
    lines = ["as_flav = 0; as_thp = 40; as_tlp = 48; as_sref = 56; as_shpl = 88; as_shph = 96;"]
    lines += table("as_flav", [v for k in t for v in e["flavor"][k]])
    lines += table("as_thp", [e["hpf"][k] for k in t])
    lines += table("as_tlp", [e["lpf"][k] for k in t])
    lines += table("as_sref", [v for k in s for v in e["source_ref"][k]])
    lines += table("as_shpl", [e["source_hpf"][k][0] for k in s])
    lines += table("as_shph", [e["source_hpf"][k][1] for k in s])
    lines.append(f"as_k = {num(e['correction'])}; as_dead = {num(e['deadband'])}; "
                 f"as_cut = {num(e['max_cut'])}; as_boost = {num(e['max_boost'])};")
    return lines


def eel_comp(d):
    t, s, c = d["targets"], d["sources"], d["comp"]
    lines = ["as_red = 0; as_ratio = 8; as_radd = 16; as_amul = 24; as_loud = 32;"]
    lines += table("as_red", [c["reduction"][k] for k in t])
    lines += table("as_ratio", [c["ratio"][k] for k in s])
    lines += table("as_radd", [c["ratio_add"][k] for k in t])
    lines += table("as_amul", [c["attack_mul"][k] for k in t])
    lines += table("as_loud", [c["loudness"][k] for k in t])
    lines.append(f"as_att_lo = {num(c['attack'][0])}; as_att_hi = {num(c['attack'][1])}; "
                 f"as_rel = {num(c['release'])}; as_rel_min = {num(c['release_min'])}; "
                 f"as_rel_max = {num(c['release_max'])}; as_rel_none = {num(c['release_none'])}; "
                 f"as_lshare = {num(c['loudness_share'])};")
    return lines


def cpp_array(name, values, cols=None):
    if cols:
        rows = [values[i:i + cols] for i in range(0, len(values), cols)]
        body = ",\n".join("    { " + ", ".join(num(v) for v in r) + " }" for r in rows)
        return f"inline constexpr double {name}[{len(rows)}][{cols}] = {{\n{body}\n}};"
    return f"inline constexpr double {name}[{len(values)}] = {{ " + ", ".join(num(v) for v in values) + " };"


def header(d):
    t, s, e, c = d["targets"], d["sources"], d["eq"], d["comp"]
    out = [
        "/*",
        " * NULL JSFX - the rule tables of the Assisted plugins (Assisted EQ, Comp, Reverb, Delay)",
        " * GENERATED by tools/build_assisted.py from tools/assisted_targets.json; do not edit.",
        " * SPDX-License-Identifier: MIT",
        " */",
        "",
        "#pragma once",
        "",
        "namespace nullassisted {",
        "",
        f"inline constexpr int kTargets = {len(t)};  // " + ", ".join(t),
        f"inline constexpr int kSources = {len(s)};  // " + ", ".join(s),
        "",
        "// Assisted EQ: regions low, mud, honk, presence, air",
        cpp_array("kEqFlavor", [v for k in t for v in e["flavor"][k]], 5),
        cpp_array("kEqTargetHpf", [e["hpf"][k] for k in t]),
        cpp_array("kEqTargetLpf", [e["lpf"][k] for k in t]),
        cpp_array("kEqSourceRef", [v for k in s for v in e["source_ref"][k]], 5),
        cpp_array("kEqSourceHpfLo", [e["source_hpf"][k][0] for k in s]),
        cpp_array("kEqSourceHpfHi", [e["source_hpf"][k][1] for k in s]),
        f"inline constexpr double kEqCorrection = {num(e['correction'])};",
        f"inline constexpr double kEqDeadband = {num(e['deadband'])};",
        f"inline constexpr double kEqMaxCut = {num(e['max_cut'])};",
        f"inline constexpr double kEqMaxBoost = {num(e['max_boost'])};",
        "",
        "// Assisted Comp",
        cpp_array("kCompReduction", [c["reduction"][k] for k in t]),
        cpp_array("kCompRatio", [c["ratio"][k] for k in s]),
        cpp_array("kCompRatioAdd", [c["ratio_add"][k] for k in t]),
        cpp_array("kCompAttackMul", [c["attack_mul"][k] for k in t]),
        cpp_array("kCompLoudness", [c["loudness"][k] for k in t]),
        f"inline constexpr double kCompAttackLo = {num(c['attack'][0])};",
        f"inline constexpr double kCompAttackHi = {num(c['attack'][1])};",
        f"inline constexpr double kCompRelease = {num(c['release'])};",
        f"inline constexpr double kCompReleaseMin = {num(c['release_min'])};",
        f"inline constexpr double kCompReleaseMax = {num(c['release_max'])};",
        f"inline constexpr double kCompReleaseNone = {num(c['release_none'])};",
        f"inline constexpr double kCompLoudnessShare = {num(c['loudness_share'])};",
        "",
        "// Assisted Reverb",
        *cpp_fx(d, "reverb"),
        "",
        "// Assisted Delay",
        *cpp_fx(d, "delay"),
        "",
        "} // namespace nullassisted",
        "",
    ]
    return "\n".join(out)


def options(text, label):
    m = re.search(r"^slider\d+:[^<]*<[^{]*\{([^}]*)\}>-?" + re.escape(label) + r"\s*$", text, flags=re.M)
    return [o.strip() for o in m.group(1).split(",")] if m else None


def splice(text, lines, path):
    start, end = text.find(BEGIN), text.find(END)
    if start < 0 or end < start:
        sys.exit(f"build_assisted: {path.name}: no generated block ({BEGIN!r} ... {END!r})")
    indent = text[text.rfind("\n", 0, start) + 1:start]
    body = "".join(f"{indent}{l}\n" for l in lines)
    return text[:start] + BEGIN + "\n" + body + indent + text[end:]


def main(argv):
    check = "--check" in argv
    d = json.loads(SRC.read_text(encoding="utf-8"))
    stale, errors = [], []
    files = {}
    for stem, kind in PLUGINS.items():
        lines = eel_eq(d) if kind == "eq" else eel_comp(d) if kind == "comp" else eel_fx(d, kind)
        for path in (JSFX / f"{stem}.jsfx", JSFX / "gfx" / f"{stem}_gfx.jsfx"):
            if not path.exists():
                continue
            text = path.read_text(encoding="utf-8")
            if options(text, "Target") != d["targets"]:
                errors.append(f"{path.name}: the Target slider must list {', '.join(d['targets'])}")
            if options(text, "Source") != d["sources"]:
                errors.append(f"{path.name}: the Source slider must list {', '.join(d['sources'])}")
            files[path] = splice(text, lines, path)
    files[HEADER] = header(d)
    for e in errors:
        print(f"build_assisted: {e}")
    for path, new in files.items():
        old = path.read_text(encoding="utf-8") if path.exists() else None
        if old != new:
            stale.append(path)
            if not check:
                path.write_text(new, encoding="utf-8")
    if check and stale:
        for p in stale:
            print(f"build_assisted: {p.relative_to(ROOT)} is not up to date; run tools/build_assisted.py")
    if errors or (check and stale):
        return 1
    if not check:
        print(f"build_assisted: {len(stale)} file(s) written")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
