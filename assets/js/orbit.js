// The 4 s orbit of a 3D render (video.hw-orbit inside .hw-stage, on the plugin, Rack and Hub pages):
// it plays once when it first comes on screen, then fades out onto the still behind it (the front view,
// the same picture as the poster and the og image). Hover or a tap plays it again. Nothing loads before
// it is first seen (preload="none"); it pauses when scrolled away. With "reduce motion" set it never
// starts by itself: a tap plays it. Christos, 2026-11-21: no loop.
(function () {
    var videos = document.querySelectorAll("video.hw-orbit");
    if (!videos.length) return;
    var still = window.matchMedia && window.matchMedia("(prefers-reduced-motion: reduce)").matches;

    function play(v) {
        var stage = v.parentNode;
        stage.classList.remove("hw-rest");
        if (v.ended || v.currentTime > 0) v.currentTime = 0;
        var p = v.play();
        if (p && p.catch) p.catch(function () { stage.classList.add("hw-rest"); });
    }

    videos.forEach(function (v) {
        var stage = v.parentNode;
        v.loop = false;
        v.addEventListener("ended", function () { stage.classList.add("hw-rest"); });
        stage.addEventListener("mouseenter", function () { if (v.paused) play(v); });
        stage.addEventListener("click", function () { if (v.paused) play(v); });
        if (still) stage.classList.add("hw-rest");
    });
    if (still || !("IntersectionObserver" in window)) return;

    var seen = new IntersectionObserver(function (entries) {
        entries.forEach(function (en) {
            var v = en.target;
            if (en.isIntersecting && !v.dataset.played) {
                v.dataset.played = "1";
                play(v);
            } else if (!en.isIntersecting && !v.paused) {
                v.pause();
                v.parentNode.classList.add("hw-rest");
            }
        });
    }, { threshold: 0.35 });
    videos.forEach(function (v) { seen.observe(v); });
})();
