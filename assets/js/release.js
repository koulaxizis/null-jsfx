// Shows the latest GitHub release under the Download button: its version, its date and a
// NEW tag for the first 30 days. Without the API (offline, rate limit) the page keeps the
// version written in the HTML.
(function () {
    const API = 'https://api.github.com/repos/koulaxizis/null-jsfx/releases/latest';
    const CACHE_KEY = 'nulljsfx-latest-release';
    const CACHE_MS = 60 * 60 * 1000;
    const NEW_DAYS = 30;

    function show(release) {
        if (!release || !release.tag) return;
        const version = document.getElementById('release-version');
        const date = document.getElementById('last-update');
        const tag = document.getElementById('release-new');
        const published = new Date(release.published);
        if (version) version.textContent = /^\d/.test(release.tag) ? 'v' + release.tag : release.tag;
        if (date && !isNaN(published)) {
            date.textContent = 'Released: ' + published.toLocaleDateString('en-GB', { day: '2-digit', month: '2-digit', year: '2-digit' });
        }
        if (tag && !isNaN(published) && Date.now() - published.getTime() < NEW_DAYS * 86400000) {
            tag.hidden = false;
        }
    }

    function cached() {
        try {
            const c = JSON.parse(localStorage.getItem(CACHE_KEY) || 'null');
            return c && Date.now() - c.time < CACHE_MS ? c.release : null;
        } catch (e) { return null; }
    }

    const hit = cached();
    if (hit) { show(hit); return; }
    fetch(API).then(function (r) {
        if (!r.ok) throw new Error('GitHub API ' + r.status);
        return r.json();
    }).then(function (data) {
        const release = { tag: data.tag_name, published: data.published_at };
        try { localStorage.setItem(CACHE_KEY, JSON.stringify({ time: Date.now(), release: release })); } catch (e) { /* storage off */ }
        show(release);
    }).catch(function () { /* keep the HTML values */ });
})();
