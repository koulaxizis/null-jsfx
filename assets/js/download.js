// Download modal: the Download button opens it (without JavaScript the button still leads to
// the GitHub releases page). It suggests NULL JSFX Hub, the installer and the VST3/CLAP
// package for the visitor's system.
// A download shows a short support step (the /support/ page: merch, a one-time donation, services)
// with "Skip to download" and "Don't show this again", at most once every 30 days (R22).
(function () {
    const base = 'https://github.com/koulaxizis/null-jsfx/releases/latest/download/';
    const modal = document.getElementById('download-modal');
    const button = document.getElementById('download-btn');
    if (!modal || !button) return;

    const options = modal.querySelector('.download-options');
    const support = document.getElementById('download-support');
    const skip = document.getElementById('download-skip');
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
    // the Linux package also holds LV2 (from 2.7)
    if (os === 'Linux') document.querySelector('#dl-native .download-option-title').firstChild.textContent = 'VST3, CLAP & LV2 for ';
    document.getElementById('dl-native').href = base + file;
    document.getElementById('dl-native-file').textContent = file;
    const installer = 'NULL-JSFX-Installer-' + os + { Windows: '.exe', macOS: '.pkg', Linux: '.sh' }[os];
    document.getElementById('dl-installer-os').textContent = os;
    document.getElementById('dl-installer').href = base + installer;
    document.getElementById('dl-installer-file').textContent = installer;
    const hub = { Windows: 'NULL-JSFX-Hub-Windows-Setup.exe', macOS: 'NULL-JSFX-Hub-macOS.dmg', Linux: 'NULL-JSFX-Hub-Linux.AppImage' }[os];
    document.getElementById('dl-hub-os').textContent = os;
    document.getElementById('dl-hub').href = base + hub;
    document.getElementById('dl-hub-file').textContent = hub;

    function showOptions() {
        support.hidden = true;
        options.hidden = false;
        skip.textContent = 'Skip to download →';
    }

    window.openDownload = function () { showOptions(); modal.classList.add('active'); };
    window.closeDownload = function () { rememberChoice(); modal.classList.remove('active'); };

    // #download in the address (the plugin pages link there) opens the window
    if (location.hash === '#download') {
        history.replaceState(null, '', location.pathname + location.search);
        openDownload();
    }

    button.addEventListener('click', function (e) {
        e.preventDefault();
        openDownload();
    });
    modal.addEventListener('click', function (e) {
        if (e.target === modal) closeDownload();
    });

    // a package link goes through the support step when it is due
    options.querySelectorAll('a[href*="/releases/latest/download/"]').forEach(function (link) {
        link.addEventListener('click', function (e) {
            if (!supportDue()) return;
            e.preventDefault();
            markSupportSeen();
            skip.href = link.href;
            options.hidden = true;
            support.hidden = false;
        });
    });

    // after a visit to /support/, the skip link reads as the way on
    document.getElementById('support-page').addEventListener('click', function () {
        skip.textContent = 'Continue to download →';
    });
    skip.addEventListener('click', function () {
        rememberChoice();
        setTimeout(closeDownload, 300);
    });
})();
