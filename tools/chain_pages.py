"""The FX chain pages of the site (chains/<slug>/index.html), their signal-flow diagrams and the
small diagram on each chain card of index.html. Used by tools/build_site.py; the pictures of the
chain window (assets/img/chains/<slug>/<n>.webp) are made by tools/site_visuals/chain_shots.py.

Every chain comes from tools/build_chains.py: the plain CHAINS and the RACK_PRESETS that also
become a REAPER chain. What a page shows of the chain window is read from the generated GFX
chain itself (DATA/FXChains/NULL JSFX GFX/<chain>.RfxChain), so the page lists exactly what
REAPER opens, Band Splitter and Mid/Side Matrix included.
"""
import html
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PLUGINS = ROOT / "DATA/Effects/null_jsfx"
GFX_CHAINS = ROOT / "DATA/FXChains/NULL JSFX GFX"
SHOTS = ROOT / "assets/img/chains"
PAGES = ROOT / "chains"
CHAIN_TEXT = ROOT / "tools/site_chains.json"

sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / "tools"))
from build_chains import CHAINS, RACK_PRESETS, REAPER_ONLY, parse_sliders, rack_only, resolve  # noqa: E402

# the Rack's defaults that reaper_preset() in build_chains.py fills in
XOVER = {"xover_low": 200, "xover_high": 2500}
BANDS = ("Low", "Mid", "High")
SOURCE_NAMES = {"lfo1": "LFO 1", "lfo2": "LFO 2", "env": "Envelope"}


def slug(name):
    return re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")


def file_name(name):
    """The chain's file name in DATA/FXChains (build_chains.py writes "/" as "-")."""
    return name.replace("/", "-") + ".RfxChain"


def jsfx_name(stem):
    return (PLUGINS / f"{stem}.jsfx").read_text(encoding="utf-8").split("\n", 1)[0][len("desc:NULL "):]


def category(stem):
    return (PLUGINS / f"{stem}.jsfx").read_text(encoding="utf-8").split("\n")[3].split(": ", 1)[1]


def window_items(name):
    """[(stem, [slider values])] of the GFX chain file, in the order REAPER lists them."""
    text = (GFX_CHAINS / file_name(name)).read_text(encoding="utf-8")
    out = []
    for m in re.finditer(r'^<JS null_jsfx/gfx/(\w+)_gfx\.jsfx ""\n\s*([^\n]*)', text, re.M):
        values = []
        for v in m.group(2).split():
            if v == "-":
                break
            values.append(float(v))
        out.append((m.group(1), values))
    return out


def source_name(src):
    return SOURCE_NAMES.get(src) or f"Macro {src[5:]}"


def frequency(hz):
    return f"{hz / 1000:g} kHz" if hz >= 1000 else f"{hz:g} Hz"


def load():
    """Every chain that has a REAPER chain, sorted by name, as a dict:
    name, slug, text, kind ("Chain" or the Rack preset group), steps [{stem, band, channel,
    values [(index, value, name)], pins}], routes [(source, slot from 1, target, depth)], globs,
    uses (the routing tags of its card), items (window_items)."""
    text = json.loads(CHAIN_TEXT.read_text(encoding="utf-8"))
    out = []
    for name, fxs in CHAINS:
        steps = [{"stem": fx[0], "band": "Full", "channel": "Stereo", "values": resolve(name, fx[0], fx[1]),
                  "pins": len(fx) > 2} for fx in fxs]
        uses = ["Pin routing"] if any(s["pins"] for s in steps) else []
        out.append({"name": name, "steps": steps, "routes": [], "globs": {}, "uses": uses,
                    "reaper_only": name in REAPER_ONLY})
    for name, _, slots, routes, globs in RACK_PRESETS:
        if rack_only(name, slots, routes, globs):
            continue  # no REAPER chain
        steps = [{"stem": sl["id"], "band": sl["band"], "channel": sl["channel"],
                  "values": resolve(name, sl["id"], sl["values"]), "pins": False} for sl in slots]
        uses = []
        if any(s["band"] != "Full" or s["channel"] != "Stereo" for s in steps):
            uses.append("Pin routing")
        if routes:
            uses.append("Modulation")
        out.append({"name": name, "steps": steps, "routes": list(routes), "globs": dict(globs), "uses": uses,
                    "reaper_only": False})
    for c in out:
        c["slug"] = slug(c["name"])
        c["text"] = text[c["name"]]
        c["items"] = window_items(c["name"])
    slugs = [c["slug"] for c in out]
    if len(set(slugs)) != len(slugs):
        sys.exit(f"chain_pages: two chains share a page name: {sorted(s for s in slugs if slugs.count(s) > 1)}")
    return sorted(out, key=lambda c: c["name"].lower())


# ---- the signal-flow diagram -------------------------------------------------------------------

def segments(c):
    """The chain as [("fx", step index)] and [("split", "band" | "ms", [(row label, [step index])])]:
    a run of band steps goes between one Band Splitter split and merge, a run of Mid/Side steps
    between one Mid/Side Matrix encode and decode, as in build_chains.reaper_preset()."""
    g = {**XOVER, **c["globs"]}
    kinds = ["band" if s["band"] != "Full" else "ms" if s["channel"] != "Stereo" else "" for s in c["steps"]]
    out, k = [], 0
    while k < len(kinds):
        if not kinds[k]:
            out.append(("fx", k))
            k += 1
            continue
        j = k
        while j < len(kinds) and kinds[j] == kinds[k]:
            j += 1
        run = range(k, j)
        if kinds[k] == "band":
            labels = {"Low": f"Low · below {frequency(g['xover_low'])}",
                      "Mid": f"Mid · {frequency(g['xover_low'])} to {frequency(g['xover_high'])}",
                      "High": f"High · above {frequency(g['xover_high'])}"}
            rows = [(labels[b], [i for i in run if c["steps"][i]["band"] == b]) for b in BANDS]
        else:
            rows = [(ch, [i for i in run if c["steps"][i]["channel"] == ch]) for ch in ("Mid", "Side")]
        out.append(("split", kinds[k], rows))
        k = j
    return out


def wrap_name(name, width=17):
    """A plugin name on at most two lines of about width characters."""
    if len(name) <= width or " " not in name:
        return [name]
    words, lines = name.split(), [""]
    for w in words:
        if lines[-1] and len(lines[-1]) + 1 + len(w) > width and len(lines) == 1:
            lines.append(w)
        else:
            lines[-1] = (lines[-1] + " " + w).strip()
    return lines


def diagram(c, names, mini=False):
    """The chain's signal flow as inline SVG, coloured by the site's CSS (classes cd-*).
    names: {stem: display name}. mini: the small version of a chain card, without text."""
    e = html.escape
    if mini:
        bw, bh, gap, row_h, node, pad, term = 16, 10, 6, 14, 4, 2, 3
    else:
        bw, bh, gap, row_h, node, pad, term = 150, 56, 30, 76, 18, 10, 16
    segs = segments(c)

    def seg_width(s):
        if s[0] == "fx":
            return bw
        label = 0 if mini else 118
        longest = max(len(r[1]) for r in s[2])
        return node + gap + label + max(1, longest) * (bw + gap) + node

    rows_max = max([len(s[2]) for s in segs if s[0] == "split"] + [1])
    mods = {}
    for src, slot_n, target, depth in c["routes"]:
        mods.setdefault(src, []).append((slot_n - 1, target, depth))
    top = (0 if not mods else (14 if mini else 64)) + (24 if not mini and any(s[0] == "split" for s in segs) else 0)
    height = top + rows_max * row_h + (pad if mini else 24)
    mid = top + rows_max * row_h / 2 + (0 if mini else 4)
    width = term * 4 + gap * 2 + sum(seg_width(s) for s in segs) + gap * (len(segs) - 1) + pad * 2
    parts, boxes = [], {}
    x = pad + term * 2
    first_cat = category(c["steps"][0]["stem"])
    midi_in = first_cat in ("MIDI", "Instruments")

    def line(x1, y1, x2, y2, cls="cd-wire"):
        parts.append(f'<path class="{cls}" d="M{x1:g} {y1:g}H{x2:g}"/>' if y1 == y2
                     else f'<path class="{cls}" d="M{x1:g} {y1:g}V{y2:g}"/>' if x1 == x2
                     else f'<path class="{cls}" d="M{x1:g} {y1:g}H{(x1 + x2) / 2:g}V{y2:g}H{x2:g}"/>')

    def box(i, bx, by):
        s = c["steps"][i]
        boxes[i] = (bx, by)
        if mini:
            parts.append(f'<rect class="cd-box" x="{bx:g}" y="{by - bh / 2:g}" width="{bw}" height="{bh}" rx="2"/>')
            return
        parts.append(f'<rect class="cd-box" x="{bx:g}" y="{by - bh / 2:g}" width="{bw}" height="{bh}" rx="6"/>')
        lines = wrap_name(names.get(s["stem"]) or jsfx_name(s["stem"]))
        y0 = by - (len(lines) - 1) * 8 + 5
        for k, ln in enumerate(lines):
            parts.append(f'<text class="cd-name" x="{bx + bw / 2:g}" y="{y0 + k * 16:g}">{e(ln)}</text>')

    # the input
    if mini:
        parts.append(f'<circle class="cd-term" cx="{pad + term:g}" cy="{mid:g}" r="{term}"/>')
    else:
        parts.append(f'<circle class="cd-term" cx="{pad + term:g}" cy="{mid:g}" r="{term}"/>'
                     f'<text class="cd-term-label" x="{pad + term:g}" y="{mid + 4:g}">{"MIDI" if midi_in else "IN"}</text>')
    for n, s in enumerate(segs):
        line(x, mid, x + gap, mid)
        x += gap
        if s[0] == "fx":
            box(s[1], x, mid)
            x += bw
            continue
        _, kind, rows = s
        w = seg_width(s)
        ys = [mid + (k - (len(rows) - 1) / 2) * row_h for k in range(len(rows))]
        # the split and the merge: a bar each, with the REAPER plugin that does it
        for bx, label in ((x, "Band Splitter" if kind == "band" else "M/S Encode"),
                          (x + w - node, "Merge" if kind == "band" else "M/S Decode")):
            parts.append(f'<rect class="cd-node" x="{bx:g}" y="{ys[0] - (bh / 2 if not mini else 3):g}" width="{node}" '
                         f'height="{ys[-1] - ys[0] + (bh if not mini else 6):g}" rx="{2 if mini else 4}"/>')
            if not mini:
                parts.append(f'<text class="cd-node-label" x="{bx + node / 2:g}" y="{ys[0] - bh / 2 - 8:g}">{e(label)}</text>')
        for (label, steps), y in zip(rows, ys):
            rx = x + node
            end = x + w - node
            if not mini:
                parts.append(f'<text class="cd-row-label" x="{rx + 10:g}" y="{y - 8:g}">{e(label.split(" · ")[0])}</text>')
                if " · " in label:
                    parts.append(f'<text class="cd-row-sub" x="{rx + 10:g}" y="{y + 16:g}">{e(label.split(" · ")[1])}</text>')
                rx += 118
            cur = x + node
            for i in steps:
                line(cur, y, rx + gap, y)
                box(i, rx + gap, y)
                cur = rx + gap + bw
                rx = cur
            line(cur, y, end, y, "cd-wire" if steps else "cd-wire cd-pass")
        x += w
    line(x, mid, x + gap, mid)
    x += gap
    parts.append(f'<circle class="cd-term" cx="{x + term:g}" cy="{mid:g}" r="{term}"/>')
    if not mini:
        parts.append(f'<text class="cd-term-label" x="{x + term:g}" y="{mid + 4:g}">OUT</text>')
    # modulation: each source above the chain, a dashed line to every slot it moves
    if mods:
        order = sorted(mods, key=lambda src: sum(boxes[i][0] for i, _, _ in mods[src]) / len(mods[src]))
        pw = 14 if mini else 92
        xs, last = [], -1e9
        for src in order:
            want = sum(boxes[i][0] for i, _, _ in mods[src]) / len(mods[src]) + bw / 2 - pw / 2
            want = max(want, last + pw + (4 if mini else 12), pad)
            xs.append(want)
            last = want
        over = xs[-1] + pw - (width - pad) if xs else 0
        if over > 0:
            xs = [v - over for v in xs]
        py = 2 if mini else 8
        ph = 6 if mini else 26
        for src, px in zip(order, xs):
            for i, target, _ in mods[src]:
                bx, by = boxes[i]
                sx, sy = px + pw / 2, py + ph
                # a box above this one (a row of a split): come down left of it, into its left side
                covered = any(oy < by and abs(ox - bx) < bw for ox, oy in boxes.values())
                if covered:
                    lx = bx - (5 if mini else 14)
                    d = f"M{sx:g} {sy:g}C{sx:g} {sy + 20:g} {lx:g} {sy + 10:g} {lx:g} {sy + 30:g}V{by:g}H{bx:g}"
                else:
                    tx, ty = bx + bw / 2, by - bh / 2
                    d = f"M{sx:g} {sy:g}C{sx:g} {(sy + ty) / 2:g} {tx:g} {(sy + ty) / 2:g} {tx:g} {ty:g}"
                parts.append(f'<path class="cd-mod" d="{d}">'
                             + ("" if mini else f'<title>{e(source_name(src))} moves {e(target)}</title>') + '</path>')
            parts.append(f'<rect class="cd-src" x="{px:g}" y="{py}" width="{pw}" height="{ph}" rx="{3 if mini else 13}"/>')
            if not mini:
                parts.append(f'<text class="cd-src-label" x="{px + pw / 2:g}" y="{py + 17}">{e(source_name(src).upper())}</text>')
    label = "" if mini else f' role="img" aria-label="Signal flow of {e(c["name"])}"'
    hidden = ' aria-hidden="true"' if mini else ""
    # on a phone the big diagram shrinks to 55% before it scrolls sideways in its frame
    style = "" if mini else f' style="min-width:{round(width * 0.55)}px"'
    k = 1.6 if mini else 1  # the small one is drawn 1.6 times its units, the same for every card
    return (f'<svg class="{"cd-mini" if mini else "chain-diagram"}" viewBox="0 0 {width:g} {height:g}" width="{width * k:g}" '
            f'height="{height * k:g}"{style}{label}{hidden}>' + "".join(parts) + "</svg>")


# ---- the page ----------------------------------------------------------------------------------

def unit(label):
    m = re.search(r"\(([^)]*)\)\s*$", label)
    return m.group(1) if m else ""


def labels(stem):
    """The full slider labels of a plugin (with their units), by index from 0."""
    return [re.sub(r"^slider\d+:[^<]*<[^>]*>-?", "", ln).strip() if re.match(r"slider\d+:[^/]", ln)
            else ln.split(":")[-1].lstrip("-").strip()
            for ln in (PLUGINS / f"{stem}.jsfx").read_text(encoding="utf-8").split("\n") if re.match(r"slider\d+:", ln)]


def bare(label):
    """A slider label without its unit."""
    return re.sub(r"\s*\([^)]*\)\s*$", "", label)


def shown(stem, index, value):
    """A slider value as the plugin shows it: the option label, or the number and its unit."""
    sl = parse_sliders(PLUGINS / f"{stem}.jsfx")[index]
    if sl[4]:
        return sl[4][int(value)]
    u = unit(labels(stem)[index])
    v = f"{value:g}".replace("-", "−")
    return v + ("" if not u else u if u == "%" else " " + u)


def live_data(c, live_stems):
    """The live player's chain [{id, s: {slider: value}}], or None when it gets no player: pin
    routing and modulation do not run in the browser, nor do instruments or MIDI effects."""
    if c["uses"] or c["reaper_only"] or not all(s["stem"] in live_stems for s in c["steps"]):
        return None
    return [{"id": stem, "s": {str(k + 1): (int(v) if float(v).is_integer() else round(v, 6)) for k, v in enumerate(vals)}}
            for stem, vals in c["items"]]


def related(c, chains):
    mine = {s["stem"] for s in c["steps"]}
    near = sorted((d for d in chains if d is not c),
                  key=lambda d: (-len(mine & {s["stem"] for s in d["steps"]}), d["name"].lower()))
    return [d for d in near if mine & {s["stem"] for s in d["steps"]}][:6]


def page(c, chains, on_site, ctx):
    """chains/<slug>/index.html. on_site: {stem: plugin dict of build_site.load()}; ctx: what the
    page shares with build_site.py (SITE, VERSION, LIVE_BUTTONS, LOOP_NAMES, footer, n_plugins)."""
    e = html.escape
    site = ctx["SITE"]
    url = f"{site}chains/{c['slug']}/"
    names = {s: p["name"] for s, p in on_site.items()}
    n = len(c["steps"])
    kinds = sorted({category(s["stem"]) for s in c["steps"]})
    title = f"{c['name']} FX chain for REAPER: {n} free plugins | NULL JSFX"
    if len(title) > 60:
        title = f"{c['name']} FX chain for REAPER | NULL JSFX"
    if len(title) > 60:
        title = f"{c['name']} | NULL JSFX"
    # the meta description: the longest of these that fits in 160 characters (search results cut the rest)
    tail = f" A free REAPER FX chain of {n} NULL JSFX plugins"
    clause = re.split(r"(?<=[.;!?])\s", c["text"])[0].rstrip(".;!?")
    short = c["text"][:159].rsplit(" ", 1)[0].rstrip(",;:") + "…"
    desc = next(d for d in (
        f"{c['text']}{tail}: " + ", ".join(names.get(s["stem"]) or jsfx_name(s["stem"]) for s in c["steps"]) + ".",
        f"{c['text']}{tail}.", f"{clause}.{tail}.", c["text"], f"{clause}.", short) if len(d) <= 160)
    first = c["items"][0][0]
    image = f"{site}assets/img/chains/{c['slug']}/1.webp"
    ld = {"@context": "https://schema.org", "@type": "CreativeWork", "name": f"{c['name']} (NULL JSFX FX chain)",
          "description": c["text"], "url": url, "image": image, "license": "https://opensource.org/licenses/MIT",
          "author": {"@type": "Person", "name": "Christos Koulaxizis"},
          "isPartOf": {"@type": "SoftwareApplication", "name": "NULL JSFX", "url": site}}
    crumbs = {"@context": "https://schema.org", "@type": "BreadcrumbList", "itemListElement": [
        {"@type": "ListItem", "position": 1, "name": "NULL JSFX", "item": site},
        {"@type": "ListItem", "position": 2, "name": "FX Chains", "item": site + "#fx-chains"},
        {"@type": "ListItem", "position": 3, "name": c["name"], "item": url}]}
    tags = "".join(f"<span>{t}</span>" for t in ["JSFX", "GFX"] + [u.upper() for u in c["uses"]] +
                   ([] if c["reaper_only"] else ["RACK PRESET"]))

    # the chain window: the list of what REAPER opens and the picture of the selected plugin
    items = []
    for k, (stem, _) in enumerate(c["items"]):
        sw, sh = shot_size(stem)
        items.append(f'<li><button type="button" class="cw-item{" on" if k == 0 else ""}" data-shot="{k + 1}" data-w="{sw}" data-h="{sh}" '
                     f'data-alt="NULL {e(jsfx_name(stem))} GFX with the settings of the {e(c["name"])} chain" '
                     f'aria-pressed="{"true" if k == 0 else "false"}"><span class="cw-check" aria-hidden="true"></span>'
                     f'JS: NULL {e(jsfx_name(stem))} GFX</button></li>')
    w, h = shot_size(first)
    window = f"""
                <section class="chain-section">
                    <h2>As it opens in REAPER</h2>
                    <div class="chain-window" data-slug="{c['slug']}">
                        <div class="cw-title"><span class="cw-dots" aria-hidden="true"><i></i><i></i><i></i></span>FX: {e(c['name'])}</div>
                        <div class="cw-body">
                            <ol class="cw-list">{"".join(items)}</ol>
                            <div class="cw-view"><img class="cw-shot" src="/assets/img/chains/{c['slug']}/1.webp" width="{w}" height="{h}" alt="NULL {e(jsfx_name(first))} GFX with the settings of the {e(c['name'])} chain" loading="lazy" decoding="async"></div>
                        </div>
                    </div>
                    <p class="chain-note">Click a plugin in the list to see it. The pictures are the real NULL JSFX GFX interfaces, drawn with this chain's settings; the window around them is drawn by this site, so REAPER's own frame looks a little different.</p>
                </section>"""

    live = live_data(c, ctx["live_stems"])
    listen = ""
    if live:
        loops = "".join(f'<option value="{k}">{e(v)}</option>' for k, v in ctx["LOOP_NAMES"])
        listen = f"""
                <section class="chain-section">
                    <h2>Listen <span class="beta-badge">BETA</span></h2>
                    <div class="live demo-big" data-live="chain" data-chain="{e(json.dumps(live, separators=(',', ':')))}">{ctx['LIVE_BUTTONS']}
                        <label class="live-field">Loop <select class="live-loop">{loops}</select></label>
                        <span class="demo-note">Live: the whole chain runs in your browser on a loop of NULL JSFX instruments, every plugin with the chain's settings. Bypass it while it plays.</span>
                        <span class="live-status demo-note" role="status"></span>
                    </div>
                    <p class="chain-note">Beta: the chain is played by the web version of each plugin, the same code as orOS Studio, which matches the plugins within the suite's tests (1e-6). Playing several plugins at once in a browser is new, so tell us if it stutters.</p>
                </section>"""

    # what each plugin is set to
    rows = []
    for k, s in enumerate(c["steps"]):
        name = names.get(s["stem"]) or jsfx_name(s["stem"])
        link = f'<a href="/{on_site[s["stem"]]["page"]}">{e(name)}</a>' if s["stem"] in on_site else e(name)
        where = s["band"] if s["band"] != "Full" else s["channel"] if s["channel"] != "Stereo" else ""
        lab = labels(s["stem"])
        vals = "".join(f'<li><span>{e(bare(lab[i]))}</span><b>{e(shown(s["stem"], i, v))}</b></li>'
                       for i, v, _ in s["values"])
        rows.append(f"""
                        <article class="chain-setting">
                            <h3><span class="cs-n">{k + 1}</span>{link}{f' <small>{e(where)}</small>' if where else ''}</h3>
                            {f'<ul>{vals}</ul>' if vals else '<p class="chain-note">Its defaults.</p>'}
                        </article>""")
    settings = f"""
                <section class="chain-section">
                    <h2>The settings</h2>
                    <p class="chain-note">What the chain changes from each plugin's defaults. Every control stays free to change after you load it.</p>
                    <div class="chain-settings">{"".join(rows)}
                    </div>
                </section>"""

    mod = ""
    if c["routes"]:
        g = c["globs"]
        lis = []
        for src, slot_n, target, depth in c["routes"]:
            stem = c["steps"][slot_n - 1]["stem"]
            how = ""
            if src.startswith("lfo"):
                rate = g.get(f"{src}_sync") or (f"{g[src + '_rate']:g} Hz" if f"{src}_rate" in g else "")
                how = ", ".join(x for x in (g.get(f"{src}_shape", "Sine"), rate) if x)
            elif src == "env":
                how = f"follows the input level, attack {g.get('env_attack', 10):g} ms, release {g.get('env_release', 150):g} ms"
            lis.append(f'<li><b>{e(source_name(src))}</b> moves <b>{e(names.get(stem) or jsfx_name(stem))} · {e(target)}</b> '
                       f'by {depth * 100:+.0f}%{f" <small>({e(how)})</small>" if how else ""}</li>')
        macro = any(r[0].startswith("macro") for r in c["routes"])
        mod = f"""
                <section class="chain-section">
                    <h2>Modulation</h2>
                    <ul class="chain-mods">{"".join(lis)}</ul>
                    <p class="chain-note">In REAPER an LFO is a parameter modulation and the envelope an audio control signal on the control it moves{"; a macro is the control with the largest depth, and the others follow it through parameter links" if macro else ""}. The Rack preset of the same name has them as real LFOs, envelope and macro knobs.</p>
                </section>"""

    how = f"""
                <section class="chain-section">
                    <h2>Load it</h2>
                    <ol class="chain-steps-how">
                        <li>Install NULL JSFX with the <a href="/#hub">Hub</a> or the JSFX download: the chains come in its <code>FXChains</code> folder.</li>
                        <li>In REAPER, open a track's FX window and pick <b>FX &#8594; Add FX chain</b>, then <b>NULL JSFX</b> (REAPER's sliders) or <b>NULL JSFX GFX</b> (our interfaces), then <b>{e(c['name'])}</b>.</li>
                        {'' if c['reaper_only'] else f'<li>In any other DAW: load the <a href="/#rack">NULL JSFX Rack</a> (VST3 or CLAP) and pick its preset <b>{e(c["name"])}</b>.</li>'}
                    </ol>
                </section>"""
    near = related(c, chains)
    rel = "".join(f'<a class="related-card" href="/chains/{d["slug"]}/"><b>{e(d["name"])}</b><small>{e(d["text"])}</small></a>' for d in near)
    rel_html = f"""
        <section class="page-related">
            <h2 class="section-title">More chains with these plugins</h2>
            <div class="related-grid">{rel}</div>
            <p class="page-more"><a href="/#fx-chains">All {len(chains)} FX chains</a></p>
        </section>""" if near else ""
    live_js = """
    <script src="/assets/js/live.js"></script>""" if live else ""
    return f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{e(title)}</title>
    <meta name="description" content="{e(desc)}">
    <link rel="canonical" href="{url}">
    <meta property="og:type" content="website">
    <meta property="og:title" content="{e(c['name'])}: a free REAPER FX chain of NULL JSFX">
    <meta property="og:description" content="{e(desc)}">
    <meta property="og:url" content="{url}">
    <meta property="og:image" content="{image}">
    <meta name="twitter:card" content="summary_large_image">
    <meta name="twitter:title" content="{e(c['name'])}: a free REAPER FX chain of NULL JSFX">
    <meta name="twitter:description" content="{e(desc)}">
    <meta name="twitter:image" content="{image}">
    <link rel="icon" type="image/svg+xml" href="/assets/img/favicon.svg">
    <link rel="stylesheet" href="/assets/css/style.css">
    <script type="application/ld+json">{json.dumps(ld)}</script>
    <script type="application/ld+json">{json.dumps(crumbs)}</script>
</head>
<body class="plugin-page chain-page">
    <main>
        <nav class="page-crumbs" aria-label="Breadcrumb">
            <a href="/" class="page-home"><img src="/assets/img/favicon.svg" alt="NULL JSFX" width="28" height="28"></a>
            <a href="/">NULL JSFX</a> <span>/</span> <a href="/#fx-chains">FX Chains</a> <span>/</span> <span>{e(c['name'])}</span>
        </nav>
        <article class="page-chain">
            <header class="chain-hero">
                <span class="guide-category">FX chain &middot; {n} plugin{'s' if n != 1 else ''} &middot; {e(' & '.join(kinds))}</span>
                <h1>{e(c['name'].upper())}</h1>
                <p>{e(c['text'])}</p>
                <div class="plugin-formats">{tags}</div>
            </header>
            <section class="chain-section chain-flow">
                <h2>Signal flow</h2>
                <div class="chain-diagram-wrap">{diagram(c, names)}</div>
            </section>{window}{listen}{settings}{mod}{how}
            <div class="page-actions">
                <a class="featured-btn" href="/#download"><svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/></svg><span>Download free</span></a>
                <small>Part of NULL JSFX {ctx['VERSION']}: {ctx['n_plugins']} plugins and {len(chains)} FX chains, free and open source (MIT).</small>
            </div>
        </article>{rel_html}
    </main>
{FOOTER}
    <script src="/assets/js/theme.js"></script>
    <script src="/assets/js/chain.js"></script>{live_js}
</body>
</html>
"""


FOOTER = """    <footer class="site-footer">
        <div class="footer-content">
            Designed with &#9825; by <a href="https://koulaxizis.gr" target="_blank" rel="noopener">Christos Koulaxizis</a>.
            <div class="footer-links">
                <a href="/support/">Support NULL JSFX</a>
                <span class="sep">&bull;</span>
                <a href="https://koulaxizis.gr/work-with-me/?lang=en#null-jsfx" target="_blank" rel="noopener">Work with me</a>
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
    </button>"""


def shot_size(stem):
    """The size of a chain window picture: twice the plugin's @gfx size (chain_shots.py)."""
    text = (PLUGINS / "gfx" / f"{stem}_gfx.jsfx").read_text(encoding="utf-8")
    w, h = (int(v) for v in re.search(r"^@gfx (\d+) (\d+)", text, re.M).groups())
    return w * 2, h * 2
