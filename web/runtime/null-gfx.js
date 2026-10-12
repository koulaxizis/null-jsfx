// Draws a translated JSFX @gfx section on a <canvas>, the way REAPER's LICE does (NULL JSFX,
// MIT License). The generated code calls G.line(), G.rect()... and keeps gfx_r, gfx_x and the
// other gfx_* / mouse_* variables in its own Float64Array; bind() hands this object that
// array and the slot of each variable, so drawing reads the colour and position straight
// from the effect's state.
// Pixel model: LICE puts pixel i at coordinate i, a canvas pixel i covers [i, i + 1), so
// drawing adds 0.5 to reach the same pixel centres.
// Plain ES5; registers globalThis.NULLGfx.
(function (g) {
  "use strict";

  var FONT_STACK = "\"Segoe UI\", Inter, system-ui, -apple-system, \"Helvetica Neue\", Arial, sans-serif";
  var DEFAULT_TEXTH = 8; // REAPER's built-in bitmap font

  function clamp01(v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

  // canvas: the element to draw on; opts.fontStack overrides the font list
  function Gfx(canvas, opts) {
    opts = opts || {};
    this.canvas = canvas;
    this.ctx = canvas.getContext("2d");
    this.fontStack = opts.fontStack || FONT_STACK;
    this.V = null; this.k = null; this.S = null;
    this.fonts = [];   // slot 1..16 -> { css, texth, asc }
    this.cur = null;   // active font (null = default)
    this.mw = 0; this.mh = 0; // gfx_measurestr results
    this.ascCache = {};
  }

  Gfx.prototype.bind = function (V, slots, S) {
    this.V = V; this.k = slots; this.S = S;
    V[slots.gfx_a] = 1;
    V[slots.gfx_texth] = DEFAULT_TEXTH;
    V[slots.gfx_clear] = -1;
  };

  // ---------------------------------------------------------------- state ---------------
  Gfx.prototype.paint = function () {
    var V = this.V, k = this.k, c = this.ctx;
    var a = clamp01(V[k.gfx_a]);
    var col = "rgb(" + Math.round(clamp01(V[k.gfx_r]) * 255) + "," + Math.round(clamp01(V[k.gfx_g]) * 255) + "," + Math.round(clamp01(V[k.gfx_b]) * 255) + ")";
    c.globalAlpha = a;
    c.globalCompositeOperation = (V[k.gfx_mode] | 0) & 1 ? "lighter" : "source-over";
    c.fillStyle = col;
    c.strokeStyle = col;
    return a > 0;
  };
  Gfx.prototype.str = function (h) { return this.S ? this.S.get(h) : ""; };

  // ---------------------------------------------------------------- shapes --------------
  // gfx_line(x, y, x2, y2[, aa]): LICE floors the end points and draws both ends
  Gfx.prototype.line = function (x1, y1, x2, y2) {
    if (!this.paint()) return 0;
    var c = this.ctx;
    x1 = Math.floor(x1) + 0.5; y1 = Math.floor(y1) + 0.5; x2 = Math.floor(x2) + 0.5; y2 = Math.floor(y2) + 0.5;
    if (x1 === x2 && y1 === y2) { c.fillRect(x1 - 0.5, y1 - 0.5, 1, 1); return 0; }
    c.lineWidth = 1;
    c.lineCap = "square";
    c.beginPath(); c.moveTo(x1, y1); c.lineTo(x2, y2); c.stroke();
    return 0;
  };
  // gfx_lineto(x, y[, aa]): from gfx_x, gfx_y, then moves there
  Gfx.prototype.lineto = function (x, y) {
    var V = this.V, k = this.k;
    this.line(V[k.gfx_x], V[k.gfx_y], x, y);
    V[k.gfx_x] = x; V[k.gfx_y] = y;
    return 0;
  };
  // gfx_rect(x, y, w, h[, filled = 1])
  Gfx.prototype.rect = function (x, y, w, h, filled) {
    x = Math.floor(x); y = Math.floor(y); w = Math.floor(w); h = Math.floor(h);
    if (w <= 0 || h <= 0 || !this.paint()) return 0;
    if (arguments.length < 5 || filled > 0.5) this.ctx.fillRect(x, y, w, h);
    else { this.ctx.lineWidth = 1; this.ctx.strokeRect(x + 0.5, y + 0.5, w - 1, h - 1); }
    return 0;
  };
  // gfx_roundrect(x, y, w, h, radius[, aa]): an outline from x to x + w
  Gfx.prototype.roundrect = function (x, y, w, h, r) {
    if (!(w > 0) || !(h > 0) || !this.paint()) return 0;
    var c = this.ctx;
    r = Math.max(0, Math.min(Math.floor(r) || 0, w / 2, h / 2));
    x += 0.5; y += 0.5;
    c.lineWidth = 1;
    c.beginPath();
    c.moveTo(x + r, y);
    c.lineTo(x + w - r, y); c.arcTo(x + w, y, x + w, y + r, r);
    c.lineTo(x + w, y + h - r); c.arcTo(x + w, y + h, x + w - r, y + h, r);
    c.lineTo(x + r, y + h); c.arcTo(x, y + h, x, y + h - r, r);
    c.lineTo(x, y + r); c.arcTo(x, y, x + r, y, r);
    c.closePath();
    c.stroke();
    return 0;
  };
  // gfx_circle(x, y, r[, fill = 0, aa])
  Gfx.prototype.circle = function (x, y, r, fill) {
    if (!(r >= 0) || !this.paint()) return 0;
    var c = this.ctx;
    c.beginPath();
    if (fill > 0.5) { c.arc(x + 0.5, y + 0.5, r + 0.5, 0, 2 * Math.PI); c.fill(); }
    else { c.lineWidth = 1; c.arc(x + 0.5, y + 0.5, r, 0, 2 * Math.PI); c.stroke(); }
    return 0;
  };
  // gfx_arc(x, y, r, ang1, ang2[, aa]): radians, 0 = up, clockwise
  Gfx.prototype.arc = function (x, y, r, a1, a2) {
    if (!(r >= 0) || !this.paint()) return 0;
    var c = this.ctx;
    c.lineWidth = 1;
    c.beginPath();
    c.arc(x + 0.5, y + 0.5, r, a1 - Math.PI / 2, a2 - Math.PI / 2);
    c.stroke();
    return 0;
  };
  // gfx_triangle(x1, y1, x2, y2, x3, y3[, x4, y4...]): a filled convex polygon
  Gfx.prototype.triangle = function () {
    var n = arguments.length & ~1;
    if (n < 6 || !this.paint()) return 0;
    var c = this.ctx;
    c.beginPath();
    for (var i = 0; i < n; i += 2) {
      var px = Math.floor(arguments[i]) + 0.5, py = Math.floor(arguments[i + 1]) + 0.5;
      if (i) c.lineTo(px, py); else c.moveTo(px, py);
    }
    c.closePath();
    c.fill();
    c.lineWidth = 1;
    c.stroke();
    return 0;
  };
  // gfx_setpixel(r, g, b) at gfx_x, gfx_y
  Gfx.prototype.setpixel = function (r, gg, b) {
    var V = this.V, k = this.k, c = this.ctx;
    c.globalAlpha = 1;
    c.globalCompositeOperation = "source-over";
    c.fillStyle = "rgb(" + Math.round(clamp01(r) * 255) + "," + Math.round(clamp01(gg) * 255) + "," + Math.round(clamp01(b) * 255) + ")";
    c.fillRect(Math.floor(V[k.gfx_x]), Math.floor(V[k.gfx_y]), 1, 1);
    return 0;
  };
  // gfx_gradrect(x, y, w, h, r, g, b, a[, drdx, dgdx, dbdx, dadx, drdy, dgdy, dbdy, dady]):
  // colour changes per pixel; drawn row by row as horizontal gradients
  Gfx.prototype.gradrect = function (x, y, w, h, r, gg, b, a) {
    x = Math.floor(x); y = Math.floor(y); w = Math.floor(w); h = Math.floor(h);
    if (w <= 0 || h <= 0) return 0;
    var d = [];
    for (var i = 8; i < 16; i++) d.push(arguments.length > i ? arguments[i] : 0);
    var c = this.ctx, V = this.V, k = this.k;
    c.globalCompositeOperation = (V[k.gfx_mode] | 0) & 1 ? "lighter" : "source-over";
    c.globalAlpha = 1;
    function css(rr, g2, bb, aa) {
      return "rgba(" + Math.round(clamp01(rr) * 255) + "," + Math.round(clamp01(g2) * 255) + "," + Math.round(clamp01(bb) * 255) + "," + clamp01(aa) + ")";
    }
    var flatY = !d[4] && !d[5] && !d[6] && !d[7];
    var rows = flatY ? 1 : h;
    for (var row = 0; row < rows; row++) {
      var r0 = r + d[4] * row, g0 = gg + d[5] * row, b0 = b + d[6] * row, a0 = a + d[7] * row;
      var grad = c.createLinearGradient(x, 0, x + w, 0);
      grad.addColorStop(0, css(r0, g0, b0, a0));
      grad.addColorStop(1, css(r0 + d[0] * w, g0 + d[1] * w, b0 + d[2] * w, a0 + d[3] * w));
      c.fillStyle = grad;
      c.fillRect(x, y + row, w, flatY ? h : 1);
    }
    return 0;
  };
  // gfx_set(r[, g, b, a = 1, mode = 0])
  Gfx.prototype.set = function (r, gg, b, a, mode) {
    var V = this.V, k = this.k, n = arguments.length;
    V[k.gfx_r] = r;
    V[k.gfx_g] = n > 1 ? gg : r;
    V[k.gfx_b] = n > 2 ? b : r;
    V[k.gfx_a] = n > 3 ? a : 1;
    V[k.gfx_mode] = n > 4 ? mode : 0;
    return 0;
  };

  // ---------------------------------------------------------------- text ----------------
  // gfx_setfont(idx[, "face", size, flags]): idx 0 = the default font; 1..16 configurable.
  // Matches ysfx (the NULL JSFX reference shots): size is the em size in pixels, gfx_texth
  // becomes that size and text hangs from its ascent line.
  Gfx.prototype.setfont = function (idx, face, size, flags) {
    var V = this.V, k = this.k, a = Math.floor(idx);
    if (a >= 1 && a <= 16 && arguments.length > 1) {
      var name = this.str(face) || "Arial", sz = arguments.length > 2 ? Math.trunc(size) : 10, fl = arguments.length > 3 ? flags >>> 0 : 0;
      var bold = false, italic = false;
      while (fl) { var ch = String.fromCharCode(fl & 255).toUpperCase(); if (ch === "B") bold = true; if (ch === "I") italic = true; fl >>>= 8; }
      this.fonts[a] = this.makeFont(name, Math.max(1, Math.abs(sz)), bold, italic);
    }
    if (a >= 1 && a <= 16 && this.fonts[a]) {
      this.cur = this.fonts[a];
      V[k.gfx_texth] = this.cur.texth;
    } else {
      this.cur = null;
      V[k.gfx_texth] = DEFAULT_TEXTH;
    }
    return 1;
  };
  Gfx.prototype.makeFont = function (name, px, bold, italic) {
    var c = this.ctx, family = "\"" + name.replace(/["\\]/g, "") + "\", " + this.fontStack;
    var style = (italic ? "italic " : "") + (bold ? "bold " : "");
    var key = style + family, ratio = this.ascCache[key];
    if (!ratio) {
      c.font = style + "100px " + family;
      var m = c.measureText("Hg"), asc = m.fontBoundingBoxAscent, desc = m.fontBoundingBoxDescent;
      if (!(asc > 0)) { asc = 97; desc = 24; } // older browsers: a typical sans
      ratio = this.ascCache[key] = { asc: asc / 100, desc: desc / 100 };
    }
    return { css: style + px + "px " + family, texth: px, asc: Math.ceil(px * ratio.asc) };
  };
  Gfx.prototype.useFont = function () {
    var f = this.cur || (this.defFont || (this.defFont = this.makeFont("Arial", DEFAULT_TEXTH, false, false)));
    this.ctx.font = f.css;
    return f;
  };
  Gfx.prototype.measureText = function (s) {
    var f = this.useFont(), lines = s.split("\n"), w = 0;
    for (var i = 0; i < lines.length; i++) w = Math.max(w, this.ctx.measureText(lines[i]).width);
    return { w: Math.round(w), h: lines.length * this.V[this.k.gfx_texth], f: f, lines: lines };
  };
  Gfx.prototype.measurestr = function (h) {
    var m = this.measureText(this.str(h));
    this.mw = m.w; this.mh = m.h;
    return 1;
  };
  Gfx.prototype.measurechar = function (ch) {
    var m = this.measureText(String.fromCharCode(Math.floor(ch)));
    this.mw = m.w; this.mh = m.h;
    return 1;
  };
  // gfx_drawstr(str[, flags, right, bottom]): at gfx_x, gfx_y; flags (with right/bottom):
  // 1 centre, 2 right, 4 middle, 8 bottom. Leaves gfx_x after the text.
  Gfx.prototype.drawText = function (s, flags, right, bottom, n) {
    var V = this.V, k = this.k, c = this.ctx;
    var m = this.measureText(s), th = V[k.gfx_texth];
    var x = V[k.gfx_x], y = V[k.gfx_y];
    if (n >= 3) {
      flags = flags | 0;
      if (flags & 1) x = (x + right - m.w) / 2; else if (flags & 2) x = right - m.w;
      if (n >= 4) { if (flags & 4) y = (y + bottom - m.h) / 2; else if (flags & 8) y = bottom - m.h; }
    }
    if (this.paint()) {
      c.textBaseline = "alphabetic";
      c.textAlign = "left";
      for (var i = 0; i < m.lines.length; i++) c.fillText(m.lines[i], Math.floor(x), Math.floor(y) + i * th + m.f.asc);
    }
    var last = m.lines[m.lines.length - 1];
    V[k.gfx_x] = x + (m.lines.length > 1 ? c.measureText(last).width : m.w);
    V[k.gfx_y] = y + (m.lines.length - 1) * th;
    return 0;
  };
  Gfx.prototype.drawstr = function (h, flags, right, bottom) {
    return this.drawText(this.str(h), flags, right, bottom, arguments.length);
  };
  Gfx.prototype.drawchar = function (ch) {
    return this.drawText(String.fromCharCode(Math.floor(ch)), 0, 0, 0, 1);
  };
  Gfx.prototype.drawnumber = function (v, digits) {
    return this.drawText(v.toFixed(Math.max(0, Math.min(20, Math.floor(digits)))), 0, 0, 0, 1);
  };

  // ---------------------------------------------------------------- input (not yet) -----
  Gfx.prototype.getchar = function () { return 0; };
  Gfx.prototype.showmenu = function () { return 0; };
  Gfx.prototype.setcursor = function () { return 0; };
  Gfx.prototype.muladdrect = function () { return 0; };

  // ---------------------------------------------------------------- frame ---------------
  // Runs one @gfx frame. size: { w, h } in CSS pixels; scale: device pixels per CSS pixel;
  // mouse: { x, y (CSS pixels), cap, wheel (notches since the last frame) }.
  Gfx.prototype.frame = function (inst, size, scale, mouse) {
    var V = this.V, k = this.k, cv = this.canvas;
    var retina = V[k.gfx_ext_retina] > 0;
    var s = retina ? (scale || 1) : 1;
    var w = Math.max(1, Math.round(size.w * s)), h = Math.max(1, Math.round(size.h * s));
    if (cv.width !== w) cv.width = w;
    if (cv.height !== h) cv.height = h;
    if (retina) V[k.gfx_ext_retina] = s;
    V[k.gfx_w] = w; V[k.gfx_h] = h;
    if (mouse) {
      V[k.mouse_x] = Math.floor(mouse.x * s);
      V[k.mouse_y] = Math.floor(mouse.y * s);
      V[k.mouse_cap] = mouse.cap | 0;
      V[k.mouse_wheel] += (mouse.wheel || 0) * 120;
    }
    var c = this.ctx;
    c.setTransform(1, 0, 0, 1, 0, 0);
    var clr = V[k.gfx_clear];
    if (clr > -1) {
      var n = clr | 0;
      c.globalAlpha = 1; c.globalCompositeOperation = "source-over";
      c.fillStyle = "rgb(" + (n & 255) + "," + ((n >> 8) & 255) + "," + ((n >> 16) & 255) + ")";
      c.fillRect(0, 0, w, h);
    }
    V[k.gfx_x] = 0; V[k.gfx_y] = 0;
    inst.gfx();
    return 0;
  };

  var API = { Gfx: Gfx };
  g.NULLGfx = API;
  if (typeof module !== "undefined" && module.exports) module.exports = API;
})(typeof globalThis !== "undefined" ? globalThis : this);
