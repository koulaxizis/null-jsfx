// Plugin Search Functionality
(function() {
    const searchInput = document.getElementById('plugin-search');
    const searchClear = document.querySelector('.search-clear');
    const searchCount = document.getElementById('search-count');
    const pluginCards = document.querySelectorAll('.plugin-card[data-name]');

    if (!searchInput || !searchClear || !searchCount) return;

    let debounceTimer;
    let activeTag = '';
    const chips = document.querySelectorAll('.tag-chip');

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
            const tagged = !activeTag || (card.dataset.tags || '').split('|').includes(activeTag);
            const matches = tagged && combinedText.includes(normalizedQuery);
            const isComingSoon = card.classList.contains('coming-soon');

            if (matches || isComingSoon) {
                card.classList.remove('hidden');
                if (!isComingSoon) visibleCount++;
            } else {
                card.classList.add('hidden');
            }
        });

        // hide a category heading when none of its plugins match
        document.querySelectorAll('.plugin-category').forEach(group => {
            group.classList.toggle('hidden', !group.querySelector('.plugin-card[data-name]:not(.hidden)'));
        });

        if (query.trim() === '' && !activeTag) {
            searchCount.textContent = '';
        } else {
            searchCount.textContent = visibleCount > 0 ?
                `${visibleCount} plugin${visibleCount !== 1 ? 's' : ''} found` :
                'No plugins match your search';
        }
        // tabs.js shows every plugin tab while a search or a use filter is on
        document.dispatchEvent(new CustomEvent('plugin-filter', { detail: { active: query.trim() !== '' || activeTag !== '' } }));
    }

    function setTag(tag) {
        activeTag = tag;
        chips.forEach(c => {
            const on = c.dataset.tag === tag;
            c.classList.toggle('active', on);
            c.setAttribute('aria-pressed', on ? 'true' : 'false');
        });
        filterPlugins(searchInput.value);
    }
    chips.forEach(c => c.addEventListener('click', () => setTag(c.dataset.tag === activeTag ? '' : c.dataset.tag)));
    window.setPluginTag = setTag;

    function clearSearch() {
        searchInput.value = '';
        searchClear.style.display = 'none';
        filterPlugins('');
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