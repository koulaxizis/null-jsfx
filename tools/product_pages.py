"""The product pages of the site: rack/index.html (NULL JSFX Rack), hub/index.html (NULL JSFX
Hub) and remotes/index.html (the three remote apps), plus the homepage section "One plugin, every
form" (forms_block()). Used by tools/build_site.py, so their counts (modules, presets, plugins, chains) come from
the repo and cannot drift. Their pictures and short loops are made by tools/site_visuals/
(rack_shots.py, hub_shots.cjs, then publish.py into assets/img/{rack,hub}/ and assets/video/site/).

The layout follows the site's product style: a hero with the interface, a row of numbers, the
presentation video, then alternating rows of text and a picture or a silent loop, a diagram, the
lists and how to get it. A row whose picture or loop is missing is left out (R14: nothing by hand).
"""
import html
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RACK_MODULES = ROOT / "native/plugins/rack/modules"
RACK_PRESETS_H = ROOT / "native/plugins/rack/RackPresets.hpp"
IMG = ROOT / "assets/img"
CLIPS = ROOT / "assets/video/site"
HARDWARE = ROOT / "assets/hardware"  # the 3D renders (the 3D thread), rack.webp and hub.webp when there
MEDIA_VIDEOS = "https://github.com/koulaxizis/null-jsfx/releases/download/media/"

DOWNLOAD_ICON = ('<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true">'
                 '<path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/></svg>')
PLAY_ICON = ('<svg width="16" height="16" viewBox="0 0 24 24" aria-hidden="true"><polygon points="7 4 20 12 7 20" fill="currentColor"/></svg>')


def rack_presets():
    """[(name, group)] of the Rack's factory presets, Init left out (RackPresets.hpp)."""
    text = RACK_PRESETS_H.read_text(encoding="utf-8")
    block = text[text.index("static const Preset kPresets[]"):]
    return [(n, g) for n, g in re.findall(r'^    \{ "([^"]+)", "([^"]+)"', block, re.M) if g != "Init"]


def rack_modules():
    return sorted(p.stem for p in RACK_MODULES.glob("*.hpp"))


def picture(folder, name, alt, w, h, cls="pp-shot", eager=False):
    """A WebP picture with its 1x and 2x files (publish.py), or "" when it is not there."""
    big = IMG / folder / f"{name}.webp"
    small = IMG / folder / f"{name}-1x.webp"
    if not big.exists():
        return ""
    srcset = f' srcset="/assets/img/{folder}/{name}-1x.webp 1x, /assets/img/{folder}/{name}.webp 2x"' if small.exists() else ""
    load = 'fetchpriority="high"' if eager else 'loading="lazy"'
    return (f'<img class="{cls}" src="/assets/img/{folder}/{name}{"-1x" if small.exists() else ""}.webp"{srcset} '
            f'width="{w}" height="{h}" alt="{html.escape(alt)}" {load} decoding="async">')


def clip(name, label, w, h):
    """A short silent loop that plays while it is on screen (assets/js/clips.js), or ""."""
    if not (CLIPS / f"{name}.mp4").exists():
        return ""
    poster = f' poster="/assets/video/site/{name}.webp"' if (CLIPS / f"{name}.webp").exists() else ""
    return (f'<video class="pp-shot pp-clip" muted loop playsinline preload="none"{poster} width="{w}" height="{h}" '
            f'aria-label="{html.escape(label)}" data-clip><source src="/assets/video/site/{name}.mp4" type="video/mp4"></video>')


def hero_media(kind, folder, name, alt, w, h):
    """The 3D unit when the 3D thread has made it: its orbit (from the "media" release as <kind>_orbit.mp4,
    played once by assets/js/orbit.js, then it settles on the still) with the still as poster, or the still alone when there is
    no orbit; otherwise the interface in a window. Never the still and the orbit together (Christos, 2026-11-21)."""
    render = HARDWARE / f"{kind}.webp"
    if render.exists() and (HARDWARE / f"{kind}.mp4").exists():
        return (f'<figure class="pp-render"><div class="hw-stage" style="background-image:url(/assets/hardware/{kind}.webp)">'
                f'<video class="hw-orbit" muted playsinline preload="none" '
                f'poster="/assets/hardware/{kind}.webp" width="1280" height="720" aria-label="{html.escape(alt)}, 3D render, turning">'
                f'<source src="{MEDIA_VIDEOS}{kind}_orbit.mp4" type="video/mp4"></video></div>'
                f'<figcaption>3D render · software</figcaption></figure>')
    if render.exists():
        return (f'<figure class="pp-render"><img src="/assets/hardware/{kind}.webp" width="1600" height="900" '
                f'alt="{html.escape(alt)}, 3D render" fetchpriority="high" decoding="async">'
                f'<figcaption>3D render · software</figcaption></figure>')
    return f'<div class="pp-window">{picture(folder, name, alt, w, h, eager=True)}</div>'


def rows(items):
    """Alternating feature rows: [(kicker, title, text, [points], media html)]; empty media drops a row."""
    out = []
    for k, (kicker, title, text, points, media) in enumerate(i for i in items if i[4]):
        lis = "".join(f"<li>{p}</li>" for p in points)
        out.append(f"""
            <section class="pp-row{' pp-flip' if k % 2 else ''}">
                <div class="pp-text">
                    <span class="pp-kicker">{kicker}</span>
                    <h2>{title}</h2>
                    <p>{text}</p>{f'<ul class="pp-points">{lis}</ul>' if lis else ''}
                </div>
                <div class="pp-media">{media}</div>
            </section>""")
    return "".join(out)


def stats(items):
    return '<div class="pp-stats">' + "".join(f'<div><b>{v}</b><span>{k}</span></div>' for v, k in items) + "</div>"


def video(stem, label):
    # the 960 px WebP copy of the poster (tools/site_images.py) when there is one: a phone loads a third of the bytes
    poster = f"/assets/video/{stem}-960.webp" if (ROOT / f"assets/video/{stem}-960.webp").exists() else f"/assets/video/{stem}.jpg"
    return (f'<video class="pp-video" poster="{poster}" controls playsinline preload="none" width="1280" height="720" '
            f'aria-label="{html.escape(label)}"><source src="{MEDIA_VIDEOS}{stem}.mp4" type="video/mp4">'
            f'<source src="/assets/video/{stem}.mp4" type="video/mp4"></video>')


RACK_DIAGRAM = """<svg class="pp-diagram" viewBox="0 0 980 360" width="980" height="360" role="img" aria-label="How the sound goes through the Rack">
<defs><marker id="pp-arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto"><path d="M0 0L10 5L0 10z" class="pp-d-head"/></marker></defs>
<path class="pp-d-wire" d="M58 190H112" marker-end="url(#pp-arrow)"/>
<rect class="pp-d-node" x="112" y="160" width="86" height="60" rx="8"/><text class="pp-d-t" x="155" y="186">INPUT</text><text class="pp-d-s" x="155" y="204">gain · split</text>
<path class="pp-d-wire" d="M198 190H236V70H262M236 150H262M236 230H262M236 310H262" />
<g>
<text class="pp-d-lane" x="270" y="52" text-anchor="start">LANE A</text><text class="pp-d-lane" x="270" y="132" text-anchor="start">LANE B</text><text class="pp-d-lane" x="270" y="212" text-anchor="start">LANE C</text><text class="pp-d-lane" x="270" y="292" text-anchor="start">LANE D</text>
<rect class="pp-d-slot" x="262" y="56" width="70" height="28" rx="5"/><rect class="pp-d-slot" x="340" y="56" width="70" height="28" rx="5"/><rect class="pp-d-slot" x="418" y="56" width="70" height="28" rx="5"/><rect class="pp-d-slot" x="496" y="56" width="70" height="28" rx="5"/>
<rect class="pp-d-slot" x="262" y="136" width="70" height="28" rx="5"/><rect class="pp-d-slot" x="340" y="136" width="70" height="28" rx="5"/>
<rect class="pp-d-slot" x="262" y="216" width="70" height="28" rx="5"/>
<rect class="pp-d-slot" x="262" y="296" width="70" height="28" rx="5"/>
<path class="pp-d-wire" d="M332 70H340M410 70H418M488 70H496M332 150H340M566 70H604V190M410 150H604M332 230H604V190M332 310H604V190" />
</g>
<rect class="pp-d-node" x="604" y="160" width="96" height="60" rx="8"/><text class="pp-d-t" x="652" y="186">LANE MIX</text><text class="pp-d-s" x="652" y="204">gain · pan · solo</text>
<path class="pp-d-wire" d="M700 190H754" marker-end="url(#pp-arrow)"/>
<rect class="pp-d-node" x="754" y="160" width="96" height="60" rx="8"/><text class="pp-d-t" x="802" y="186">OUTPUT</text><text class="pp-d-s" x="802" y="204">gain · mix</text>
<path class="pp-d-wire" d="M850 190H918" marker-end="url(#pp-arrow)"/>
<path class="pp-d-loop" d="M802 160V18H155V160" marker-end="url(#pp-arrow)"/><text class="pp-d-s" x="480" y="12">feedback loop, from slot to slot</text>
<circle class="pp-d-term" cx="40" cy="190" r="18"/><text class="pp-d-tt" x="40" y="194">IN</text>
<circle class="pp-d-term" cx="936" cy="190" r="18"/><text class="pp-d-tt" x="936" y="194">OUT</text>
<g class="pp-d-mods"><rect class="pp-d-src" x="290" y="336" width="400" height="22" rx="11"/><text class="pp-d-srct" x="490" y="352">MACROS · LFOS · ENVELOPE · SEQUENCER · MIDI → ANY VALUE</text></g>
</svg>"""

HUB_DIAGRAM = """<svg class="pp-diagram" viewBox="0 0 980 260" width="980" height="260" role="img" aria-label="How a new release reaches your computer through the Hub">
<defs><marker id="pp-arrow2" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto"><path d="M0 0L10 5L0 10z" class="pp-d-head"/></marker></defs>
<rect class="pp-d-node" x="20" y="96" width="170" height="68" rx="8"/><text class="pp-d-t" x="105" y="124">GITHUB RELEASE</text><text class="pp-d-s" x="105" y="144">packages + catalog</text>
<path class="pp-d-wire" d="M190 130H262" marker-end="url(#pp-arrow2)"/>
<text class="pp-d-s" x="226" y="118">once a day</text>
<rect class="pp-d-node" x="262" y="96" width="170" height="68" rx="8"/><text class="pp-d-t" x="347" y="124">NULL JSFX HUB</text><text class="pp-d-s" x="347" y="144">only what changed</text>
<path class="pp-d-wire" d="M432 130H504" marker-end="url(#pp-arrow2)"/>
<text class="pp-d-s" x="468" y="118">sha256</text>
<rect class="pp-d-node" x="504" y="96" width="170" height="68" rx="8"/><text class="pp-d-t" x="589" y="124">CHECKED</text><text class="pp-d-s" x="589" y="144">every file verified</text>
<path class="pp-d-wire" d="M674 130H712V40H748M712 100H748M712 160H748M712 220H748"/>
<rect class="pp-d-slot" x="748" y="24" width="212" height="32" rx="6"/><text class="pp-d-st" x="854" y="45">REAPER Effects · JSFX, GFX</text>
<rect class="pp-d-slot" x="748" y="84" width="212" height="32" rx="6"/><text class="pp-d-st" x="854" y="105">VST3 folder</text>
<rect class="pp-d-slot" x="748" y="144" width="212" height="32" rx="6"/><text class="pp-d-st" x="854" y="165">CLAP folder</text>
<rect class="pp-d-slot" x="748" y="204" width="212" height="32" rx="6"/><text class="pp-d-st" x="854" y="225">LV2 (Linux) · AU (macOS)</text>
</svg>"""


def shell(title, desc, url, image, body, crumbs_name, ld):
    e = html.escape
    from chain_pages import FOOTER  # the same footer as the plugin and chain pages
    crumbs = {"@context": "https://schema.org", "@type": "BreadcrumbList", "itemListElement": [
        {"@type": "ListItem", "position": 1, "name": "NULL JSFX", "item": "https://nulljsfx.tech/"},
        {"@type": "ListItem", "position": 2, "name": crumbs_name, "item": url}]}
    return f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{e(title)}</title>
    <meta name="description" content="{e(desc)}">
    <link rel="canonical" href="{url}">
    <meta property="og:type" content="website">
    <meta property="og:title" content="{e(title)}">
    <meta property="og:description" content="{e(desc)}">
    <meta property="og:url" content="{url}">
    <meta property="og:image" content="{image}">
    <meta name="twitter:card" content="summary_large_image">
    <meta name="twitter:title" content="{e(title)}">
    <meta name="twitter:description" content="{e(desc)}">
    <meta name="twitter:image" content="{image}">
    <link rel="icon" type="image/svg+xml" href="/assets/img/favicon.svg">
    <link rel="stylesheet" href="/assets/css/style.css">
    <script type="application/ld+json">{json.dumps(ld)}</script>
    <script type="application/ld+json">{json.dumps(crumbs)}</script>
</head>
<body class="plugin-page product-page">
    <main>
        <nav class="page-crumbs" aria-label="Breadcrumb">
            <a href="/" class="page-home"><img src="/assets/img/favicon.svg" alt="NULL JSFX" width="28" height="28"></a>
            <a href="/">NULL JSFX</a> <span>/</span> <span>{e(crumbs_name)}</span>
        </nav>
        <article class="page-product">{body}
        </article>
    </main>
{FOOTER}
    <script src="/assets/js/theme.js"></script>
    <script src="/assets/js/clips.js"></script>
    <script src="/assets/js/orbit.js"></script>
</body>
</html>
"""


def software_ld(name, desc, url, image, version, extra=None):
    ld = {"@context": "https://schema.org", "@type": "SoftwareApplication", "name": name, "description": desc, "url": url,
          "image": image, "applicationCategory": "MultimediaApplication", "operatingSystem": "Windows, macOS, Linux",
          "softwareVersion": version, "license": "https://opensource.org/licenses/MIT", "isAccessibleForFree": True,
          "offers": {"@type": "Offer", "price": "0", "priceCurrency": "EUR"},
          "author": {"@type": "Person", "name": "Christos Koulaxizis"},
          "downloadUrl": "https://github.com/koulaxizis/null-jsfx/releases/latest"}
    ld.update(extra or {})
    return ld


def og_image(kind, fallback):
    return f"https://nulljsfx.tech/assets/hardware/{kind}.webp" if (HARDWARE / f"{kind}.webp").exists() else fallback


def rack_page(plugins, chains, version):
    e = html.escape
    on_site = {p["stem"]: p for p in plugins}
    modules = rack_modules()
    presets = rack_presets()
    groups = {}
    for n, g in presets:
        groups.setdefault(g, []).append(n)
    chain_slugs = {c["name"]: c["slug"] for c in chains}
    url = "https://nulljsfx.tech/rack/"
    desc = (f"NULL JSFX Rack: a free VST3 and CLAP plugin that holds up to eight of {len(modules)} NULL modules, "
            "with lanes, bands, Mid/Side, macros, LFOs and a sequencer.")
    image = og_image("rack", "https://nulljsfx.tech/assets/img/rack/mastering.webp")
    W, H = 960, 680
    hero = f"""
            <header class="pp-hero">
                <span class="pp-kicker">VST3 &middot; CLAP &middot; LV2 &middot; any DAW &middot; free</span>
                <h1>NULL JSFX RACK</h1>
                <p class="pp-lead">Build your own effect. Up to eight NULL modules in one plugin, each on the full sound, a band, mid, side, left or right, with modulation that moves them.</p>
                <div class="pp-actions">
                    <a class="featured-btn" href="/#download">{DOWNLOAD_ICON}<span>Get the Rack</span></a>
                    <a class="pp-ghost" href="#tour">{PLAY_ICON}<span>Watch the tour</span></a>
                </div>
                {hero_media("rack", "rack", "mastering", "NULL JSFX Rack with the Mastering Chain preset", W, H)}
            </header>
            {stats([(len(modules), "modules"), (len(presets), "presets"), (8, "slots"), (4, "lanes"), (5, "bands"), (32, "mod routes")])}
            <section class="pp-tour" id="tour">
                <h2 class="pp-h">The tour</h2>
                {video("rack", "NULL JSFX Rack presentation video")}
            </section>"""
    feats = rows([
        ("Chain", "Eight slots, any order",
         f"Pick any of {len(modules)} modules for each slot: the same DSP as the NULL JSFX plugins, so a module sounds exactly like its plugin. Drag a slot to reorder, bypass it or set its own mix.",
         ["Every module with its own interface", "A/B, undo and presets per module", f"{len(presets)} factory presets in three groups"],
         picture("rack", "mastering", "The Rack's chain with the Mastering Chain preset", W, H)),
        ("Routing", "Bands, Mid/Side, left and right",
         "Put a slot on the lows, the mids or the highs at crossovers you choose, split into up to five bands, or work on mid, side, left or right alone.",
         ["Low-Mid and Mid-High crossovers on the front panel", "Split into 2 to 5 bands, with solo and bypass per band"],
         picture("rack", "bands", "The Rack with the Multiband Distortion preset", W, H)),
        ("Modulation", "Macros that move many controls",
         "Eight macros, each with as many routes as you need. Turn one knob and a build-up rises, a filter opens and the pump drops in, all at once.",
         ["Up to 32 routes, each with its depth and curve", "Scenes: up to 4 snapshots that Scene Morph moves through"],
         clip("rack-macro", "Macro 1 turns three modules of the Build-Up preset", 1280, 906)),
        ("Movement", "LFOs and an envelope follower",
         "Two LFOs with tempo sync and five shapes, an envelope follower that listens to the input, Smooth Random, a pitch follower and an ADSR.",
         ["Sine, triangle, square, saw and random", "Sources from MIDI: note, velocity, gate, mod wheel, pressure, bend, CC"],
         clip("rack-lfo", "An LFO sweeps the filter of the Auto Filter preset", 1280, 906)),
        ("Rhythm", "A 16-step sequencer",
         "Steps with glide, from 1/32 to whole notes, locked to the host's tempo: rhythmic gates, filter patterns and stutters.",
         [], clip("rack-sequencer", "The step sequencer of the Rhythm Engine preset", 1280, 906)),
        ("Lanes", "Parallel lanes and a feedback loop",
         "Run up to four lanes side by side with their own gain, pan, mute and solo, or feed the output of one slot back into an earlier one.",
         ["Lane Mix between lanes A and B", "Feedback from any slot to any earlier slot"],
         picture("rack", "lanes", "The Rack with the Parallel Drum Smash preset", W, H)),
    ])
    diagram = f"""
            <section class="pp-block">
                <h2 class="pp-h">How the sound goes through it</h2>
                <div class="pp-diagram-wrap">{RACK_DIAGRAM}</div>
            </section>"""
    preset_html = []
    for g, title in (("Chain", "Chains"), ("Routing", "Routing"), ("Macro", "Macros")):
        names = groups.get(g, [])
        items = " ".join(f'<a href="/chains/{chain_slugs[n]}/">{e(n)}</a>' if n in chain_slugs else f"<span>{e(n)}</span>" for n in names)
        preset_html.append(f'<div class="pp-group"><h3>{title} <small>{len(names)}</small></h3><p class="pp-tags">{items}</p></div>')
    module_items = " ".join(f'<a href="/{on_site[m]["page"]}">{e(on_site[m]["name"])}</a>' if m in on_site else f"<span>{e(m)}</span>"
                            for m in sorted(modules, key=lambda m: (on_site[m]["name"] if m in on_site else m).lower()))
    lists = f"""
            <section class="pp-block">
                <h2 class="pp-h">{len(presets)} presets</h2>
                <p class="pp-sub">Presets that also exist as a REAPER FX chain open their chain page.</p>
                <div class="pp-groups">{"".join(preset_html)}</div>
            </section>
            <section class="pp-block">
                <h2 class="pp-h">{len(modules)} modules</h2>
                <p class="pp-sub">Every NULL JSFX effect that fits a slot; meters, instruments, MIDI effects and plugins with more than 25 controls stay plugins of their own.</p>
                <p class="pp-tags">{module_items}</p>
            </section>"""
    get = f"""
            <section class="pp-get">
                <h2>Free, like every NULL plugin</h2>
                <p>The Rack comes with the VST3 and CLAP download and with the Hub. No account, no licence key.</p>
                <div class="pp-actions">
                    <a class="featured-btn" href="/#download">{DOWNLOAD_ICON}<span>Download free</span></a>
                    <a class="pp-ghost" href="/hub/"><span>Get it with the Hub</span></a>
                </div>
                <small>NULL JSFX {version}: {len(plugins)} plugins, {len(chains)} FX chains, MIT licence.</small>
            </section>"""
    body = hero + feats + diagram + lists + get
    return shell("NULL JSFX Rack: free modular effect, VST3 and CLAP", desc, url, image, body, "Rack",
                 software_ld("NULL JSFX Rack", desc, url, image, version, {"applicationSubCategory": "Audio effect host (VST3, CLAP)"}))


def hub_page(plugins, chains, version):
    url = "https://nulljsfx.tech/hub/"
    desc = (f"NULL JSFX Hub: a free app that installs, updates and removes all {len(plugins)} NULL JSFX plugins "
            "as JSFX, GFX, VST3, CLAP and LV2. No account, no tracking.")
    image = og_image("hub", "https://nulljsfx.tech/assets/img/hub/plugins.webp")
    W, H = 1200, 750
    hero = f"""
            <header class="pp-hero">
                <span class="pp-kicker">Free app &middot; Windows, Linux, macOS</span>
                <h1>NULL JSFX HUB</h1>
                <p class="pp-lead">Every plugin in one window. Install, update and remove them in any format, hear a demo of each one, and get told when a new version is out.</p>
                <div class="pp-actions">
                    <a class="featured-btn" href="/#download">{DOWNLOAD_ICON}<span>Get the Hub</span></a>
                    <a class="pp-ghost" href="#tour">{PLAY_ICON}<span>Watch the tour</span></a>
                </div>
                {hero_media("hub", "hub", "plugins", "NULL JSFX Hub with every plugin of the suite", W, H)}
            </header>
            {stats([(len(plugins), "plugins"), (5, "formats"), (len(chains), "FX chains"), (6, "starter profiles"), (0, "accounts")])}
            <section class="pp-tour" id="tour">
                <h2 class="pp-h">The tour</h2>
                {video("hub", "NULL JSFX Hub presentation video")}
            </section>"""
    feats = rows([
        ("Library", "Every plugin, one place",
         f"All {len(plugins)} plugins with their interface, the formats each one comes in and what is already on your computer.",
         ["JSFX and GFX for REAPER, VST3, CLAP, LV2 on Linux, AU on macOS", "One by one, a whole group, or everything"],
         picture("hub", "plugins", "The Hub's list of plugins", W, H)),
        ("Search", "Find what you need",
         "Search by name or by what you need: eq, reverb, vocal, drums. The list narrows as you type.",
         [], clip("hub-search", "Searching the Hub for reverb", 1280, 800)),
        ("Install", "One click",
         "Install puts a plugin in every format picked in the bar, in the folders your DAW reads. Only the files it needs are downloaded.",
         ["Checks every download against the release", "Check and repair, roll back, the same set on another computer"],
         clip("hub-install", "Installing Delay from the Hub", 1280, 800)),
        ("Plugin page", "Video, demo and guide",
         "Each plugin has its page: the presentation video, a before-and-after listen, its controls and factory presets, and each format on its own.",
         [], picture("hub", "detail", "The Hub's page for Delay", W, H)),
        ("Profiles", "Starter profiles",
         "Podcast, Beatmaking, Mixing, Mastering, Guitar and Sound Design: ready-made sets of plugins and extras, installed with one button.",
         [], clip("hub-profile", "A starter profile in the Hub", 1280, 800)),
        ("Extras", "The Rack, chains and more",
         f"Turn on the Rack, the {len(chains)} FX chains, the factory presets, the tablet remote and the font, each on its own.",
         [], picture("hub", "extras", "The Hub's extras", W, H)),
        ("Privacy", "No account, no tracking",
         "It only reads the release list from GitHub. Updates are checked once a day, with one notice per release, and it can start in the tray.",
         ["English and Greek", "Uninstall everything in one step"],
         picture("hub", "settings", "The Hub's settings", W, H)),
    ])
    diagram = f"""
            <section class="pp-block">
                <h2 class="pp-h">How an update reaches you</h2>
                <div class="pp-diagram-wrap">{HUB_DIAGRAM}</div>
            </section>"""
    steps = """
            <section class="pp-block">
                <h2 class="pp-h">Three steps</h2>
                <ol class="pp-steps">
                    <li><b>Download</b><span>The Hub for Windows (setup), Linux (AppImage or .deb) or macOS (.dmg).</span></li>
                    <li><b>Pick</b><span>Choose the formats once, then the plugins, a group or a starter profile.</span></li>
                    <li><b>Play</b><span>Open your DAW: the plugins are there. The Hub tells you when there is a new version.</span></li>
                </ol>
            </section>"""
    get = f"""
            <section class="pp-get">
                <h2>Free, open source, MIT</h2>
                <p>The easiest way to get NULL JSFX and keep it up to date.</p>
                <div class="pp-actions">
                    <a class="featured-btn" href="/#download">{DOWNLOAD_ICON}<span>Download the Hub</span></a>
                    <a class="pp-ghost" href="/rack/"><span>Meet the Rack</span></a>
                </div>
                <small>NULL JSFX {version}: {len(plugins)} plugins, {len(chains)} FX chains.</small>
            </section>"""
    body = hero + feats + diagram + steps + get
    return shell("NULL JSFX Hub: install and update every plugin, free", desc, url, image, body, "Hub",
                 software_ld("NULL JSFX Hub", desc, url, image, version, {"applicationSubCategory": "Plugin manager"}))


FDROID = ("fdroidrepos://apps.koulaxizis.workers.dev/null-jsfx-fdroid/repo?fingerprint="
          "1C071614D51503C5ED3260A1B69A0D44CEA6057A5DC6A3ACC3FA7AD130EF64EB")
APK = "https://github.com/koulaxizis/null-jsfx/releases/latest/download/NULL-JSFX-Remote-{}-Android.apk"


def device(kind, name, alt, w, h):
    """A screen of a remote app in a drawn tablet or phone frame, or "" when the screen is missing."""
    shot = picture("remotes", name, alt, w, h)
    return f'<div class="pp-device pp-{kind}">{shot}</div>' if shot else ""


def remotes_page(plugins, chains, version):
    url = "https://nulljsfx.tech/remotes/"
    desc = ("Free Android remotes from NULL JSFX: your DAW's mixer and plugins, your digital mixer or orOS Studio, "
            "on your phone or tablet over Wi-Fi. No account, no Google.")
    image = "https://nulljsfx.tech/assets/img/remote/remote-for-daws.jpg"
    T, P, M = (1180, 820), (412, 860), (1280, 800)
    more = ('Also in <a href="https://useoros.online/download/#koulaxizis-apps" target="_blank" rel="noopener">Koulaxizis Apps</a>, '
            f'<a href="{FDROID}">our F-Droid repository</a> (F-Droid, Droid-ify) or Obtainium from <code>github.com/koulaxizis/null-jsfx</code>.')
    hero = f"""
            <header class="pp-hero">
                <span class="pp-kicker">Free apps &middot; Android 8.0+ &middot; phone and tablet &middot; no Google</span>
                <h1>NULL JSFX REMOTES</h1>
                <p class="pp-lead">Walk away from the computer. Your DAW, your mixer or orOS Studio on your phone or tablet, over your own Wi-Fi.</p>
                <div class="pp-actions">
                    <a class="featured-btn" href="#daws"><span>Remote for DAWs</span></a>
                    <a class="pp-ghost" href="#mixers"><span>Remote for Mixers</span></a>
                    <a class="pp-ghost" href="#studio"><span>Remote for orOS Studio</span></a>
                </div>
                <div class="pp-devices">{device("tablet", "mixers-desk", "Remote for Mixers on a tablet: the channels of a Behringer X AIR", *M)}{device("phone", "daws-phone", "Remote for DAWs on a phone: the mixer of Bitwig Studio", *P)}</div>
            </header>
            {stats([(7, "DAW tabs"), (8, "console families"), (len(plugins), "plugins to control"), (0, "accounts")])}"""
    daws = rows([
        ("Mixer", "Your DAW's mixer and transport",
         "Faders, pan, mute, solo and arm with meters, and the transport, for the DAW on your computer.",
         ["REAPER, Ardour and Mixbus, Bitwig Studio, Ableton Live, Cubase and FL Studio",
          "Finds REAPER on your network; Cubase and FL Studio over network MIDI"],
         device("tablet", "daws-desk", "Remote for DAWs on a tablet: Ardour's mixer and transport", *T)),
        ("Plugins", "The plugin's own interface",
         "Open a track's FX and every NULL JSFX plugin shows the interface it has on the computer, with its presets and A/B. What you turn moves in the DAW.",
         [], device("tablet", "daws-plugin", "Remote for DAWs: NULL Delay on an Ardour track", *T)),
        ("Sends", "Sends and FX tracks", "Set the sends of a track to its FX tracks, as in the DAW.", [],
         device("tablet", "daws-sends", "Remote for DAWs: the sends of a Bitwig track", *T)),
        ("Mackie Control &middot; beta", "Any DAW with Mackie Control",
         "Mixer, transport, meters and time display for Logic Pro, Studio One, Pro Tools, Cakewalk, Reason and other DAWs with Mackie Control, without plugins.",
         [], device("tablet", "daws-mackie", "Remote for DAWs: the Mackie Control tab", *T)),
    ])
    mixers = rows([
        ("Consoles", "Your digital mixer",
         "It finds the console on your network and knows its model. Channels, buses, FX returns and main: faders, mute, solo, pan and meters.",
         ["Behringer X AIR, X32 and WING, Midas M32", "Allen &amp; Heath SQ, Qu, CQ, dLive and Avantis; Yamaha CL, QL, TF and DM7"],
         device("tablet", "mixers-wing", "Remote for Mixers on a tablet: a Behringer WING", *M)),
        ("My mix", "A musician's own monitor mix",
         "Lock the app to one bus and hand it to the drummer: they set their own monitor mix and nothing else.",
         ["Monitor sends, DCAs and mute groups"], device("phone", "mixers-mymix", "Remote for Mixers on a phone: My mix, locked to one bus", *P)),
        ("Any console", "One app, many desks",
         "Allen &amp; Heath and Yamaha desks connect over the network too, with the same faders and mutes.", [],
         device("tablet", "mixers-sq", "Remote for Mixers on a tablet: an Allen &amp; Heath SQ", *M)),
    ])
    studio = f"""
            <section class="pp-row" id="studio">
                <div class="pp-text">
                    <span class="pp-kicker">Remote for orOS Studio</span>
                    <h2>orOS Studio from your phone</h2>
                    <p>Control orOS Studio on your computer while the sound plays there. Scan the QR code Studio shows and you are in.</p>
                    <ul class="pp-points"><li>Every NULL JSFX effect with its own interface, presets and A/B</li><li>Pads and keys for the NULL instruments, recorded into MIDI clips</li></ul>
                    <div class="pp-actions pp-left">
                        <a class="featured-btn" href="https://github.com/koulaxizis/oros/releases/download/android-apks/oros-studioremote.apk" rel="noopener">{DOWNLOAD_ICON}<span>Get the APK</span></a>
                        <a class="pp-ghost" href="https://useoros.online/studio/remote/" target="_blank" rel="noopener"><span>Or in a browser</span></a>
                    </div>
                </div>
                <div class="pp-media"><img class="pp-shot" src="/assets/img/remote/oros-studio-remote.jpg" width="1280" height="720" loading="lazy" decoding="async" alt="Remote for orOS Studio on a phone: the mixer, the drum pads and a NULL JSFX effect chain"></div>
            </section>"""

    def get(kind):
        return f"""
            <div class="pp-actions">
                <a class="featured-btn" href="{APK.format(kind)}" rel="noopener">{DOWNLOAD_ICON}<span>Get Remote for {kind}</span></a>
            </div>
            <p class="pp-sub pp-center">Beta: tested against software stand-ins, not yet on every real setup. {more}</p>"""
    steps = """
            <section class="pp-block">
                <h2 class="pp-h">Three steps</h2>
                <ol class="pp-steps">
                    <li><b>Install</b><span>The APK from here, F-Droid, Obtainium or Koulaxizis Apps.</span></li>
                    <li><b>Connect</b><span>Phone and computer or mixer on the same Wi-Fi: it finds REAPER and the consoles, or you type the address.</span></li>
                    <li><b>Mix</b><span>From the stage, the booth or the couch.</span></li>
                </ol>
            </section>"""
    body = (hero + '\n            <h2 class="pp-anchor" id="daws">Remote for DAWs <small>beta</small></h2>' + daws + get("DAWs")
            + '\n            <h2 class="pp-anchor" id="mixers">Remote for Mixers <small>beta</small></h2>' + mixers + get("Mixers")
            + studio + steps)
    ld = {"@context": "https://schema.org", "@type": "ItemList", "name": "NULL JSFX remotes", "itemListElement": [
        {"@type": "ListItem", "position": i + 1, "item": {"@type": "MobileApplication", "name": n, "operatingSystem": "Android 8.0+",
         "applicationCategory": "MultimediaApplication", "offers": {"@type": "Offer", "price": "0", "priceCurrency": "EUR"}}}
        for i, n in enumerate(["Remote for DAWs", "Remote for Mixers", "Remote for orOS Studio"])]}
    return shell("NULL JSFX Remotes: your DAW and mixer on Android, free", desc, url, image, body, "Remotes", ld)


def trailer_seconds():
    """The trailer's length, from its mp4 (the duration in the movie header, so no ffprobe in CI)."""
    import struct
    data = (ROOT / "assets/video/trailer.mp4").read_bytes()
    i = data.index(b"mvhd")
    version = data[i + 4]
    scale, length = struct.unpack(">IQ" if version else ">II", data[i + 24:i + 36] if version else data[i + 16:i + 24])
    return length / scale


def forms_block(plugins):
    """The homepage section "One plugin, every form": Delay as JSFX, GFX, VST3/CLAP/LV2, a Rack module
    and on the remote, from the pictures publish.py makes (a tile whose picture is missing is left out)."""
    e = html.escape
    delay = next(p for p in plugins if p["stem"] == "delay")
    sliders = []
    for line in (ROOT / "DATA/Effects/null_jsfx/delay.jsfx").read_text(encoding="utf-8").splitlines():
        m = re.match(r"slider\d+:([-\d.]+)<([-\d.]+),([-\d.]+),[^{>]*(?:\{([^}]*)\})?>(.+)", line)
        if m:
            v, lo, hi, names, label = m.groups()
            v, lo, hi = float(v), float(lo), float(hi)
            shown = names.split(",")[int(v)] if names else f"{v:g}"
            sliders.append((label, (v - lo) / (hi - lo) if hi > lo else 0, shown))
    lines = "".join(f'<div class="fm-slider"><span>{e(label)}</span><i style="--v:{f:.3f}"></i><b>{e(shown)}</b></div>'
                    for label, f, shown in sliders[:9])
    jsfx = f'<div class="fm-jsfx" role="img" aria-label="Delay with REAPER\'s own sliders, drawn by the site">{lines}</div>'
    tiles = [
        ("JSFX", "REAPER's own sliders (drawn by the site)", jsfx),
        ("GFX JSFX", "The NULL interface inside REAPER",
         f'<img src="/{delay["shot"]}" width="960" height="816" loading="lazy" decoding="async" alt="Delay as GFX JSFX">'),
        ("VST3 &middot; CLAP &middot; LV2", "Any DAW", picture("forms", "delay-native", "Delay as a VST3 and CLAP plugin", 960, 816, cls="fm-img")),
        ("Rack module", "One of eight slots", picture("forms", "delay-rack", "Delay in the Rack's fourth slot", 960, 680, cls="fm-img")),
        ("Remote", "On your tablet", picture("remotes", "daws-plugin", "Delay on Remote for DAWs", 1180, 820, cls="fm-img")),
    ]
    trailer = ""
    if (ROOT / "assets/video/trailer.mp4").exists():  # tools/video/make_trailer.py
        trailer = f"""
            <div class="fm-trailer">
                <h3>The whole suite in {round(trailer_seconds())} seconds</h3>
                {video("trailer", "NULL JSFX trailer: plugins, the Rack, the Hub and the remotes")}
            </div>"""
    cards = "".join(f'\n                <figure class="fm-tile">{media}<figcaption><b>{t}</b><span>{c}</span></figcaption></figure>'
                    for t, c, media in tiles if media)
    return f"""        <section class="forms" aria-labelledby="forms-title">
            <h2 class="section-title" id="forms-title">One plugin, every form</h2>
            <p class="forms-lead">Every NULL effect comes in each form you need, and each one is tested to sound the same. Here is <a href="{delay["page"]}">Delay</a>; hear it in your browser on its page.</p>
            <div class="fm-grid">{cards}
            </div>{trailer}
        </section>
"""


def files(plugins, chains, version):
    return [(ROOT / "rack/index.html", rack_page(plugins, chains, version)),
            (ROOT / "hub/index.html", hub_page(plugins, chains, version)),
            (ROOT / "remotes/index.html", remotes_page(plugins, chains, version))]
