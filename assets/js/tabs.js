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
        const search = document.getElementById('plugin-search');
        if (search && search.value.trim()) {
            search.value = '';
            search.dispatchEvent(new Event('input'));
        }
        show(b.dataset.tab, true);
    }));

    // searching looks through every plugin tab at once (search.js hides what does not match)
    const search = document.getElementById('plugin-search');
    if (search) {
        let before = null;
        search.addEventListener('input', () => {
            if (search.value.trim()) {
                if (before === null) before = current === 'chains' ? 'effects' : current;
                buttons.forEach(b => {
                    b.classList.remove('active');
                    b.setAttribute('aria-selected', 'false');
                });
                document.querySelectorAll('.tab-panel').forEach(p => { p.hidden = !p.classList.contains('tab-plugins'); });
            } else if (before !== null) {
                show(before, false);
                before = null;
            }
        });
        // the clear button (and Escape) empty the box without an input event
        const restore = () => { if (before !== null && !search.value.trim()) { show(before, false); before = null; } };
        const clear = window.clearSearch;
        if (clear) window.clearSearch = function() { clear(); restore(); };
        const clearButton = document.querySelector('.search-clear');
        if (clearButton) clearButton.addEventListener('click', restore);
        search.addEventListener('keydown', e => { if (e.key === 'Escape') setTimeout(restore); });
    }

    function fromHash() {
        const name = Object.keys(hashes).find(k => hashes[k] === location.hash);
        if (name) show(name, false);
    }
    window.addEventListener('hashchange', fromHash);
    fromHash();
})();
