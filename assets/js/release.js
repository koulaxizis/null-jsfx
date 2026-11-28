// Shows a NEW tag under the Download button for the first 30 days after a release.
// The version and the date are written into index.html with every release (Bible Part VI.1),
// so the page makes no request to GitHub and stores nothing in the browser (privacy, 2026-11-23).
(function () {
    const NEW_DAYS = 30;
    // Up to 4.2 this script cached the GitHub API answer here; clear what old visits left.
    try { localStorage.removeItem('nulljsfx-latest-release'); } catch (e) { /* storage off */ }
    const date = document.getElementById('last-update');
    const tag = document.getElementById('release-new');
    if (!date || !tag) return;
    const m = /(\d{2})\/(\d{2})\/(\d{2})/.exec(date.textContent);
    if (!m) return;
    const published = new Date(2000 + parseInt(m[3], 10), parseInt(m[2], 10) - 1, parseInt(m[1], 10));
    if (!isNaN(published) && Date.now() - published.getTime() < NEW_DAYS * 86400000) {
        tag.hidden = false;
    }
})();
