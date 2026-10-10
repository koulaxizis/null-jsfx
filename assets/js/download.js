// Download modal: the Download button opens it (without JavaScript the button still leads to
// the GitHub releases page). It suggests the VST3/CLAP package for the visitor's system.
// The first download of a visit shows a short support step (Ko-fi, newsletter) with
// "Skip to download"; later downloads in the same visit start right away.
(function () {
    const base = 'https://github.com/koulaxizis/null-jsfx/releases/latest/download/';
    const modal = document.getElementById('download-modal');
    const button = document.getElementById('download-btn');
    if (!modal || !button) return;

    const options = modal.querySelector('.download-options');
    const support = document.getElementById('download-support');
    const skip = document.getElementById('download-skip');
    const SEEN_KEY = 'nulljsfx-support-seen';

    function detectOS() {
        const ua = navigator.userAgent || '';
        const platform = (navigator.userAgentData && navigator.userAgentData.platform) || navigator.platform || '';
        if (/Mac|iPhone|iPad/i.test(platform) || /Mac OS X/i.test(ua)) return 'macOS';
        if (/Linux|X11/i.test(platform) || /Linux/i.test(ua)) return 'Linux';
        return 'Windows';
    }

    const os = detectOS();
    const file = 'NULL-JSFX-VST3-CLAP-' + os + '.zip';
    document.getElementById('dl-os').textContent = os;
    document.getElementById('dl-native').href = base + file;
    document.getElementById('dl-native-file').textContent = file;

    function supportSeen() {
        try { return sessionStorage.getItem(SEEN_KEY) === '1'; } catch (e) { return false; }
    }
    function markSupportSeen() {
        try { sessionStorage.setItem(SEEN_KEY, '1'); } catch (e) { /* private mode */ }
    }

    function showOptions() {
        support.hidden = true;
        options.hidden = false;
        skip.textContent = 'Skip to download →';
    }

    window.openDownload = function () { showOptions(); modal.classList.add('active'); };
    window.closeDownload = function () { modal.classList.remove('active'); };

    button.addEventListener('click', function (e) {
        e.preventDefault();
        openDownload();
    });
    modal.addEventListener('click', function (e) {
        if (e.target === modal) closeDownload();
    });

    // every package link in the modal goes through the support step once per visit
    options.querySelectorAll('a[href*="/releases/latest/download/"]').forEach(function (link) {
        link.addEventListener('click', function (e) {
            if (supportSeen()) return;
            e.preventDefault();
            markSupportSeen();
            skip.href = link.href;
            options.hidden = true;
            support.hidden = false;
        });
    });

    // after a visit to Ko-fi or the newsletter, the skip link reads as the way on
    ['support-kofi', 'support-news'].forEach(function (id) {
        document.getElementById(id).addEventListener('click', function () {
            skip.textContent = 'Continue to download →';
        });
    });
    skip.addEventListener('click', function () {
        setTimeout(closeDownload, 300);
    });
})();
