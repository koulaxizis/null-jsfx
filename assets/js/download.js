// Download modal: the Download button opens it (without JavaScript the button still leads to
// the GitHub releases page). It suggests the VST3/CLAP package for the visitor's system.
(function () {
    const base = 'https://github.com/koulaxizis/null-jsfx/releases/latest/download/';
    const modal = document.getElementById('download-modal');
    const button = document.getElementById('download-btn');
    if (!modal || !button) return;

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

    window.openDownload = function () { modal.classList.add('active'); };
    window.closeDownload = function () { modal.classList.remove('active'); };

    button.addEventListener('click', function (e) {
        e.preventDefault();
        openDownload();
    });
    modal.addEventListener('click', function (e) {
        if (e.target === modal) closeDownload();
    });

    const copy = document.getElementById('reapack-copy');
    copy.addEventListener('click', function () {
        const url = document.getElementById('reapack-url').textContent;
        const done = function () {
            copy.textContent = 'Copied';
            setTimeout(function () { copy.textContent = 'Copy'; }, 1500);
        };
        if (navigator.clipboard) {
            navigator.clipboard.writeText(url).then(done, function () {});
        }
    });
})();
