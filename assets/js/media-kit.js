// Media kit page: a download shows the support step with "Skip to download" and
// "Don't show this again" (R22), the same step and the same 30-day rule as the main download
// dialog. Without JavaScript the links download directly.
(function () {
    const modal = document.getElementById('mk-support-modal');
    const skip = document.getElementById('download-skip');
    if (!modal || !skip) return;
    // R22: the support step shows at most once every 30 days in this browser
    // (localStorage 'nulljsfx-support-seen', the time it last showed);
    // "Don't show this again" ('nulljsfx-support-off') turns it off for good.
    // Without storage it shows every time, as a plain step.
    const SEEN_KEY = 'nulljsfx-support-seen';
    const OFF_KEY = 'nulljsfx-support-off';
    const GAP = 30 * 24 * 60 * 60 * 1000;
    const never = document.getElementById('support-never');
    function supportDue() {
        try {
            if (localStorage.getItem(OFF_KEY) === '1') return false;
            const t = parseInt(localStorage.getItem(SEEN_KEY) || '0', 10);
            return !(t > 0 && Date.now() - t < GAP);
        } catch (e) { return true; }
    }
    function markSupportSeen() {
        try { localStorage.setItem(SEEN_KEY, String(Date.now())); } catch (e) { /* private mode */ }
    }
    function rememberChoice() {
        if (!never || !never.checked) return;
        try { localStorage.setItem(OFF_KEY, '1'); } catch (e) { /* private mode */ }
    }
    function close() { rememberChoice(); modal.classList.remove('active'); }

    document.querySelectorAll('a.mk-dl').forEach(function (link) {
        link.addEventListener('click', function (e) {
            if (!supportDue()) return;
            e.preventDefault();
            markSupportSeen();
            skip.href = link.href;
            skip.textContent = 'Skip to download →';
            modal.classList.add('active');
        });
    });
    document.getElementById('support-page').addEventListener('click', function () {
        skip.textContent = 'Continue to download →';
    });
    skip.addEventListener('click', function () { setTimeout(close, 300); });
    document.getElementById('mk-support-close').addEventListener('click', close);
    modal.addEventListener('click', function (e) { if (e.target === modal) close(); });
    document.addEventListener('keydown', function (e) { if (e.key === 'Escape') close(); });
})();
