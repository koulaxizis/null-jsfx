#!/usr/bin/env python3
"""Checks that every NULL JSFX plugin follows the suite's header and code conventions.

Header (first lines of every file, in this order):

    desc:NULL <Name>
    author: Christos Koulaxizis
    tags: <lowercase keywords>
    //category: One Slider | Essentials | Advanced
    //NULL JSFX v2.0 • https://nulljsfx.tech • MIT License

followed by a blank line, the sliders, a blank line and the four pins
(in_pin:Left, in_pin:Right, out_pin:Left, out_pin:Right); sidechain plugins add
in_pin:Sidechain Left and in_pin:Sidechain Right after the two main inputs.

Usage: lint.py file.jsfx [...]   Exit status 1 if any file has an error.
"""
import re
import sys
from pathlib import Path

AUTHOR = "author: Christos Koulaxizis"
FOOTER = "//NULL JSFX v2.0 • https://nulljsfx.tech • MIT License"
CATEGORIES = ("One Slider", "Essentials", "Advanced")
PINS = ["in_pin:Left", "in_pin:Right", "out_pin:Left", "out_pin:Right"]
SIDECHAIN_PINS = ["in_pin:Left", "in_pin:Right", "in_pin:Sidechain Left", "in_pin:Sidechain Right",
                  "out_pin:Left", "out_pin:Right"]

# EEL2 built-ins and keywords that may be read without being assigned first.
BUILTINS = set("""
spl0 spl1 srate samplesblock tempo play_state play_position beat_position ts_num ts_denom num_ch trigger
sin cos tan asin acos atan atan2 sqrt pow exp log log10 abs min max sign floor ceil rand invsqrt sqr
memset memcpy freembuf loop while this local static instance global
fft ifft fft_permute fft_ipermute fft_real ifft_real convolve_c file_open file_close file_avail file_riff file_mem file_var
pdc_delay pdc_bot_ch pdc_top_ch ext_noinit ext_tail_size ext_nodenorm slider_automate sliderchange
""".split())


def strip_comments(code):
    code = re.sub(r"/\*.*?\*/", "", code, flags=re.S)
    return re.sub(r"//[^\n]*", "", code)


def check(path):
    errors = []
    text = path.read_text(encoding="utf-8")
    lines = text.split("\n")

    # --- header ---
    if not re.fullmatch(r"desc:NULL [A-Z0-9][A-Za-z0-9/ \-]*[A-Za-z0-9]", lines[0]):
        errors.append(f"line 1 must be 'desc:NULL <Name>', found {lines[0]!r}")
    if len(lines) < 5 or lines[1] != AUTHOR:
        errors.append(f"line 2 must be {AUTHOR!r}")
    if len(lines) < 5 or not re.fullmatch(r"tags: [a-z0-9][a-z0-9 \-/]*", lines[2]):
        errors.append("line 3 must be 'tags: <lowercase keywords>'")
    m = re.fullmatch(r"//category: (.+)", lines[3]) if len(lines) > 3 else None
    category = m.group(1) if m else None
    if category not in CATEGORIES:
        errors.append(f"line 4 must be '//category: <{' | '.join(CATEGORIES)}>'")
    if len(lines) < 5 or lines[4] != FOOTER:
        errors.append(f"line 5 must be {FOOTER!r}")

    header_end = next((i for i, l in enumerate(lines) if l.startswith("@")), len(lines))
    header = lines[:header_end]
    sliders = [l for l in header if re.match(r"slider\d+:", l)]
    if not sliders:
        errors.append("no sliders")
    for l in sliders:
        name = re.sub(r"^slider\d+:[^>]*>", "", l)
        if re.search(r"-\(|-%|\w-\w*\)?$", name.lstrip("-")) and "/" not in name:
            errors.append(f"slider label style must be 'Name (unit)': {name!r}")
    if category == "One Slider" and len(sliders) != 1:
        errors.append(f"One Slider plugin has {len(sliders)} sliders")
    if category in ("Essentials", "Advanced") and len(sliders) == 1:
        errors.append(f"{category} plugin has only one slider; should it be One Slider?")
    pins = [l.strip() for l in header if l.startswith(("in_pin:", "out_pin:"))]
    if pins not in (PINS, SIDECHAIN_PINS):
        errors.append("pins must be in_pin:Left, in_pin:Right, out_pin:Left, out_pin:Right "
                      "(plus in_pin:Sidechain Left/Right after the main inputs for sidechain plugins)")

    # --- code ---
    code = strip_comments("\n".join(lines[header_end:]))
    for pat, msg in [
        (r"\bif\s*\(", "C-style 'if' (EEL2 uses cond ? (a) : (b))"),
        (r"[{}]", "braces are not EEL2 syntax"),
        (r"\breturn\b", "'return' is not EEL2"),
        (r"\bint\s*\(", "'int()' is not EEL2 (use floor())"),
        (r"\belse\b", "'else' is not EEL2"),
    ]:
        if re.search(pat, code):
            errors.append(msg)
    if not re.search(r"\bspl0\s*(=|\+=|\*=|-=)", code) or not re.search(r"\bspl1\s*(=|\+=|\*=|-=)", code):
        errors.append("never writes spl0/spl1 (plugin output)")
    if "@sample" not in code:
        errors.append("no @sample section")

    # Variables read but never assigned anywhere evaluate to 0 in EEL2 (e.g. 'sr' instead of 'srate').
    assigned = set(re.findall(r"\b([A-Za-z_]\w*)(?:\.\w+)*\s*(?:=|\+=|-=|\*=|/=|\|=|&=)(?!=)", code))
    functions, params = set(), set()
    for name, args, extra in re.findall(r"function\s+([\w.]+)\s*\(([^)]*)\)\s*(?:(?:local|instance|static|global)\s*\(([^)]*)\))?", code):
        functions.add(name.split(".")[-1])
        params |= {a.strip() for a in (args + "," + extra).split(",") if a.strip()}
    used = set(re.findall(r"(?<![\w.$#@'\"])([A-Za-z_]\w*)\b(?!\s*\(|\.)", code))
    called = set(re.findall(r"(?<![\w.])([A-Za-z_]\w*)\s*\(", code))
    for name in sorted(used - assigned - BUILTINS - functions - params):
        if re.fullmatch(r"slider\d+|spl\d+|function|local|instance|static|global", name):
            continue
        errors.append(f"'{name}' is read but never assigned (EEL2 treats it as 0)")
    for name in sorted(called - functions - BUILTINS):
        if name in ("function", "local", "instance", "static", "global", "loop", "while"):
            continue
        errors.append(f"'{name}()' is not an EEL2 function")
    return errors


def main(paths):
    failed = 0
    for p in map(Path, paths):
        errs = check(p)
        if errs:
            failed += 1
            print(f"LINT {p.name}")
            for e in errs:
                print(f"    {e}")
    print(f"\nlint: {failed} of {len(paths)} plugins have convention errors")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
