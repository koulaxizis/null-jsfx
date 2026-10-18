#!/usr/bin/env python3
"""Generates the plugin cards and the FX chain cards (two tabs) in index.html, the info-modal guides in assets/js/modals.js
and the plugin lists in README.md and llms.txt.

Names, categories and slider ranges come straight from the .jsfx headers, so the site can
never drift from the plugins. Everything a header can't tell (tagline, slider explanations,
uses, properties, icon) lives in tools/site_content.json, keyed by file name without .jsfx.
The FX chains come from tools/build_chains.py, with one line of text each in
tools/site_chains.json.

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
SHOTS = ROOT / "assets/img/gfx"  # GFX interface screenshots, made by tools/render_gfx_shots.sh
VIDEOS = ROOT / "assets/video"  # presentation videos and their posters, made by tools/video/make_videos.py
CONTENT = ROOT / "tools/site_content.json"
INDEX = ROOT / "index.html"
MODALS = ROOT / "assets/js/modals.js"
README = ROOT / "README.md"
LLMS = ROOT / "llms.txt"
STUDIO_FX = ROOT / "web/fx/index.json"  # the effects orOS Studio runs (web version)
CHAIN_TEXT = ROOT / "tools/site_chains.json"

sys.dont_write_bytecode = True  # no __pycache__ in tools/
sys.path.insert(0, str(ROOT / "tools"))
from build_chains import CHAINS, RACK_PRESETS, rack_only  # noqa: E402

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
    studio = {fx["id"] for fx in json.loads(STUDIO_FX.read_text(encoding="utf-8"))["effects"]}
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
            "shot": f"assets/img/gfx/{stem}.png" if (SHOTS / f"{stem}.png").exists() else None,
            "video": f"assets/video/{stem}.mp4" if (VIDEOS / f"{stem}.mp4").exists() else None,
            "studio": stem in studio,
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


def chain_cards(plugins):
    """The FX chain cards; a step links to its plugin's guide when the plugin is on the site."""
    e = html.escape
    text = json.loads(CHAIN_TEXT.read_text(encoding="utf-8"))
    # (name, [(plugin, tag)], what REAPER routing it uses) for the plain chains and the routing ones
    chains = [(c, [(stem, "") for stem, _ in fxs], []) for c, fxs in CHAINS]
    for c, _, slots, routes, globs in RACK_PRESETS:
        if rack_only(c, slots, routes, globs):
            continue  # no REAPER chain
        uses = []
        if any(sl["band"] != "Full" or sl["channel"] != "Stereo" for sl in slots):
            uses.append("Pin routing")
        if routes:
            uses.append("Modulation")
        steps = [(sl["id"], sl["band"] if sl["band"] != "Full" else sl["channel"] if sl["channel"] != "Stereo" else "")
                 for sl in slots]
        chains.append((c, steps, uses))
    names = [c for c, _, _ in chains]
    if sorted(text) != sorted(names):
        sys.exit(f"build_site: {CHAIN_TEXT.name} must have one entry per chain in build_chains.py: "
                 f"missing {sorted(set(names) - set(text))}, extra {sorted(set(text) - set(names))}")
    on_site = {p["stem"]: p for p in plugins}
    nxt = ROOT / "tools/site_content_next.json"
    waiting = set(json.loads(nxt.read_text(encoding="utf-8"))) if nxt.exists() else set()
    out = []
    for chain, steps_in, uses in sorted(chains, key=lambda c: c[0].lower()):
        if any(stem in waiting for stem, _ in steps_in):
            continue  # a plugin of the chain still waits off the site (site_content_next.json): so does the chain
        steps = []
        for stem, tag in steps_in:
            label = on_site[stem]["name"] if stem in on_site else \
                (PLUGINS / f"{stem}.jsfx").read_text(encoding="utf-8").split("\n", 1)[0][len("desc:NULL "):]
            label = e(label) + (f' <small>{e(tag)}</small>' if tag else "")
            if stem in on_site:
                steps.append(f"""<button type="button" class="chain-step" onclick="openGuide('{on_site[stem]['key']}')">{label}</button>""")
            else:
                steps.append(f"""<span class="chain-step">{label}</span>""")
        arrow = '<span class="chain-arrow" aria-hidden="true">&#8594;</span>'
        tags = "".join(f"<span>{t}</span>" for t in ["JSFX", "GFX"] + [u.upper() for u in uses])
        out.append(f"""                <article class="chain-card">
                    <h3>{e(chain.upper())}</h3>
                    <p class="plugin-desc">{e(text[chain])}</p>
                    <div class="chain-steps">{arrow.join(steps)}</div>
                    <div class="plugin-formats">{tags}</div>
                </article>
""")
    return "".join(out)


def tabs_block(plugins):
    """Effects and FX Chains tabs: the effect categories and the chain cards."""
    chains = chain_cards(plugins)
    return f"""            <div class="plugin-tabs" role="tablist">
                <button type="button" class="plugin-tab active" role="tab" aria-selected="true" aria-controls="tab-effects" id="tab-btn-effects" data-tab="effects">Effects <span class="category-count">{len(plugins)}</span></button>
                <button type="button" class="plugin-tab" role="tab" aria-selected="false" aria-controls="tab-chains" id="tab-btn-chains" data-tab="chains">FX Chains <span class="category-count">{chains.count("<article")}</span></button>
            </div>
            <div class="tab-panel" id="tab-effects" role="tabpanel" aria-labelledby="tab-btn-effects">
{cards_block(plugins)}            </div>
            <div class="tab-panel" id="tab-chains" role="tabpanel" aria-labelledby="tab-btn-chains" hidden>
                <p class="category-desc chains-desc">Ready-made REAPER FX chains of NULL JSFX plugins, in the JSFX download (<code>FXChains</code> folder, see <code>INSTALL.txt</code>). Load one with FX &#8594; Add FX chain; every plugin stays fully editable. Chains marked pin routing split the sound into bands or into mid and side; modulation ones move a control with an LFO, the input level or another control. Click a plugin for its guide.</p>
                <div class="chain-grid">
{chains}                </div>
            </div>
"""


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
    formats: [{", ".join(js(f) for f in p['formats'])}]""" + (f""",
    shot: {js(p['shot'])}""" if p["shot"] else "") + (f""",
    video: {js(p['video'])}""" if p["video"] else "") + (""",
    studio: true""" if p["studio"] else "") + """
  },
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
        (INDEX, "<!-- BEGIN PLUGINS: " + marker + " -->", "            <!-- END PLUGINS -->", tabs_block(plugins)),
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
