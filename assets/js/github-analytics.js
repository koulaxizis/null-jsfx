// GitHub Releases Download Tracker
(function() {
    var REPO_OWNER = 'koulaxizis';
    var REPO_NAME = 'null-jsfx';
    var GITHUB_API_URL = 'https://api.github.com/repos/' + REPO_OWNER + '/' + REPO_NAME + '/releases';

    var CACHE_KEY = 'nulljsfx-github-downloads';
    var CACHE_DURATION = 2 * 60 * 60 * 1000;

    async function fetchGitHubDownloads() {
        try {
            var cached = JSON.parse(localStorage.getItem(CACHE_KEY) || 'null');
            if (cached && (Date.now() - cached.timestamp < CACHE_DURATION)) {
                updateDownloadCount(cached.downloads);
                return cached.downloads;
            }

            var response = await fetch(GITHUB_API_URL);

            if (!response.ok) {
                throw new Error('GitHub API error: ' + response.status);
            }

            var releases = await response.json();
            var totalDownloads = 0;

            releases.forEach(function(release) {
                if (release.assets && Array.isArray(release.assets)) {
                    release.assets.forEach(function(asset) {
                        totalDownloads += asset.download_count || 0;
                    });
                }
            });

            localStorage.setItem(CACHE_KEY, JSON.stringify({
                downloads: totalDownloads,
                timestamp: Date.now()
            }));

            updateDownloadCount(totalDownloads);
            return totalDownloads;

        } catch (error) {
            console.warn('[NULL JSFX] GitHub downloads fetch failed:', error);
            updateDownloadCount(0);
            return 0;
        }
    }

    function updateDownloadCount(count) {
        var countDisplays = document.querySelectorAll('.download-count');
        countDisplays.forEach(function(el) {
            el.textContent = count.toLocaleString();
        });
    }

    fetchGitHubDownloads();

    setInterval(fetchGitHubDownloads, 30 * 60 * 1000);

    window.refreshGitHubDownloads = fetchGitHubDownloads;
})();