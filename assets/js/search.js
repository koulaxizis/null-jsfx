// Plugin Search Functionality
(function() {
    const searchInput = document.getElementById('plugin-search');
    const searchClear = document.querySelector('.search-clear');
    const searchCount = document.getElementById('search-count');
    const pluginCards = document.querySelectorAll('.plugin-card[data-name]');

    if (!searchInput || !searchClear || !searchCount) return;

    let debounceTimer;

    function normalizeText(text) {
        return text.toLowerCase().trim().replace(/\s+/g, ' ');
    }

    function filterPlugins(query) {
        const normalizedQuery = normalizeText(query);
        let visibleCount = 0;

        pluginCards.forEach(card => {
            const dataName = card.getAttribute('data-name') || '';
            const pluginName = card.querySelector('h3')?.textContent || '';
            const pluginDesc = card.querySelector('.plugin-desc')?.textContent || '';

            const combinedText = normalizeText(`${dataName} ${pluginName} ${pluginDesc}`);
            const matches = combinedText.includes(normalizedQuery);
            const isComingSoon = card.classList.contains('coming-soon');

            if (matches || isComingSoon) {
                card.classList.remove('hidden');
                if (!isComingSoon) visibleCount++;
            } else {
                card.classList.add('hidden');
            }
        });

        if (query.trim() === '') {
            searchCount.textContent = '';
        } else {
            searchCount.textContent = visibleCount > 0 ?
                `${visibleCount} plugin${visibleCount !== 1 ? 's' : ''} found` :
                'No plugins match your search';
        }
    }

    function clearSearch() {
        searchInput.value = '';
        searchClear.style.display = 'none';
        searchCount.textContent = '';
        pluginCards.forEach(card => {
            card.classList.remove('hidden');
        });
        searchInput.focus();
    }

    window.clearSearch = clearSearch;

    searchInput.addEventListener('input', function(e) {
        clearTimeout(debounceTimer);
        const query = e.target.value.trim();
        searchClear.style.display = query.length > 0 ? 'block' : 'none';
        debounceTimer = setTimeout(function() {
            filterPlugins(query);
        }, 150);
    });

    searchClear.addEventListener('click', clearSearch);

    document.addEventListener('keydown', function(e) {
        if (e.key === '/' && document.activeElement !== searchInput && !document.activeElement.matches('input, textarea, button')) {
            e.preventDefault();
            searchInput.focus();
        }
        if (e.key === 'Escape' && document.activeElement === searchInput) {
            clearSearch();
        }
    });
})();