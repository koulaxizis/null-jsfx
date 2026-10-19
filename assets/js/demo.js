// Before/after audio demos: assets/audio/demos/<plugin>.mp3 is the music through the plugin and
// dry.mp3 the same music without it (tools/render_demos.py). Both play together and the switch
// only unmutes one, so they stay in step. One demo plays at a time; a video starting stops it.
(function () {
    const script = document.currentScript;
    const base = new URL('../audio/demos/', script ? script.src : location.href).href;
    const wet = new Audio(), dry = new Audio();
    wet.preload = dry.preload = 'none';
    let owner = null, bypass = false;

    function render() {
        document.querySelectorAll('[data-demo]').forEach(function (el) {
            const on = el === owner;
            el.classList.toggle('demo-playing', on);
            el.classList.toggle('demo-bypass', on && bypass);
        });
    }

    function stop() {
        wet.pause();
        dry.pause();
        owner = null;
        render();
    }

    function play(el) {
        if (owner === el) return stop();
        stop();
        wet.src = base + el.dataset.demo + '.mp3';
        dry.src = base + 'dry.mp3';
        owner = el;
        bypass = false;
        wet.muted = false;
        dry.muted = true;
        render();
        Promise.all([wet.play(), dry.play()]).catch(stop);
    }

    function toggle(el) {
        if (owner !== el) return play(el);
        bypass = !bypass;
        wet.muted = bypass;
        dry.muted = !bypass;
        render();
    }

    wet.addEventListener('ended', stop);
    // keep the dry copy in step if the wet one stalled while loading
    wet.addEventListener('playing', function () {
        if (Math.abs(dry.currentTime - wet.currentTime) > 0.05) dry.currentTime = wet.currentTime;
    });

    // capture phase, so a click on the demo buttons inside a card does not open the card's guide
    document.addEventListener('click', function (e) {
        const button = e.target.closest('.demo-play, .demo-ab');
        if (!button) return;
        e.preventDefault();
        e.stopPropagation();
        const el = button.closest('[data-demo]');
        if (button.classList.contains('demo-play')) play(el); else toggle(el);
    }, true);
    document.addEventListener('play', function (e) {
        if (e.target.tagName === 'VIDEO') stop();
    }, true);

    window.stopDemo = stop;
})();
