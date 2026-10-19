// Effects / Instruments / MIDI / FX Chains tabs in the plugin section.
// #instruments, #midi and #fx-chains in the address open their tab.
(function() {
    const buttons = document.querySelectorAll('.plugin-tab');
    if (!buttons.length) return;

    const hashes = { instruments: '#instruments', midi: '#midi', chains: '#fx-chains' };
    let current = 'effects';

    function show(name, updateHash) {
        current = name;
        buttons.forEach(b => {
            const on = b.dataset.tab === name;
            b.classList.toggle('active', on);
            b.setAttribute('aria-selected', on ? 'true' : 'false');
            document.getElementById('tab-' + b.dataset.tab).hidden = !on;
        });
        if (updateHash) {
            history.replaceState(null, '', hashes[name] || location.pathname + location.search);
        }
    }

    buttons.forEach(b => b.addEventListener('click', () => {
        // a tab shows its own plugins: drop the search and the use filter
        const search = document.getElementById('plugin-search');
        if (search && search.value.trim()) {
            search.value = '';
            search.dispatchEvent(new Event('input'));
        }
        if (window.setPluginTag) window.setPluginTag('');
        show(b.dataset.tab, true);
    }));

    // while a search or a use filter is on, every plugin tab shows at once (search.js hides what
    // does not match); when both are off, the tab that was open comes back
    let before = null;
    document.addEventListener('plugin-filter', e => {
        if (e.detail.active) {
            if (before === null) before = current === 'chains' ? 'effects' : current;
            buttons.forEach(b => {
                b.classList.remove('active');
                b.setAttribute('aria-selected', 'false');
            });
            document.querySelectorAll('.tab-panel').forEach(p => { p.hidden = !p.classList.contains('tab-plugins'); });
        } else if (before !== null) {
            const tab = before;
            before = null;
            show(tab, false);
        }
    });

    function fromHash() {
        const name = Object.keys(hashes).find(k => hashes[k] === location.hash);
        if (name) show(name, false);
    }
    window.addEventListener('hashchange', fromHash);
    fromHash();
})();
