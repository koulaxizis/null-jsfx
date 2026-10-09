#!/usr/bin/env python3
"""Generates the plugin cards in index.html, the info-modal guides in assets/js/modals.js
and the plugin lists in README.md and llms.txt.

Names, categories and slider ranges come straight from the .jsfx headers, so the site can
never drift from the plugins. Everything a header can't tell (tagline, slider explanations,
uses, properties, icon) lives in tools/site_content.json, keyed by file name without .jsfx.

Usage: build_site.py           rewrite the generated blocks
       build_site.py --check   exit 1 if the site is out of date (used by CI)
"""
import html
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PLUGINS = ROOT / "DATA/Effects/null_jsfx"
GFX = PLUGINS / "gfx"
NATIVE = ROOT / "native/plugins"
CONTENT = ROOT / "tools/site_content.json"
INDEX = ROOT / "index.html"
MODALS = ROOT / "assets/js/modals.js"
README = ROOT / "README.md"
LLMS = ROOT / "llms.txt"

CATEGORIES = [
    ("One Slider", "one-slider", "One control, instant result. Turn it up until it sounds right."),
    ("Essentials", "essentials", "The everyday mixing and mastering toolbox."),
    ("Advanced", "advanced", "Deeper sound design, creative effects and special processing."),
]


def number(v):
    v = float(v)
    s = str(int(v)) if v == int(v) else "%g" % v
    return s.replace("-", "−")


def parse_sliders(text):
    sliders = []
    for line in text.split("\n"):
        m = re.match(r"slider\d+:(.*)", line)
        if not m:
            continue
        body = m.group(1)
        if body.startswith("/"):  # file slider: /folder:default:Label
            sliders.append((body.split(":")[-1], "WAV file in REAPER/Data/" + body[1:].split(":")[0]))
            continue
        rng, label = re.match(r"[^<]*<([^>]*)>(.*)", body).groups()
        label = label.lstrip("-")
        unit = re.search(r"\(([^)]*)\)\s*$", label)
        name = re.sub(r"\s*\([^)]*\)\s*$", "", label).strip()
        if "{" in rng:
            rng = " / ".join(o.strip() for o in re.search(r"\{(.*)\}", rng).group(1).split(","))
        else:
            lo, hi = (float(x) for x in rng.split(",")[:2])
            lo_s, hi_s = number(lo), number(hi)
            if lo < 0 < hi:
                hi_s = "+" + hi_s
            rng = f"{lo_s} to {hi_s}"
            if unit:
                rng += unit.group(1) if unit.group(1) == "%" else " " + unit.group(1)
        sliders.append((name, rng))
    return sliders


def formats(stem):
    """The versions a plugin ships in: always JSFX, plus GFX and VST3/CLAP when they exist."""
    out = ["JSFX"]
    if (GFX / f"{stem}_gfx.jsfx").exists():
        out.append("GFX")
    if (NATIVE / stem / "Plugin.cpp").exists():
        out += ["VST3", "CLAP"]
    return out


def load():
    content = json.loads(CONTENT.read_text(encoding="utf-8"))
    plugins, errors = [], []
    files = sorted(PLUGINS.glob("*.jsfx"))
    unlisted = [p.name for p in files if p.stem not in content]
    if unlisted:
        print(f"build_site: not on the site (no entry in {CONTENT.name}): {' '.join(unlisted)}", file=sys.stderr)
    for path in files:
        stem = path.stem
        if stem not in content:
            continue
        lines = path.read_text(encoding="utf-8").split("\n")
        category = lines[3].split(": ", 1)[1]
        c = content[stem]
        sliders = parse_sliders("\n".join(lines))
        if len(c["slider_desc"]) != len(sliders):
            errors.append(f"{stem}: {len(sliders)} sliders but {len(c['slider_desc'])} slider_desc entries")
            continue
        plugins.append({
            "stem": stem,
            "key": stem.replace("_", "-"),
            "name": c.get("name") or lines[0][len("desc:NULL "):],
            "category": category,
            "sliders": [(n, r, d) for (n, r), d in zip(sliders, c["slider_desc"])],
            "formats": formats(stem),
            **{k: c[k] for k in ("card_desc", "keywords", "uses", "properties", "icon")},
        })
    for stem in sorted(set(content) - {p.stem for p in files}):
        errors.append(f"{stem}: in {CONTENT.name} but there is no {stem}.jsfx")
    if errors:
        sys.exit("build_site: " + "\n  ".join(["content does not match the plugins:"] + errors))
    return sorted(plugins, key=lambda p: p["name"].lower())


def card(p):
    e = html.escape
    icon = re.sub(r"\s*\n\s*", "\n                            ", p["icon"].strip())
    icon = icon.replace("\n                            </svg>", "\n                        </svg>")
    keywords = p["keywords"]
    badges = ""
    if len(p["formats"]) > 1:
        keywords += " " + " ".join(f.lower() for f in p["formats"][1:])
        spans = "".join(f"<span>{f}</span>" for f in p["formats"])
        badges = f"""
                        <div class="plugin-formats">{spans}</div>"""
    return f"""                <article class="plugin-card" data-name="{e(keywords)}" onclick="openGuide('{p['key']}')">
                    <div class="plugin-icon">
                        {icon}
                    </div>
                    <div class="plugin-info">
                        <h3>{e(p['name'].upper())}</h3>
                        <p class="plugin-desc">{e(p['card_desc'])}</p>{badges}
                        <span class="info-badge">?</span>
                    </div>
                </article>
"""


def cards_block(plugins):
    out = []
    for title, slug, desc in CATEGORIES:
        group = [p for p in plugins if p["category"] == title]
        out.append(f"""            <div class="plugin-category" id="{slug}">
                <h3 class="category-title">{title} <span class="category-count">{len(group)}</span></h3>
                <p class="category-desc">{desc}</p>
                <div class="plugin-grid">
""")
        out += [card(p) for p in group]
        out.append("""                </div>
            </div>
""")
    return "".join(out)


def js(s):
    return "'" + s.replace("\\", "\\\\").replace("'", "\\'") + "'"


def guides_block(plugins):
    out = ["const guides = {\n"]
    for title, _, _ in CATEGORIES:
        out.append(f"\n  // ─── {title.upper()} ───\n")
        for p in (p for p in plugins if p["category"] == title):
            sliders = ",\n".join(
                f"      {{ name: {js(n)}, range: {js(r)}, desc: {js(d)} }}" for n, r, d in p["sliders"])
            uses = ", ".join(js(u) for u in p["uses"])
            out.append(f"""  {js(p['key'])}: {{
    title: {js(p['name'].upper())},
    subtitle: {js(p['card_desc'])},
    category: {js(title)},
    sliders: [
{sliders}
    ],
    uses: [{uses}],
    properties: {js(p['properties'])},
    formats: [{", ".join(js(f) for f in p['formats'])}]
  }},
""")
    out[-1] = out[-1].rstrip(",\n") + "\n"
    out.append("};\n")
    return "".join(out)


def list_block(plugins, with_desc):
    out = []
    for title, _, desc in CATEGORIES:
        group = [p for p in plugins if p["category"] == title]
        if with_desc:
            out.append(f"\n### {title} ({len(group)})\n{desc}\n\n")
            out += [f"- {p['name']}: {p['card_desc']}"
                    + (f" (also as {', '.join(p['formats'][1:])})" if len(p['formats']) > 1 else "") + "\n"
                    for p in group]
        else:
            out.append(f"| **{title}** ({len(group)}) | {', '.join(p['name'] for p in group)} |\n")
    return ("| Category | Plugins |\n|----------|---------|\n" if not with_desc else "") + "".join(out)


def replace_between(text, begin, end, new):
    i, j = text.index(begin) + len(begin), text.index(end)
    return text[:i] + "\n" + new + text[j:]


def main(check):
    plugins = load()
    counts = {t: sum(p["category"] == t for p in plugins) for t, _, _ in CATEGORIES}
    marker = "generated by tools/build_site.py"
    targets = [
        (INDEX, "<!-- BEGIN PLUGINS: " + marker + " -->", "            <!-- END PLUGINS -->", cards_block(plugins)),
        (MODALS, "// BEGIN GUIDES: " + marker, "// END GUIDES", guides_block(plugins)),
        (README, "<!-- BEGIN PLUGIN LIST: " + marker + " -->", "<!-- END PLUGIN LIST -->", list_block(plugins, False)),
        (LLMS, "<!-- BEGIN PLUGIN LIST: " + marker + " -->", "<!-- END PLUGIN LIST -->", list_block(plugins, True)),
    ]
    files = []
    for path, begin, end, block in targets:
        old = path.read_text(encoding="utf-8")
        if begin not in old:
            # a file without the markers is left alone, e.g. the live index.html before a launch
            print(f"build_site: {path.name} has no generated block, left as is", file=sys.stderr)
            continue
        new = replace_between(old, begin, end, block)
        new = re.sub(r"\b\d+ (professional audio effects)", f"{len(plugins)} \\1", new)
        files.append((path, old, new))
    stale = [f.name for f, old, new in files if old != new]
    if check:
        if stale:
            sys.exit(f"build_site: {', '.join(stale)} out of date; run tools/build_site.py")
        print(f"build_site: up to date for all {len(plugins)} plugins {counts}")
        return
    for path, _, new in files:
        path.write_text(new, encoding="utf-8")
    print(f"build_site: {len(plugins)} plugins {counts}; updated {', '.join(stale) or 'nothing'}")


if __name__ == "__main__":
    main("--check" in sys.argv[1:])
