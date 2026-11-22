"""The site's own "page not found": 404.html at the root, which GitHub Pages serves for any
unknown address at any depth (/nope, /plugins/tilts/, /chains/x/y.html). Used by
tools/build_site.py, so its list of plugins and FX chains comes from the repo and cannot drift
(R14: never edited by hand).

Every link and asset path is absolute, because the page answers at any depth. It is noindex and has
no canonical, og or JSON-LD: it is not a page of its own. Without JavaScript it still shows the way
home, to the plugins, the download and the product pages; with it, it reads the address and offers
the closest plugin, chain or page ("Did you mean"), and a search box that jumps to one by name.
"""
import html
import json

from chain_pages import FOOTER  # the same footer as the plugin, chain and product pages

# the hand-made pages that are not plugins or chains: (name, address)
PAGES = [("NULL JSFX Rack", "/rack/"), ("NULL JSFX Hub", "/hub/"), ("Remotes", "/remotes/"),
         ("Support NULL JSFX", "/support/"), ("Media kit", "/media-kit/")]

DOWNLOAD_ICON = ('<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true">'
                 '<path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/></svg>')
SEARCH_ICON = ('<svg class="search-icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true">'
               '<circle cx="11" cy="11" r="8"/><path d="m21 21-4.35-4.35"/></svg>')

# reads the address, finds the closest entry by its last path part (exact, prefix, then edit
# distance) and offers it; the search box goes to an exact name, else to the closest one
SCRIPT = """(function () {
    var all = JSON.parse(document.getElementById('nf-data').textContent);
    function norm(s) {
        try { s = decodeURIComponent(s); } catch (e) {}
        return s.toLowerCase().replace(/\\.html?$/, '').replace(/^index$/, '').replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '');
    }
    function key(url) { var parts = url.split('/').filter(Boolean); return parts[parts.length - 1] || ''; }
    function dist(a, b) {
        var prev = [], cur, i, j;
        for (j = 0; j <= b.length; j++) prev[j] = j;
        for (i = 1; i <= a.length; i++) {
            cur = [i];
            for (j = 1; j <= b.length; j++)
                cur[j] = Math.min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] === b[j - 1] ? 0 : 1));
            prev = cur;
        }
        return prev[b.length];
    }
    function closest(word) {
        var w = norm(word), best = null, score = 1e9;
        if (w.length < 2) return null;
        all.forEach(function (e) {
            [key(e[1]), norm(e[0])].forEach(function (k) {
                var s = k === w ? 0 : (w.length >= 3 && (k.indexOf(w) === 0 || w.indexOf(k) === 0)) ? 1 : 2 + dist(w, k);
                if (s < score) { score = s; best = e; }
            });
        });
        return score <= 2 + Math.max(2, Math.floor(w.length / 4)) ? best : null;
    }
    var parts = location.pathname.split('/').filter(Boolean);
    var guess = null;
    for (var i = parts.length - 1; i >= 0 && !guess; i--) guess = closest(parts[i]);
    if (guess && guess[1] !== location.pathname) {
        var a = document.getElementById('nf-guess-link');
        a.href = guess[1];
        a.textContent = guess[0];
        document.getElementById('nf-guess').hidden = false;
    }
    var list = document.getElementById('nf-names');
    all.forEach(function (e) { var o = document.createElement('option'); o.value = e[0]; list.appendChild(o); });
    var form = document.getElementById('nf-search'), input = document.getElementById('nf-input');
    var status = document.getElementById('nf-status');
    form.hidden = false;
    form.addEventListener('submit', function (ev) {
        ev.preventDefault();
        var q = input.value.trim().toLowerCase();
        var hit = all.filter(function (e) { return e[0].toLowerCase() === q; })[0] || closest(q);
        if (hit) location.href = hit[1];
        else status.textContent = q ? 'Nothing close to that. Try the full list of plugins.' : '';
    });
})();"""


def page(plugins, chains):
    """404.html: [(name, address)] of every plugin, chain and product page, and the ways back."""
    e = html.escape
    entries = [[p["name"], "/" + p["page"]] for p in plugins] + \
        [[c["name"] + " (FX chain)", f"/chains/{c['slug']}/"] for c in chains] + [[n, u] for n, u in PAGES]
    data = json.dumps(entries, separators=(",", ":")).replace("</", "<\\/")
    return f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Page not found | NULL JSFX</title>
    <meta name="description" content="This page of NULL JSFX does not exist. Find the plugin, FX chain or page you were looking for.">
    <meta name="robots" content="noindex">
    <meta name="theme-color" content="#1b1a18">
    <link rel="icon" type="image/svg+xml" href="/assets/img/favicon.svg">
    <link rel="stylesheet" href="/assets/css/style.css">
</head>
<body class="plugin-page product-page not-found-page">
    <main>
        <nav class="page-crumbs" aria-label="Breadcrumb">
            <a href="/" class="page-home"><img src="/assets/img/favicon.svg" alt="NULL JSFX" width="28" height="28"></a>
            <a href="/">NULL JSFX</a> <span>/</span> <span>Page not found</span>
        </nav>
        <article class="page-product">
            <header class="pp-hero nf-hero">
                <span class="pp-kicker">404 &middot; Page not found</span>
                <h1>NOTHING HERE</h1>
                <p class="pp-lead">This address leads nowhere: the page never existed, or it moved. Every one of the {len(plugins)} plugins has its own page, and so do the {len(chains)} FX chains.</p>
                <p class="nf-guess" id="nf-guess" hidden>Did you mean <a id="nf-guess-link" href="/">NULL JSFX</a>?</p>
                <form class="nf-search" id="nf-search" role="search" hidden>
                    <div class="search-wrapper">
                        {SEARCH_ICON}
                        <input type="search" id="nf-input" class="plugin-search-input" list="nf-names" placeholder="Search plugins and FX chains..." aria-label="Search plugins and FX chains" autocomplete="off">
                    </div>
                    <datalist id="nf-names"></datalist>
                    <span class="nf-status" id="nf-status" role="status"></span>
                </form>
                <div class="pp-actions">
                    <a class="featured-btn" href="/">Home</a>
                    <a class="pp-ghost" href="/#tab-effects">All plugins</a>
                    <a class="pp-ghost" href="/#download">{DOWNLOAD_ICON}<span>Download</span></a>
                    <a class="pp-ghost" href="/hub/">Hub</a>
                    <a class="pp-ghost" href="/rack/">Rack</a>
                </div>
            </header>
        </article>
    </main>
{FOOTER}
    <script type="application/json" id="nf-data">{data}</script>
    <script src="/assets/js/theme.js"></script>
    <script>
{SCRIPT}
    </script>
</body>
</html>
"""
