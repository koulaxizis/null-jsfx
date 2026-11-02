// Media kit page: the first download of a visit shows the Ko-fi support step with
// "Skip to download" (R22), the same step and the same visit flag as the main download dialog;
// later downloads start right away. Without JavaScript the links download directly.
(function () {
    const modal = document.getElementById('mk-support-modal');
    const skip = document.getElementById('download-skip');
    if (!modal || !skip) return;
    const SEEN_KEY = 'nulljsfx-support-seen';

    function seen() {
        try { return sessionStorage.getItem(SEEN_KEY) === '1'; } catch (e) { return false; }
    }
    function markSeen() {
        try { sessionStorage.setItem(SEEN_KEY, '1'); } catch (e) { /* private mode */ }
    }
    function close() { modal.classList.remove('active'); }

    document.querySelectorAll('a.mk-dl').forEach(function (link) {
        link.addEventListener('click', function (e) {
            if (seen()) return;
            e.preventDefault();
            markSeen();
            skip.href = link.href;
            skip.textContent = 'Skip to download →';
            modal.classList.add('active');
        });
    });
    ['support-kofi', 'support-follow'].forEach(function (id) {
        document.getElementById(id).addEventListener('click', function () {
            skip.textContent = 'Continue to download →';
        });
    });
    skip.addEventListener('click', function () { setTimeout(close, 300); });
    document.getElementById('mk-support-close').addEventListener('click', close);
    modal.addEventListener('click', function (e) { if (e.target === modal) close(); });
    document.addEventListener('keydown', function (e) { if (e.key === 'Escape') close(); });
})();
