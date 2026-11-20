// Plays the 4 s orbit of a plugin's 3D render (video.hw-orbit on the plugin pages) only while it
// is on screen: muted, looping, nothing loaded before it is first seen (preload="none"). With
// "reduce motion" set, it never starts by itself and shows its controls instead.
(function () {
    var videos = document.querySelectorAll("video.hw-orbit");
    if (!videos.length) return;
    var still = window.matchMedia && window.matchMedia("(prefers-reduced-motion: reduce)").matches;
    if (still || !("IntersectionObserver" in window)) {
        videos.forEach(function (v) { v.controls = true; });
        return;
    }
    var seen = new IntersectionObserver(function (entries) {
        entries.forEach(function (en) {
            var v = en.target;
            if (en.isIntersecting) {
                var p = v.play();
                if (p && p.catch) p.catch(function () { v.controls = true; });
            } else {
                v.pause();
            }
        });
    }, { threshold: 0.35 });
    videos.forEach(function (v) { seen.observe(v); });
})();
