// The short silent loops of the product pages (/rack/, /hub/): each plays only while it is on
// screen and stops when it leaves. With reduced motion they stay on their poster and get controls.
(function () {
  'use strict';
  var clips = document.querySelectorAll('video[data-clip]');
  if (!clips.length) return;
  var still = window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  if (still || !('IntersectionObserver' in window)) {
    clips.forEach(function (v) { v.controls = true; });
    return;
  }
  var seen = new IntersectionObserver(function (entries) {
    entries.forEach(function (e) {
      var v = e.target;
      if (e.isIntersecting) {
        if (v.preload === 'none') v.preload = 'auto';
        var p = v.play();
        if (p && p.catch) p.catch(function () { v.controls = true; });
      } else {
        v.pause();
      }
    });
  }, { threshold: 0.35 });
  clips.forEach(function (v) { seen.observe(v); });
})();
