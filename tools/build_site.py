#!/usr/bin/env python3
"""Generates the plugin cards and the FX chain cards (Effects, Instruments, MIDI and FX Chains tabs) in index.html, the info-modal guides in assets/js/modals.js
and the plugin lists in README.md and llms.txt.

Names, categories and slider ranges come straight from the .jsfx headers, so the site can
never drift from the plugins. Everything a header can't tell (tagline, slider explanations,
uses, properties, icon) lives in tools/site_content.json, keyed by file name without .jsfx.
The FX chains come from tools/build_chains.py, with one line of text each in
tools/site_chains.json.

Every plugin also gets its own page, plugins/<key>/index.html, and sitemap.xml lists them.

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
PAGES = ROOT / "plugins"  # one page per plugin: plugins/<key>/index.html
DEMOS = ROOT / "assets/audio/demos"  # before/after audio, made by tools/render_demos.py
PRESETS = ROOT / "tools/presets"
SITEMAP = ROOT / "sitemap.xml"
SITE = "https://nulljsfx.tech/"
# the videos stream from the "media" release (.github/workflows/media.yml); the site's copy is the fallback
MEDIA_VIDEOS = "https://github.com/koulaxizis/null-jsfx/releases/download/media/"
VERSION = re.search(r'id="release-version">v([\d.]+)<', INDEX.read_text(encoding="utf-8")).group(1)

sys.dont_write_bytecode = True  # no __pycache__ in tools/
sys.path.insert(0, str(ROOT / "tools"))
from build_chains import CHAINS, RACK_PRESETS, rack_only  # noqa: E402

CATEGORIES = [
    ("One Slider", "one-slider", "One control, instant result. Turn it up until it sounds right."),
    ("Essentials", "essentials", "The everyday mixing and mastering toolbox."),
    ("Advanced", "advanced", "Deeper sound design, creative effects and special processing."),
    ("Assisted", "assisted", "Effects that set themselves: press Learn, play the track, pick a target."),
    ("Instruments", "instruments", "Synths, keys, bass and drums. Play them from MIDI, no samples needed."),
    ("MIDI", "midi", "Tools that change the notes before they reach an instrument."),
]


# the use filter above the plugin tabs: (label, words in a plugin's keywords, uses or tagline)
TAGS = [
    ("Vocals", r"vocal|voice|sibilan|breath|dialog|podcast|singer"),
    ("Drums", r"drum|kick|snare|percussion|hi-?hat|cymbal|transient|groove"),
    ("Bass", r"\bbass|\bsub\b|808|low end"),
    ("Guitar", r"guitar|\bamp\b|amplifier|cabinet|pedal"),
    ("Synths & Keys", r"synth|\bpads?\b|\bkeys\b|piano|\bleads?\b|organ"),
    ("Mastering", r"master|loudness|lufs|limit|dither"),
    ("Stereo", r"stereo|width|mid/side|mid-side|\bpan|mono"),
    ("Space", r"reverb|delay|echo|\broom\b|ambien|hall"),
    ("Modulation", r"chorus|flang|phaser|tremolo|vibrato|\blfo|modulat|rotary"),
    ("Character", r"lo-?fi|tape|vinyl|saturat|distort|crunch|grit|warm|tube|analog"),
    ("Repair", r"repair|denois|noise reduction|\bhum\b|clicks|plosive|resonance suppress|harshness|cleanup|clean up"),
    ("Creative", r"glitch|stutter|granular|freeze|riser|transition|sound design|texture"),
]


def tags(p):
    # the keywords, the tagline and the first two uses: later uses list every source a plugin can take
    text = " ".join([p["keywords"], p["card_desc"], *p["uses"][:2]]).lower()
    return [label for label, rx in TAGS if re.search(rx, text)]


def demos():
    index = DEMOS / "demos.json"
    return json.loads(index.read_text(encoding="utf-8")) if index.exists() else {}


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
    """The versions a plugin ships in: always JSFX, plus GFX and VST3/CLAP/LV2 when they exist (LV2 is built for Linux only)."""
    out = ["JSFX"]
    if (GFX / f"{stem}_gfx.jsfx").exists():
        out.append("GFX")
    if (NATIVE / stem / "Plugin.cpp").exists():
        out += ["VST3", "CLAP", "LV2"]
    return out


def load():
    content = json.loads(CONTENT.read_text(encoding="utf-8"))
    plugins, errors = [], []
    demo_index = demos()
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
            "demo": demo_index.get(stem),
            "presets": [n for n, _ in json.loads((PRESETS / f"{stem}.json").read_text(encoding="utf-8"))]
            if (PRESETS / f"{stem}.json").exists() else [],
            "page": f"plugins/{stem.replace('_', '-')}/",
            "beta": bool(c.get("beta")),
            **{k: c[k] for k in ("card_desc", "keywords", "uses", "properties", "icon")},
        })
    for stem in sorted(set(content) - {p.stem for p in files}):
        errors.append(f"{stem}: in {CONTENT.name} but there is no {stem}.jsfx")
    if errors:
        sys.exit("build_site: " + "\n  ".join(["content does not match the plugins:"] + errors))
    for p in plugins:
        p["tags"] = tags(p)
    return sorted(plugins, key=lambda p: p["name"].lower())


# a plugin marked "beta" in site_content.json: a BETA badge on its card, guide and page, a note
# in its guide and page, "(beta)" in README.md, llms.txt and the Hub catalog
BETA_BADGE = '<span class="beta-badge">BETA</span>'
BETA_NOTE = ("Beta: its rules have been tested on test signals so far and may still be tuned from your feedback, "
             "so a project saved now can sound a little different after an update.")


DEMO_BUTTONS = ("""<button type="button" class="demo-play" aria-label="Listen: with and without the plugin" title="Listen">"""
                """<svg class="i-play" width="12" height="12" viewBox="0 0 24 24" aria-hidden="true"><polygon points="6 4 20 12 6 20" fill="currentColor"/></svg>"""
                """<svg class="i-stop" width="12" height="12" viewBox="0 0 24 24" aria-hidden="true"><rect x="6" y="6" width="12" height="12" fill="currentColor"/></svg></button>"""
                """<button type="button" class="demo-ab" title="Switch between the plugin and the dry sound">"""
                """<span class="on">ON</span><span class="off">BYPASS</span></button>""")


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
    demo = f"""
                    <div class="demo demo-card" data-demo="{p['stem']}">{DEMO_BUTTONS}</div>""" if p["demo"] else ""
    return f"""                <article class="plugin-card" data-name="{e(keywords)}" data-tags="{e('|'.join(p['tags']))}" onclick="openGuide('{p['key']}')">{demo}
                    <div class="plugin-icon">
                        {icon}
                    </div>
                    <div class="plugin-info">
                        <h3><a href="{p['page']}" class="card-link">{e(p['name'].upper())}</a>{' ' + BETA_BADGE if p['beta'] else ''}</h3>
                        <p class="plugin-desc">{e(p['card_desc'])}</p>{badges}
                        <span class="info-badge">?</span>
                    </div>
                </article>
"""


def cards_block(plugins, cats):
    out = []
    for title, slug, desc in (c for c in CATEGORIES if c[0] in cats):
        group = [p for p in plugins if p["category"] == title]
        if not group:
            continue  # a category whose plugins all wait for launch (R18) stays off the site too
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


TABS = [  # (tab id, label, categories in it); the FX Chains tab comes last
    ("effects", "Effects", ["One Slider", "Essentials", "Advanced", "Assisted"]),
    ("instruments", "Instruments", ["Instruments"]),
    ("midi", "MIDI", ["MIDI"]),
]


def tabs_block(plugins):
    """Effects, Instruments, MIDI and FX Chains tabs: the plugin categories and the chain cards."""
    chains = chain_cards(plugins)
    buttons, panels = [], []
    for i, (tab, label, cats) in enumerate(TABS):
        n = sum(p["category"] in cats for p in plugins)
        on = i == 0
        buttons.append(f"""                <button type="button" class="plugin-tab{' active' if on else ''}" role="tab" aria-selected="{'true' if on else 'false'}" aria-controls="tab-{tab}" id="tab-btn-{tab}" data-tab="{tab}">{label} <span class="category-count">{n}</span></button>
""")
        panels.append(f"""            <div class="tab-panel tab-plugins" id="tab-{tab}" role="tabpanel" aria-labelledby="tab-btn-{tab}"{'' if on else ' hidden'}>
{cards_block(plugins, cats)}            </div>
""")
    chips = "".join(f"""<button type="button" class="tag-chip" data-tag="{html.escape(label)}" aria-pressed="false">{html.escape(label)}</button>"""
                    for label, _ in TAGS)
    return f"""            <div class="tag-filter" role="group" aria-label="Filter by use">
                <button type="button" class="tag-chip active" data-tag="" aria-pressed="true">All</button>{chips}
            </div>
            <div class="plugin-tabs" role="tablist">
{"".join(buttons)}                <button type="button" class="plugin-tab" role="tab" aria-selected="false" aria-controls="tab-chains" id="tab-btn-chains" data-tab="chains">FX Chains <span class="category-count">{chains.count("<article")}</span></button>
            </div>
{"".join(panels)}            <div class="tab-panel" id="tab-chains" role="tabpanel" aria-labelledby="tab-btn-chains" hidden>
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
        if not any(p["category"] == title for p in plugins):
            continue  # nothing launched in it yet (R18)
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
    studio: true""" if p["studio"] else "") + (f""",
    demo: {js(p['stem'])}""" if p["demo"] else "") + (f""",
    beta: {js(BETA_NOTE)}""" if p["beta"] else "") + f""",
    page: {js(p['page'])}
  }},
""")
    out[-1] = out[-1].rstrip(",\n") + "\n"
    out.append("};\n")
    return "".join(out)


CATEGORY_TAB = {"One Slider": "Effects", "Essentials": "Effects", "Advanced": "Effects", "Assisted": "Effects",
                "Instruments": "Instruments", "MIDI": "MIDI"}
FORMAT_NAMES = {"JSFX": "JSFX for REAPER, with REAPER's own sliders", "GFX": "JSFX GFX for REAPER, with its own interface",
                "VST3": "VST3 for any DAW", "CLAP": "CLAP for any DAW",
                "LV2": "LV2 for Linux DAWs such as Ardour, Carla and Qtractor"}
KIND = {"Instruments": "instrument", "MIDI": "MIDI tool"}


def chains_with(stem):
    """Names of the FX chains and Rack presets a plugin is part of."""
    out = [c for c, fxs in CHAINS if any(s == stem for s, _ in fxs)]
    out += [c for c, _, slots, routes, globs in RACK_PRESETS if any(sl["id"] == stem for sl in slots)]
    return sorted(set(out), key=str.lower)


def page(p, plugins):
    """The plugin's own page: everything in its guide, its demo, presets, chains and neighbours."""
    e = html.escape
    kind = KIND.get(p["category"], "audio plugin")
    url = SITE + p["page"]
    title = f"{p['name']} by NULL JSFX: free {kind} for REAPER, VST3 and CLAP"
    desc = f"{p['name']}: {p['card_desc']}. Free and open source {kind} for REAPER (JSFX) and any DAW (VST3, CLAP) on Windows, macOS and Linux, plus LV2 on Linux."
    poster = p["video"].rsplit("/", 1)[-1].replace(".mp4", ".jpg") if p["video"] else None
    image = SITE + f"assets/video/{poster}" if poster else SITE + "assets/img/social-preview.jpg"
    ld = {
        "@context": "https://schema.org", "@type": "SoftwareApplication", "name": f"NULL {p['name']}",
        "description": p["card_desc"], "url": url, "image": image,
        "applicationCategory": "MultimediaApplication", "applicationSubCategory": f"Audio {kind}",
        "operatingSystem": "Windows, macOS, Linux", "softwareVersion": VERSION, "license": "https://opensource.org/licenses/MIT",
        "offers": {"@type": "Offer", "price": "0", "priceCurrency": "EUR"},
        "author": {"@type": "Person", "name": "Christos Koulaxizis"},
        "isPartOf": {"@type": "SoftwareApplication", "name": "NULL JSFX", "url": SITE},
    }
    crumbs = {"@context": "https://schema.org", "@type": "BreadcrumbList", "itemListElement": [
        {"@type": "ListItem", "position": 1, "name": "NULL JSFX", "item": SITE},
        {"@type": "ListItem", "position": 2, "name": p["category"], "item": SITE + "#" + CATEGORY_TAB[p["category"]].lower()},
        {"@type": "ListItem", "position": 3, "name": p["name"], "item": url}]}
    tab_hash = {"Effects": "", "Instruments": "#instruments", "MIDI": "#midi"}[CATEGORY_TAB[p["category"]]]
    media = (f'<video class="guide-shot guide-video" poster="/{p["video"][:-4]}.jpg" controls playsinline preload="none" width="1280" height="720" aria-label="{e(p["name"])} presentation video">'
             f'<source src="{MEDIA_VIDEOS}{p["stem"]}.mp4" type="video/mp4"><source src="/{p["video"]}" type="video/mp4"></video>'
             if p["video"] else f'<img class="guide-shot" src="/{p["shot"]}" alt="{e(p["name"])} interface" width="720" height="480">')
    sliders = "".join(f'<li><span class="slider-name">{e(n)}</span><span class="slider-desc">{e(r)} — {e(d)}</span></li>'
                      for n, r, d in p["sliders"])
    uses = "".join(f"<li>{e(u)}</li>" for u in p["uses"])
    formats = "".join(f'<li><span class="format-tag">{f}</span>{e(FORMAT_NAMES[f])}</li>' for f in p["formats"])
    demo = f"""
                <section class="guide-section">
                    <h2>Listen</h2>
                    <div class="demo demo-big" data-demo="{p['stem']}">{DEMO_BUTTONS}<span class="demo-note">The same music with the <b>{e(p['demo'])}</b> preset and without the plugin. Switch while it plays.</span></div>
                </section>""" if p["demo"] else ""
    presets = f"""
                <section class="guide-section">
                    <h2>Factory presets</h2>
                    <p class="page-list">{" · ".join(e(x) for x in p["presets"] if x != "Default")}</p>
                </section>""" if [x for x in p["presets"] if x != "Default"] else ""
    beta = f"""
                <p class="beta-note">{e(BETA_NOTE)}</p>""" if p["beta"] else ""
    chains = chains_with(p["stem"])
    chains_html = f"""
                <section class="guide-section">
                    <h2>In the FX chains and Rack presets</h2>
                    <p class="page-list">{" · ".join(e(c) for c in chains)}</p>
                    <p class="page-more"><a href="/#fx-chains">See every FX chain</a></p>
                </section>""" if chains else ""
    studio = """
                <section class="guide-section">
                    <h2>Try it</h2>
                    <a class="try-studio guide-studio" href="https://useoros.online/studio/" target="_blank" rel="noopener"><svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><circle cx="12" cy="12" r="10"/><polygon points="10 8 16 12 10 16 10 8" fill="currentColor"/></svg><span class="try-studio-text"><b>Try it on orOS Studio</b><small>Add it to a track in your browser, with its own interface. Nothing to install.</small></span></a>
                </section>""" if p["studio"] else ""
    shared = set(p["tags"])
    near = sorted((q for q in plugins if q is not p),
                  key=lambda q: (-(q["category"] == p["category"]) - len(shared & set(q["tags"])), q["name"].lower()))[:6]
    related = "".join(f'<a class="related-card" href="/{q["page"]}"><b>{e(q["name"])}</b><small>{e(q["card_desc"])}</small></a>' for q in near)
    return f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{e(title)}</title>
    <meta name="description" content="{e(desc)}">
    <link rel="canonical" href="{url}">
    <meta property="og:type" content="website">
    <meta property="og:title" content="{e(p['name'])}: {e(p['card_desc'])}">
    <meta property="og:description" content="{e(desc)}">
    <meta property="og:url" content="{url}">
    <meta property="og:image" content="{image}">
    <meta name="twitter:card" content="summary_large_image">
    <meta name="twitter:title" content="{e(p['name'])}: {e(p['card_desc'])}">
    <meta name="twitter:description" content="{e(desc)}">
    <meta name="twitter:image" content="{image}">
    <link rel="icon" type="image/svg+xml" href="/assets/img/favicon.svg">
    <link rel="stylesheet" href="/assets/css/style.css">
    <script type="application/ld+json">{json.dumps(ld)}</script>
    <script type="application/ld+json">{json.dumps(crumbs)}</script>
</head>
<body class="plugin-page">
    <main>
        <nav class="page-crumbs" aria-label="Breadcrumb">
            <a href="/" class="page-home"><img src="/assets/img/favicon.svg" alt="NULL JSFX" width="28" height="28"></a>
            <a href="/">NULL JSFX</a> <span>/</span> <a href="/{tab_hash}">{CATEGORY_TAB[p['category']]}</a> <span>/</span> <span>{e(p['name'])}</span>
        </nav>
        <article class="page-plugin">
            <header class="guide-header page-header">
                <span class="guide-category">{e(p['category'])}</span>
                <h1>{e(p['name'].upper())}{' ' + BETA_BADGE if p['beta'] else ''}</h1>
                <p>{e(p['card_desc'])}</p>{beta}
            </header>
            {media}
            <div class="page-actions">
                <a class="featured-btn" href="/#download"><svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/></svg><span>Download free</span></a>
                <small>Part of NULL JSFX {VERSION}: {len(plugins)} plugins, free and open source (MIT). Easiest with <a href="/#hub">NULL JSFX Hub</a>.</small>
            </div>
            <div class="guide-body">{demo}
                <section class="guide-section">
                    <h2>Controls</h2>
                    <ul class="guide-sliders">{sliders}</ul>
                </section>
                <section class="guide-section">
                    <h2>Best uses</h2>
                    <ul class="use-cases">{uses}</ul>
                </section>
                <section class="guide-section">
                    <h2>Properties</h2>
                    <p class="page-props">{e(p['properties'])}</p>
                </section>
                <section class="guide-section">
                    <h2>Formats</h2>
                    <ul class="guide-formats">{formats}</ul>
                </section>{presets}{chains_html}{studio}
            </div>
        </article>
        <section class="page-related">
            <h2 class="section-title">Goes well with</h2>
            <div class="related-grid">{related}</div>
            <p class="page-more"><a href="/{tab_hash}">All {len(plugins)} NULL JSFX plugins</a></p>
        </section>
    </main>
    <footer class="site-footer">
        <div class="footer-content">
            Designed with &#9825; by <a href="https://koulaxizis.gr" target="_blank" rel="noopener">Christos Koulaxizis</a>.
            <div class="footer-links">
                <a href="https://ko-fi.com/koulaxizis" target="_blank" rel="noopener">Support on Ko-fi</a>
                <span class="sep">&bull;</span>
                <a href="https://github.com/koulaxizis/null-jsfx" target="_blank" rel="noopener">GitHub</a>
                <span class="sep">&bull;</span>
                <a href="/">nulljsfx.tech</a>
            </div>
        </div>
    </footer>
    <button id="theme-toggle" class="theme-toggle-floating" aria-label="Toggle color theme" title="Switch between light/dark theme">
        <svg class="sun-icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="5"/><path d="M12 1v2 M12 21v2 M4.22 4.22l1.42 1.42 M18.36 18.36l1.42 1.42 M1 12h2 M21 12h2 M4.22 19.78l1.42-1.42 M18.36 5.64l1.42-1.42"/></svg>
        <svg class="moon-icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="display:none;"><path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z"/></svg>
    </button>
    <script src="/assets/js/theme.js"></script>
    <script src="/assets/js/demo.js"></script>
</body>
</html>
"""


def sitemap(plugins):
    d, m, y = re.search(r'id="last-update">Released: (\d+)/(\d+)/(\d+)<', INDEX.read_text(encoding="utf-8")).groups()
    today = f"20{y}-{m}-{d}"  # the release date: changes with each release, not each run
    urls = [(SITE, "1.0", "weekly")] + [(SITE + p["page"], "0.8", "monthly") for p in plugins] + \
        [(SITE + "supporters/", "0.5", "monthly")]
    body = "".join(f"""    <url>
        <loc>{u}</loc>
        <lastmod>{today}</lastmod>
        <changefreq>{f}</changefreq>
        <priority>{pr}</priority>
    </url>
""" for u, pr, f in urls)
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">
{body}</urlset>
"""


def list_block(plugins, with_desc):
    out = []
    for title, _, desc in CATEGORIES:
        group = [p for p in plugins if p["category"] == title]
        if not group:
            continue  # nothing launched in it yet (R18)
        if with_desc:
            out.append(f"\n### {title} ({len(group)})\n{desc}\n\n")
            out += [f"- [{p['name']}]({SITE}{p['page']}){' (beta)' if p['beta'] else ''}: {p['card_desc']}"
                    + (f" (also as {', '.join(p['formats'][1:])})" if len(p['formats']) > 1 else "") + "\n"
                    for p in group]
        else:
            out.append(f"| **{title}** ({len(group)}) | {', '.join(p['name'] + (' (beta)' if p['beta'] else '') for p in group)} |\n")
    return ("| Category | Plugins |\n|----------|---------|\n" if not with_desc else "") + "".join(out)


def replace_between(text, begin, end, new):
    i, j = text.index(begin) + len(begin), text.index(end)
    return text[:i] + "\n" + new + text[j:]


def main(check):
    plugins = load()
    counts = {t: sum(p["category"] == t for p in plugins) for t, _, _ in CATEGORIES}
    n_chains = chain_cards(plugins).count("<article")
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
        # the counts in the hand-written text around the generated blocks
        new = re.sub(r"\b\d+ (professional audio plugins|free audio plugins)", f"{len(plugins)} \\1", new)
        new = re.sub(r"\b(All |suite of )\d+( (?:audio )?plugins)", f"\\g<1>{len(plugins)}\\2", new)
        new = re.sub(r"\b\d+( ready-made FX chains| REAPER FX chains)", f"{n_chains}\\1", new)
        files.append((path, old, new))
    # whole files: the plugin pages and the sitemap
    for path, new in [(PAGES / p["page"].split("/")[1] / "index.html", page(p, plugins)) for p in plugins] + \
            [(SITEMAP, sitemap(plugins))]:
        files.append((path, path.read_text(encoding="utf-8") if path.exists() else None, new))
    gone = [d for d in PAGES.glob("*") if d.is_dir() and d.name not in {p["page"].split("/")[1] for p in plugins}]
    stale = [str(f.relative_to(ROOT)) for f, old, new in files if old != new] + [str(d.relative_to(ROOT)) for d in gone]
    if check:
        if stale:
            sys.exit(f"build_site: {', '.join(stale)} out of date; run tools/build_site.py")
        print(f"build_site: up to date for all {len(plugins)} plugins {counts}")
        return
    for path, old, new in files:
        if old != new:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(new, encoding="utf-8")
    for d in gone:
        __import__("shutil").rmtree(d)
    shown = stale if len(stale) < 12 else stale[:10] + [f"and {len(stale) - 10} more"]
    print(f"build_site: {len(plugins)} plugins {counts}; updated {', '.join(shown) or 'nothing'}")


if __name__ == "__main__":
    main("--check" in sys.argv[1:])
