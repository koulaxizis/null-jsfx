// Effects / FX Chains tabs in the plugin section. #fx-chains in the address opens the chains.
(function() {
    const buttons = document.querySelectorAll('.plugin-tab');
    if (!buttons.length) return;

    function show(name, updateHash) {
        buttons.forEach(b => {
            const on = b.dataset.tab === name;
            b.classList.toggle('active', on);
            b.setAttribute('aria-selected', on ? 'true' : 'false');
            document.getElementById('tab-' + b.dataset.tab).hidden = !on;
        });
        if (updateHash) {
            history.replaceState(null, '', name === 'chains' ? '#fx-chains' : location.pathname + location.search);
        }
    }

    buttons.forEach(b => b.addEventListener('click', () => show(b.dataset.tab, true)));

    // searching looks through the effects
    const search = document.getElementById('plugin-search');
    if (search) search.addEventListener('input', () => show('effects', false));

    if (location.hash === '#fx-chains') show('chains', false);
})();
