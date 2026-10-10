// "More than X downloads" under the Download button, from downloads.json on this site (written
// daily by the Count downloads workflow from GitHub's public release download counts). No
// request to any other server; without the file, or with 0, the line stays hidden.
(function () {
    const el = document.getElementById('download-count');
    if (!el || typeof fetch !== 'function') return;
    fetch('downloads.json', { cache: 'no-cache' }).then(function (r) {
        if (!r.ok) throw new Error('downloads.json ' + r.status);
        return r.json();
    }).then(function (data) {
        const n = parseInt(data && data.display, 10);
        if (!(n > 0)) return;
        el.innerHTML = 'More than <strong>' + n.toLocaleString('en-US') + '</strong> downloads';
        el.hidden = false;
    }).catch(function () { /* the line stays hidden */ });
})();
