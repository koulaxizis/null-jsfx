// The chain window of an FX chain page (chains/<slug>/, tools/chain_pages.py): a click on a
// plugin in the list shows its picture (assets/img/chains/<slug>/<n>.webp), as REAPER shows the
// plugin picked in its FX chain window. The arrow keys move through the list.
(function () {
    'use strict';
    function pick(win, b) {
        win.querySelectorAll('.cw-item').forEach(function (x) {
            const on = x === b;
            x.classList.toggle('on', on);
            x.setAttribute('aria-pressed', String(on));
        });
        const img = win.querySelector('.cw-shot');
        img.width = +b.dataset.w;
        img.height = +b.dataset.h;
        img.alt = b.dataset.alt;
        img.src = '/assets/img/chains/' + win.dataset.slug + '/' + b.dataset.shot + '.webp';
    }
    document.addEventListener('click', function (e) {
        const b = e.target.closest('.chain-window .cw-item');
        if (b) pick(b.closest('.chain-window'), b);
    });
    document.addEventListener('keydown', function (e) {
        const b = e.target.closest && e.target.closest('.chain-window .cw-item');
        if (!b || (e.key !== 'ArrowDown' && e.key !== 'ArrowUp')) return;
        const all = Array.from(b.closest('.cw-list').querySelectorAll('.cw-item'));
        const next = all[all.indexOf(b) + (e.key === 'ArrowDown' ? 1 : -1)];
        if (!next) return;
        e.preventDefault();
        next.focus();
        pick(b.closest('.chain-window'), next);
    });
})();
