// Live player of a plugin page: a loop of NULL JSFX instruments (assets/audio/loops/, made by
// tools/render_loops.js) plays through the plugin itself, run in the browser by the web version
// (web/runtime, web/fx: the same code as orOS Studio). Bypass, the loop and the factory preset
// can change while it plays. Nothing loads until Play is pressed.
(function () {
    'use strict';
    const script = document.currentScript;
    const site = new URL('../../', script ? script.src : location.href).href;
    const UID = 'p';
    let ctx = null, chain = null, src = null, out = null, owner = null, ready = null;
    const buffers = {};

    function addScript(url) {
        return new Promise(function (ok, fail) {
            const s = document.createElement('script');
            s.src = url;
            s.onload = ok;
            s.onerror = function () { fail(new Error('could not load ' + url)); };
            document.head.appendChild(s);
        });
    }

    function loop(name) {
        if (!buffers[name]) {
            buffers[name] = fetch(site + 'assets/audio/loops/' + name + '.mp3')
                .then(function (r) { if (!r.ok) throw new Error('loop ' + name); return r.arrayBuffer(); })
                .then(function (b) { return ctx.decodeAudioData(b); });
        }
        return buffers[name];
    }

    function data(el) {
        try { return JSON.parse(el.dataset.presets || '{}'); } catch (e) { return {}; }
    }

    /** the slider values of preset k: its own values over the defaults of every slider a preset sets */
    function values(el, k) {
        const d = data(el), v = Object.assign({}, d.d || {});
        const p = (d.p || [])[k];
        if (p) Object.assign(v, p[1]);
        return v;
    }

    function render() {
        document.querySelectorAll('[data-live]').forEach(function (el) {
            el.classList.toggle('demo-playing', el === owner);
            const ab = el.querySelector('.live-ab');
            el.classList.toggle('demo-bypass', ab.getAttribute('aria-pressed') === 'true');
        });
    }

    function status(el, text) {
        const s = el.querySelector('.live-status');
        if (s) s.textContent = text || '';
    }

    function stop() {
        if (src) { try { src.stop(); } catch (e) { /* not started */ } src.disconnect(); src = null; }
        if (chain) { chain.disconnect(); chain = null; }
        owner = null;
        render();
    }

    function start(el) {
        if (owner) stop();
        owner = el;
        render();
        status(el, 'Loading…');
        ctx = ctx || new (window.AudioContext || window.webkitAudioContext)();
        const id = el.dataset.live;
        ready = ready || addScript(site + 'web/runtime/null-node.js');
        const want = el;
        ready
            .then(function () { return Promise.all([window.NULLFXNode.load(ctx, site + 'web/', [id]), loop(el.querySelector('.live-loop').value)]); })
            .then(function (r) {
                if (owner !== want) return;
                const preset = +el.querySelector('.live-preset').value;
                const bypass = el.querySelector('.live-ab').getAttribute('aria-pressed') === 'true';
                chain = new window.NULLFXNode.Chain(ctx, { chain: [{ uid: UID, id: id, sliders: values(el, preset), bypass: bypass }],
                    time: { playing: true, tempo: 100, position: 0 } });
                out = out || ctx.createGain();
                out.connect(ctx.destination);
                chain.node.connect(out);
                src = ctx.createBufferSource();
                src.buffer = r[1];
                src.loop = true;
                src.connect(chain.node);
                return ctx.resume().then(function () { src.start(); status(el, ''); });
            })
            .catch(function (e) {
                if (owner === want) stop();
                status(el, 'This browser could not play it (' + e.message + ').');
            });
    }

    function setPreset(el) {
        if (owner !== el || !chain) return;
        const v = values(el, +el.querySelector('.live-preset').value);
        Object.keys(v).forEach(function (i) { chain.setSlider(UID, +i, v[i]); });
    }

    function setBypass(el) {
        const ab = el.querySelector('.live-ab');
        const on = ab.getAttribute('aria-pressed') !== 'true';
        ab.setAttribute('aria-pressed', String(on));
        if (owner === el && chain) chain.setBypass(UID, on);
        render();
    }

    document.addEventListener('click', function (e) {
        const b = e.target.closest('[data-live] button');
        if (!b) return;
        const el = b.closest('[data-live]');
        if (b.classList.contains('live-play')) { if (owner === el) stop(); else start(el); }
        else if (b.classList.contains('live-ab')) setBypass(el);
    });
    document.addEventListener('change', function (e) {
        const el = e.target.closest('[data-live]');
        if (!el) return;
        if (e.target.classList.contains('live-preset')) setPreset(el);
        else if (e.target.classList.contains('live-loop') && owner === el) start(el);
    });
    // one sound at a time: a video or the before/after demo starting stops the live player
    document.addEventListener('play', function () { if (owner) stop(); }, true);
    document.addEventListener('click', function (e) { if (owner && e.target.closest('.demo-play')) stop(); }, true);
})();
