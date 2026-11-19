// "X downloads" (the exact count) under the Download button, from downloads.json on this site (written
// daily by the Count downloads workflow from GitHub's public release download counts). No
// request to any other server; without the file, or with 0, the line stays hidden.
//
// Android (APPS-DL): downloads.json "android" holds {package: {channel: n}}, copied by the same bot
// from the Koulaxizis Apps download counter. Elements with data-dl="pkg,prefix." (a trailing "."
// is a prefix) and optional data-dl-ch="manager,…" get the sum in their [data-n] child and are
// shown once it is above 0; data-dl-break="pkg" lists the channels ("Koulaxizis Apps 12 · Direct
// APK 30 · …").
(function () {
    const el = document.getElementById('download-count');
    if (typeof fetch !== 'function') return;
    const NAMES = { manager: 'Koulaxizis Apps', github: 'Direct APK', fdroid: 'F-Droid', droidify: 'Droid-ify',
        obtainium: 'Obtainium', fdroid_org: 'f-droid.org', other: 'Other', estimate: 'before 19/11 (estimate)' };
    const ORDER = ['manager', 'github', 'fdroid', 'droidify', 'obtainium', 'fdroid_org', 'other', 'estimate'];
    const list = (s) => String(s || '').split(',').map((x) => x.trim()).filter(Boolean);
    const fits = (want, pkg) => want.some((p) => p === pkg || (p.endsWith('.') && pkg.startsWith(p)));
    const fmt = (n) => n.toLocaleString('en-US');
    function sum(apps, want, chs) {
        let n = 0;
        for (const pkg of Object.keys(apps)) {
            if (!fits(want, pkg)) continue;
            for (const ch of Object.keys(apps[pkg])) {
                const v = apps[pkg][ch];
                if (typeof v === 'number' && (!chs.length || chs.includes(ch))) n += v;
            }
        }
        return n;
    }
    function android(apps) {
        document.querySelectorAll('[data-dl]').forEach((box) => {
            const n = sum(apps, list(box.getAttribute('data-dl')), list(box.getAttribute('data-dl-ch')));
            if (!(n > 0)) return;
            (box.querySelector('[data-n]') || box).textContent = fmt(n);
            box.hidden = false;
        });
        document.querySelectorAll('[data-dl-break]').forEach((box) => {
            const row = apps[box.getAttribute('data-dl-break')];
            if (!row) return;
            const parts = Object.keys(row).filter((ch) => row[ch] > 0)
                .sort((a, b) => (ORDER.indexOf(a) + 99) % 99 - (ORDER.indexOf(b) + 99) % 99)
                .map((ch) => (NAMES[ch] || ch) + ' ' + fmt(row[ch]));
            if (!parts.length) return;
            box.textContent = parts.join(' · ');
            box.hidden = false;
        });
    }
    fetch('downloads.json', { cache: 'no-cache' }).then(function (r) {
        if (!r.ok) throw new Error('downloads.json ' + r.status);
        return r.json();
    }).then(function (data) {
        if (data && data.android && typeof data.android === 'object') android(data.android);
        const n = parseInt(data && data.display, 10);
        if (!el || !(n > 0)) return;
        el.innerHTML = '<strong>' + n.toLocaleString('en-US') + '</strong> downloads';
        el.hidden = false;
    }).catch(function () { /* the lines stay hidden */ });
})();
