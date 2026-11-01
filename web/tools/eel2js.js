// EEL2 / JSFX -> JavaScript translator for NULL JSFX (https://nulljsfx.tech, MIT License).
//
// Turns one .jsfx file into a plain JavaScript module that registers itself on
// globalThis.NULLFX[id]. The output runs with web/runtime/eel-rt.js in an AudioWorklet,
// in Node (tests) or on a page (the @gfx part). Nothing is evaluated at runtime: the
// generated code is committed and read like any other script.
//
// The translation follows EEL2 as REAPER and ysfx run it (checked by web/test/):
//  - the grammar's odd shapes are kept: a + b - c is a + (b - c), a * b / c is a * (b / c);
//  - == != && || ! ?: use EEL2's 0.00001 closeness, === !== are exact;
//  - % << >> | & ~ work on integers (% on absolute values, x % 0 = 0);
//  - memory: index = trunc(x + 0.00001), out-of-range reads and writes share one slot;
//  - every variable starts at 0, function locals keep their value between calls;
//  - an assignment from a computed value turns denormals, inf and NaN into 0;
//  - functions with namespaces (this., instance()) are resolved per call site.
"use strict";

var fs = require("fs");
var path = require("path");

// ---------------------------------------------------------------- JSFX file ----------

function parseSliderLine(n, rest) {
  // sliderN:[var=]default<min,max,inc[{a,b}][:log[=centre]]>[-]Name   or   sliderN:/folder:default:Name
  var s = { index: n, name: "", def: 0, min: 0, max: 1, inc: 0, enums: null, shape: "lin", hidden: false, path: null, varName: null };
  var m = /^([A-Za-z_][A-Za-z0-9_.]*)=(.*)$/.exec(rest);
  if (m && !/^\//.test(rest)) { s.varName = m[1]; rest = m[2]; }
  if (rest.charAt(0) === "/") {
    m = /^(\/[^:]*):([^:]*):(.*)$/.exec(rest);
    if (!m) throw new Error("slider" + n + ": bad file slider");
    s.path = m[1]; s.defFile = m[2]; s.def = 0; s.min = 0; s.max = 0; s.inc = 1;
    rest = m[3];
  } else {
    m = /^([-+0-9.]*)\s*<([^>]*)>(.*)$/.exec(rest);
    if (!m) {
      m = /^([-+0-9.]*)\s*(.*)$/.exec(rest);
      s.def = parseFloat(m[1]) || 0; rest = m[2];
    } else {
      s.def = parseFloat(m[1]) || 0;
      var spec = m[2]; rest = m[3];
      var en = /\{([^}]*)\}/.exec(spec);
      if (en) { s.enums = en[1].split(","); spec = spec.replace(en[0], ""); }
      var shape = /:(log|sqr)(?:=([-+0-9.]+))?/.exec(spec);
      if (shape) { s.shape = shape[1]; if (shape[2]) s.shapeArg = parseFloat(shape[2]); spec = spec.replace(shape[0], ""); }
      var p = spec.split(",");
      s.min = parseFloat(p[0]) || 0;
      s.max = p.length > 1 ? parseFloat(p[1]) || 0 : 0;
      s.inc = p.length > 2 ? parseFloat(p[2]) || 0 : 0;
    }
  }
  rest = rest.trim();
  if (rest.charAt(0) === "-") { s.hidden = true; rest = rest.slice(1); }
  s.name = rest.trim();
  return s;
}

// Reads a .jsfx (and its imports) into { header, sections: {init, slider, block, sample, gfx}, imports }.
function readJsfx(file, seen) {
  seen = seen || {};
  var abs = path.resolve(file);
  if (seen[abs]) return null;
  seen[abs] = true;
  var text = fs.readFileSync(abs, "utf8").replace(/\r\n?/g, "\n");
  var lines = text.split("\n");
  var hdr = { desc: "", author: "", tags: "", category: "", sliders: [], inPins: [], outPins: [], options: "", gfx: null };
  var sections = { init: [], slider: [], block: [], sample: [], gfx: [], serialize: [] };
  var imports = [];
  var cur = null;
  for (var i = 0; i < lines.length; i++) {
    var line = lines[i];
    var sm = /^@(\w+)(.*)$/.exec(line);
    if (sm) {
      cur = sm[1];
      if (!sections[cur]) throw new Error(file + ": unknown section @" + cur);
      if (cur === "gfx") {
        var dims = sm[2].trim().split(/\s+/).map(Number);
        hdr.gfx = { w: dims[0] || 0, h: dims[1] || 0 };
      }
      // keep line numbers: pad with the line's offset
      sections[cur].push({ line: i + 1, text: "" });
      continue;
    }
    if (cur) { sections[cur][sections[cur].length - 1].text += line + "\n"; continue; }
    var m;
    if ((m = /^desc:\s*(.*)$/.exec(line))) hdr.desc = m[1].trim();
    else if ((m = /^author:\s*(.*)$/.exec(line))) hdr.author = m[1].trim();
    else if ((m = /^tags:\s*(.*)$/.exec(line))) hdr.tags = m[1].trim();
    else if ((m = /^\/\/category:\s*(.*)$/.exec(line))) hdr.category = m[1].trim();
    else if ((m = /^slider(\d+):(.*)$/.exec(line))) hdr.sliders.push(parseSliderLine(+m[1], m[2].trim()));
    else if ((m = /^in_pin:(.*)$/.exec(line))) { if (m[1].trim() !== "none") hdr.inPins.push(m[1].trim()); }
    else if ((m = /^out_pin:(.*)$/.exec(line))) { if (m[1].trim() !== "none") hdr.outPins.push(m[1].trim()); }
    else if ((m = /^options:(.*)$/.exec(line))) hdr.options = m[1].trim();
    else if ((m = /^import\s+(.*)$/.exec(line))) imports.push(m[1].trim());
  }
  var out = { file: abs, header: hdr, sections: sections, imported: [] };
  imports.forEach(function (imp) {
    var r = readJsfx(path.join(path.dirname(abs), imp), seen);
    if (r) out.imported.push(r);
  });
  return out;
}

// ---------------------------------------------------------------- lexer ---------------

function Lexer(src, where) {
  this.src = src; this.pos = 0; this.where = where || ""; this.tok = null;
  this.lineStarts = null;
}
Lexer.prototype.fail = function (msg, pos) {
  var p = pos == null ? this.pos : pos, line = 1, col = 1;
  for (var i = 0; i < p && i < this.src.length; i++) { if (this.src.charAt(i) === "\n") { line++; col = 1; } else col++; }
  throw new Error(this.where + ":" + line + ":" + col + ": " + msg);
};
Lexer.prototype.skip = function () {
  var s = this.src;
  for (;;) {
    var c = s.charAt(this.pos);
    if (c === " " || c === "\t" || c === "\n" || c === "\r") { this.pos++; continue; }
    if (c === "/" && s.charAt(this.pos + 1) === "/") { while (this.pos < s.length && s.charAt(this.pos) !== "\n") this.pos++; continue; }
    if (c === "/" && s.charAt(this.pos + 1) === "*") {
      var e = s.indexOf("*/", this.pos + 2);
      this.pos = e < 0 ? s.length : e + 2; continue;
    }
    return;
  }
};
// token: {k: 'num'|'id'|'sid'|'str'|'op'|'eof', v, pos}
Lexer.prototype.next = function () {
  this.skip();
  var s = this.src, p = this.pos, c = s.charAt(p), m;
  if (p >= s.length) return (this.tok = { k: "eof", v: "", pos: p });
  var rest = s.slice(p, p + 400);
  if ((m = /^(?:0[xX]|\$[xX])([0-9a-fA-F]+)/.exec(rest))) { this.pos += m[0].length; return (this.tok = { k: "num", v: parseInt(m[1], 16), pos: p }); }
  if ((m = /^\$'(\\?.)'/.exec(rest))) { this.pos += m[0].length; return (this.tok = { k: "num", v: unescapeChar(m[1]).charCodeAt(0), pos: p }); }
  if ((m = /^\$(pi|e|phi)\b/i.exec(rest))) {
    this.pos += m[0].length;
    var k = m[1].toLowerCase();
    return (this.tok = { k: "num", v: k === "pi" ? Math.PI : k === "e" ? Math.E : 1.618033988749894848, pos: p });
  }
  if ((m = /^(?:[0-9]+\.?[0-9]*|\.[0-9]+)/.exec(rest))) {
    if (/^[A-Za-z_]/.test(rest.slice(m[0].length))) this.fail("bad number");
    this.pos += m[0].length; return (this.tok = { k: "num", v: parseFloat(m[0]), pos: p });
  }
  if ((m = /^[A-Za-z_][A-Za-z0-9_.]*/.exec(rest))) { this.pos += m[0].length; return (this.tok = { k: "id", v: m[0], pos: p }); }
  if ((m = /^#[A-Za-z0-9_.]*/.exec(rest))) { this.pos += m[0].length; return (this.tok = { k: "sid", v: m[0], pos: p }); }
  if (c === "'" || c === '"') {
    var i = p + 1, txt = "";
    while (i < s.length && s.charAt(i) !== c) {
      if (s.charAt(i) === "\\" && i + 1 < s.length) { txt += unescapeChar("\\" + s.charAt(i + 1)); i += 2; }
      else txt += s.charAt(i++);
    }
    if (i >= s.length) this.fail("unterminated string", p);
    this.pos = i + 1;
    if (c === "'") {
      var v = 0;
      for (var j = 0; j < txt.length; j++) v = v * 256 + (txt.charCodeAt(j) & 255);
      return (this.tok = { k: "num", v: v, pos: p });
    }
    return (this.tok = { k: "str", v: txt, pos: p });
  }
  var ops = ["===", "!==", "==", "!=", "<=", ">=", "<<", ">>", "&&", "||", "+=", "-=", "*=", "/=", "%=", "|=", "&=", "~=", "^="];
  for (var o = 0; o < ops.length; o++) {
    if (s.substr(p, ops[o].length) === ops[o]) { this.pos += ops[o].length; return (this.tok = { k: "op", v: ops[o], pos: p }); }
  }
  this.pos++;
  return (this.tok = { k: "op", v: c, pos: p });
};

function unescapeChar(e) {
  if (e.length < 2 || e.charAt(0) !== "\\") return e;
  var c = e.charAt(1);
  return c === "n" ? "\n" : c === "t" ? "\t" : c === "r" ? "\r" : c === "0" ? "\0" : c;
}

// ---------------------------------------------------------------- parser --------------
// AST: num{v} var{name} sid{name} str{v} call{name,args} mem{base,idx,g} un{op,a}
//      bin{op,a,b} asg{op,target,value} if{c,a,b} seq{items} fndef

function Parser(src, where) {
  this.lx = new Lexer(src, where);
  this.lx.next();
  this.functions = [];
}
Parser.prototype.peek = function () { return this.lx.tok; };
Parser.prototype.isOp = function (v) { var t = this.lx.tok; return t.k === "op" && t.v === v; };
Parser.prototype.eat = function (v) {
  if (!this.isOp(v)) this.lx.fail("expected '" + v + "' but found '" + this.lx.tok.v + "'", this.lx.tok.pos);
  this.lx.next();
};
Parser.prototype.program = function () {
  var e = this.expression(true);
  if (this.peek().k !== "eof") this.lx.fail("unexpected '" + this.peek().v + "'", this.peek().pos);
  return e || { t: "num", v: 0 };
};
// expression: if_else (';' if_else?)* ; at top level 'function' definitions may appear as statements
Parser.prototype.expression = function (top) {
  var items = [];
  for (;;) {
    var t = this.peek();
    if (t.k === "eof" || (t.k === "op" && (t.v === ")" || t.v === "," || t.v === "]"))) break;
    if (t.k === "op" && t.v === ";") { this.lx.next(); continue; }
    if (t.k === "id" && t.v === "function") {
      if (!top) this.lx.fail("function definition inside an expression", t.pos);
      this.functions.push(this.fndef());
      continue;
    }
    items.push(this.ifElse());
    if (!this.isOp(";")) break;
  }
  if (!items.length) return null;
  return items.length === 1 ? items[0] : { t: "seq", items: items };
};
Parser.prototype.fndef = function () {
  var pos = this.peek().pos;
  this.lx.next(); // function
  var nt = this.peek();
  if (nt.k !== "id") this.lx.fail("function name expected", nt.pos);
  var fn = { name: nt.v, params: [], local: [], instance: [], globals: [], body: null, pos: pos };
  this.lx.next();
  this.eat("(");
  while (!this.isOp(")")) {
    var p = this.peek();
    if (p.k !== "id") this.lx.fail("parameter name expected", p.pos);
    fn.params.push(p.v);
    this.lx.next();
    if (this.isOp("*")) { this.lx.next(); fn.refs = fn.refs || {}; fn.refs[p.v.toLowerCase()] = true; } // pass by reference
    if (this.isOp(",")) this.lx.next();
  }
  this.eat(")");
  for (;;) {
    var k = this.peek();
    if (k.k === "id" && /^(local|instance|globals|global)$/.test(k.v)) {
      var list = k.v === "local" ? fn.local : k.v === "instance" ? fn.instance : fn.globals;
      this.lx.next();
      this.eat("(");
      while (!this.isOp(")")) {
        var q = this.peek();
        if (q.k !== "id" && q.k !== "sid") this.lx.fail("name expected", q.pos);
        list.push(q.v);
        this.lx.next();
        if (this.isOp(",")) this.lx.next();
      }
      this.eat(")");
      continue;
    }
    break;
  }
  this.eat("(");
  fn.body = this.expression(false) || { t: "num", v: 0 };
  this.eat(")");
  return fn;
};
Parser.prototype.ifElse = function () {
  var c = this.logical();
  if (!this.isOp("?")) return c;
  this.lx.next();
  var a = null, b = null;
  if (this.isOp(":")) { this.lx.next(); b = this.ifElse(); }
  else {
    a = this.ifElse();
    if (this.isOp(":")) { this.lx.next(); b = this.ifElse(); }
  }
  return { t: "if", c: c, a: a, b: b };
};
Parser.prototype.binLevel = function (ops, sub) {
  var a = sub.call(this);
  for (;;) {
    var t = this.peek();
    if (t.k !== "op" || ops.indexOf(t.v) < 0) return a;
    this.lx.next();
    a = { t: "bin", op: t.v, a: a, b: sub.call(this) };
  }
};
Parser.prototype.logical = function () { return this.binLevel(["&&", "||"], this.cmp); };
Parser.prototype.cmp = function () { return this.binLevel(["<", ">", "<=", ">=", "==", "===", "!=", "!=="], this.andor); };
Parser.prototype.andor = function () { return this.binLevel(["&", "|", "~"], this.add); };
Parser.prototype.add = function () { return this.binLevel(["+"], this.sub); };
Parser.prototype.sub = function () { return this.binLevel(["-"], this.mul); };
Parser.prototype.mul = function () { return this.binLevel(["*"], this.div); };
Parser.prototype.div = function () { return this.binLevel(["/"], this.mod); };
Parser.prototype.mod = function () { return this.binLevel(["%", "<<", ">>"], this.pow); };
Parser.prototype.pow = function () { return this.binLevel(["^"], this.unary); };
Parser.prototype.unary = function () {
  var t = this.peek();
  if (t.k === "op" && (t.v === "-" || t.v === "+" || t.v === "!")) {
    this.lx.next();
    var a = this.unary();
    return t.v === "+" ? a : { t: "un", op: t.v, a: a };
  }
  return this.assignment();
};
var ASG_OPS = ["=", "+=", "-=", "*=", "/=", "%=", "|=", "&=", "~=", "^="];
Parser.prototype.assignment = function () {
  var target = this.primary();
  var t = this.peek();
  if (t.k === "op" && ASG_OPS.indexOf(t.v) >= 0) {
    if (!target.assignable) this.lx.fail("cannot assign to this", t.pos);
    this.lx.next();
    var value = this.ifElse();
    if (target.t === "sid" || target.t === "str") {
      if (t.v === "=") return { t: "call", name: "strcpy", args: [target, value] };
      if (t.v === "+=") return { t: "call", name: "strcat", args: [target, value] };
      this.lx.fail("bad string assignment", t.pos);
    }
    return { t: "asg", op: t.v, target: target, value: value };
  }
  return target;
};
Parser.prototype.args = function () {
  var list = [];
  if (this.isOp(")")) { this.lx.next(); return list; }
  for (;;) {
    var e = this.expression(false);
    list.push(e || { t: "num", v: 0 });
    if (this.isOp(",")) { this.lx.next(); continue; }
    this.eat(")");
    return list;
  }
};
Parser.prototype.primary = function () {
  var t = this.peek(), node;
  if (t.k === "num") { this.lx.next(); node = { t: "num", v: t.v }; }
  else if (t.k === "str") {
    this.lx.next();
    var v = t.v;
    while (this.peek().k === "str") { v += this.peek().v; this.lx.next(); } // "a" "b" joins
    node = { t: "str", v: v };
  } else if (t.k === "sid") { this.lx.next(); node = { t: "sid", name: t.v, assignable: true }; }
  else if (t.k === "id") {
    this.lx.next();
    if (this.isOp("(")) {
      this.lx.next();
      var args = this.args();
      if (this.isOp("(") && (t.v === "while" || t.v === "loop")) { // loop(n)(body), while(cond)(body)
        this.lx.next();
        args.push(this.expression(false) || { t: "num", v: 0 });
        this.eat(")");
      }
      node = { t: "call", name: t.v, args: args, pos: t.pos, assignable: true };
    } else node = { t: "var", name: t.v, pos: t.pos, assignable: true };
  } else if (t.k === "op" && t.v === "(") {
    this.lx.next();
    var e = this.expression(false) || { t: "num", v: 0 };
    this.eat(")");
    node = e;
    node.assignable = true;
  } else this.lx.fail("unexpected '" + (t.v || "end of code") + "'", t.pos);
  while (this.isOp("[")) {
    this.lx.next();
    var idx = this.isOp("]") ? null : this.expression(false);
    this.eat("]");
    node = { t: "mem", base: node, idx: idx, assignable: true };
  }
  return node;
};

// ---------------------------------------------------------------- compiler -------------

var RESERVED_HOST = ["srate", "samplesblock", "num_ch", "tempo", "play_state", "play_position", "beat_position",
  "ts_num", "ts_denom", "pdc_delay", "pdc_bot_ch", "pdc_top_ch", "pdc_midi", "trigger", "ext_noinit",
  "ext_nodenorm", "ext_tail_size", "ext_gr_meter", "gfx_r", "gfx_g", "gfx_b", "gfx_a", "gfx_a2", "gfx_x", "gfx_y",
  "gfx_w", "gfx_h", "gfx_mode", "gfx_clear", "gfx_texth", "gfx_dest", "gfx_ext_retina", "gfx_ext_flags",
  "mouse_x", "mouse_y", "mouse_cap", "mouse_wheel", "mouse_hwheel"];

// Built-in functions: [argument count (or -1 any), js template or handler]
var MATH1 = { sin: "Math.sin", cos: "Math.cos", tan: "Math.tan", asin: "Math.asin", acos: "Math.acos", atan: "Math.atan",
  exp: "Math.exp", log: "Math.log", log10: "R.log10", floor: "Math.floor", ceil: "Math.ceil", abs: "Math.abs",
  sign: "R.sign", sqr: "R.sqr", invsqrt: "R.invsqrt", sqrt: "R.sqrt", rand: "R.rand" };
var MATH2 = { atan2: "Math.atan2", pow: "R.pow", min: "R.min", max: "R.max" };

// The effect's memory size in slots, as EEL2 sets it from options:maxmem: 8388608 by
// default, rounded up to whole blocks of 65536 and at most 512 blocks (33554432).
function memMax(options) {
  var m = /(?:^|\s)maxmem=(\S+)/.exec(options || ""), n = m ? Math.floor(parseFloat(m[1])) || 0 : 0;
  if (n <= 0) return 8388608;
  return Math.min(Math.ceil(n / 65536), 512) * 65536;
}

function Compiler(effect, opts) {
  this.effect = effect;
  this.opts = opts || {};
  this.memMax = memMax(effect.header.options);
  this.vars = {};         // js name -> true (closure variables, start at 0)
  this.varOrder = [];
  this.fnDefs = {};       // eel name -> def (latest wins, like EEL2's redefinition)
  this.specs = {};        // spec key -> { js, code }
  this.specOrder = [];
  this.strings = [];      // literal strings
  this.strIndex = {};
  this.named = {};        // #name -> slot
  this.namedOrder = [];
  this.tmp = 0;
  this.temps = [];
  this.usesRand = false;
  this.usesFile = false;
  this.usesMidi = false;
  this.usedBuiltins = {};
}

function jsIdent(prefix, name) {
  return prefix + name.replace(/[^A-Za-z0-9_]/g, function (c) { return c === "." ? "$" : "_" + c.charCodeAt(0).toString(16) + "_"; });
}

Compiler.prototype.useVar = function (eelName) {
  var low = eelName.toLowerCase(); // EEL2 variable names are case-insensitive
  var js = jsIdent("v_", low);
  if (!this.vars[js]) { this.vars[js] = low; this.varOrder.push(js); }
  return js;
};
// temporaries live in the function being generated (fast locals, not shared state)
Compiler.prototype.newTmp = function () {
  var js = "t" + this.tmp++;
  this.temps.push(js);
  return js;
};
Compiler.prototype.withTemps = function (f) {
  var saved = this.temps;
  this.temps = [];
  var code = f.call(this);
  var decl = this.temps.length ? "var " + this.temps.join(", ") + ";\n" : "";
  this.temps = saved;
  return decl + code;
};
Compiler.prototype.strLiteral = function (v) {
  if (this.strIndex[v] == null) { this.strIndex[v] = this.strings.length; this.strings.push(v); }
  return 10000 + this.strIndex[v]; // read-only string handles, as in REAPER
};
Compiler.prototype.namedString = function (name) {
  var key = name.toLowerCase();
  if (name === "#") key = "#__tmp" + this.namedOrder.length; // each bare # is its own temporary
  if (this.named[key] == null) { this.named[key] = 90000 + this.namedOrder.length; this.namedOrder.push(key); }
  return this.named[key];
};

// scope: { ns, fn, locals: {name: eel name}, params: {name: js}, refs: {name: eel name}, instance, globals }
// eelName gives the global EEL2 name a variable resolves to (null for a plain parameter)
Compiler.prototype.eelName = function (name, scope) {
  var low = name.toLowerCase();
  if (!scope.fn) return low;
  if (scope.params[low]) return null;
  var head = low.split(".")[0], tail = low.slice(head.length);
  if (scope.refs[head]) return scope.refs[head] + tail;
  if (scope.locals[head]) return scope.locals[head] + tail;
  if (low === "this") return scope.ns || "this";
  if (low.indexOf("this.") === 0) return this.thisPath(low, scope.ns);
  if (scope.globals[low]) return low;
  if (scope.instance[head] && scope.ns) return scope.ns + "." + low;
  return low;
};
Compiler.prototype.resolveName = function (name, scope) {
  var low = name.toLowerCase();
  if (scope.fn && scope.params[low]) return { js: scope.params[low] };
  return { js: this.useVar(this.eelName(name, scope)) };
};
Compiler.prototype.thisPath = function (low, ns) {
  // this.x -> ns.x ; this..x -> parent(ns).x
  var rest = low.slice(5), base = ns;
  while (rest.charAt(0) === ".") { rest = rest.slice(1); base = base.indexOf(".") >= 0 ? base.slice(0, base.lastIndexOf(".")) : ""; }
  return base ? base + "." + rest : rest;
};
// name of a call target after namespace resolution (this.f, instance.f)
Compiler.prototype.resolveCallName = function (name, scope) {
  var low = name.toLowerCase();
  if (low.indexOf(".") < 0) return low;
  if (scope.fn) {
    var head = low.split(".")[0];
    if (scope.refs[head] || scope.locals[head] || head === "this" || (scope.instance[head] && scope.ns)) return this.eelName(low, scope);
  }
  return low;
};
Compiler.prototype.findFunction = function (callName) {
  // exact name, then shorter suffixes; the namespace is everything before the last part
  var parts = callName.split(".");
  for (var i = 0; i < parts.length; i++) {
    var cand = parts.slice(i).join(".");
    if (this.fnDefs[cand]) return { def: this.fnDefs[cand], ns: parts.slice(0, parts.length - 1).join(".") };
  }
  return null;
};

Compiler.prototype.specialize = function (def, ns, refs) {
  refs = refs || {};
  var key = def.uid + "|" + ns + "|" + Object.keys(refs).map(function (k) { return k + "=" + refs[k]; }).join(",");
  var sp = this.specs[key];
  if (sp) {
    if (sp.compiling) throw new Error("recursive call to " + def.name);
    return sp;
  }
  sp = this.specs[key] = { js: jsIdent("f" + this.specOrder.length + "_", def.name + (ns ? "@" + ns : "")), compiling: true };
  this.specOrder.push(key);
  var scope = { ns: ns, fn: def, locals: {}, params: {}, refs: refs, instance: {}, globals: {}, localPrefix: "" };
  var self = this;
  scope.localPrefix = "__l" + this.specOrder.length + ".";
  def.local.forEach(function (l) { scope.locals[l.toLowerCase()] = scope.localPrefix + l.toLowerCase(); self.useVar(scope.localPrefix + l.toLowerCase()); });
  def.instance.forEach(function (l) { scope.instance[l.toLowerCase()] = true; });
  def.globals.forEach(function (l) { scope.globals[l.toLowerCase()] = true; });
  var params = [];
  def.params.forEach(function (p, i) {
    if (refs[p.toLowerCase()] != null) return;
    var js = "p" + i + "_" + p.replace(/[^A-Za-z0-9_]/g, "_");
    scope.params[p.toLowerCase()] = js;
    params.push(js);
  });
  var body = this.withTemps(function () { return this.body(def.body, scope, true); });
  sp.code = "function " + sp.js + "(" + params.join(", ") + ") {\n" + body + "}\n";
  sp.compiling = false;
  return sp;
};

// ---- expression compilation (ANF-ish): returns { pre: [stmt], v: jsexpr, pure: bool }
// pre statements must run before v is evaluated; pure means v has no side effects and
// may be evaluated again (variable, constant, temp).

function ex(pre, v, pure) { return { pre: pre || [], v: v, pure: !!pure }; }

Compiler.prototype.body = function (node, scope, wantReturn) {
  var r = this.expr(node, scope);
  var s = r.pre.join("");
  if (wantReturn) s += "return " + r.v + ";\n";
  else if (!r.pure) s += r.v + ";\n";
  return s;
};
Compiler.prototype.stmts = function (node, scope) { // value discarded
  var r = this.expr(node, scope);
  return r.pre.join("") + (r.pure ? "" : r.v + ";\n");
};
// combine children so that their pre-statements and values keep left-to-right order:
// when a later child has statements to run first, earlier values are saved in temps
Compiler.prototype.seqArgs = function (rs) {
  var pre = [], vals = [];
  for (var i = 0; i < rs.length; i++) {
    if (rs[i].pre.length) {
      for (var j = 0; j < vals.length; j++) {
        if (!/^\(?-?[0-9.]+(e[-+]?[0-9]+)?\)?$/.test(vals[j])) {
          var t = this.newTmp();
          pre.push(t + " = " + vals[j] + ";\n");
          vals[j] = t;
        }
      }
      pre = pre.concat(rs[i].pre);
    }
    vals.push(rs[i].v);
  }
  return { pre: pre, vals: vals };
};
function num(v) {
  if (v === Infinity) return "Infinity";
  if (v === -Infinity) return "-Infinity";
  if (v !== v) return "NaN";
  var s = String(v);
  return v < 0 || (v === 0 && 1 / v < 0) ? "(" + s + ")" : s;
}
Compiler.prototype.truth = function (r) { return "R.t(" + r + ")"; };

Compiler.prototype.expr = function (n, scope) {
  var self = this;
  switch (n.t) {
    case "num": return ex(null, num(n.v), true);
    case "str": return ex(null, String(this.strLiteral(n.v)), true);
    case "sid": return ex(null, String(this.namedString(n.name)), true);
    case "var": return ex(null, this.resolveName(n.name, scope).js, true);
    case "seq": {
      var pre = [];
      for (var i = 0; i < n.items.length - 1; i++) pre.push(this.stmts(n.items[i], scope));
      var last = this.expr(n.items[n.items.length - 1], scope);
      return ex(pre.concat(last.pre), last.v, last.pure);
    }
    case "un": {
      var a = this.expr(n.a, scope);
      if (n.op === "-") return ex(a.pre, "(-" + a.v + ")", false);
      return ex(a.pre, "(" + this.truth(a.v) + " ? 0 : 1)", false);
    }
    case "bin": return this.binary(n, scope);
    case "if": return this.ifelse(n, scope);
    case "mem": {
      var r = this.memIndex(n, scope);
      // inline fast path for block 0 (M0); other blocks and out-of-range go through MEM.rc
      var tR = this.newTmp();
      return ex(r.pre, "(" + tR + " = " + r.v + " + 0.00001, " + tR + " > -1 && " + tR + " < 65536 ? M0[" + tR + " | 0] : MEM.rc(" + tR + "))", false);
    }
    case "asg": return this.assign(n, scope);
    case "call": return this.call(n, scope);
  }
  throw new Error("unknown node " + n.t);
};
Compiler.prototype.memIndex = function (n, scope) {
  var b = this.expr(n.base, scope);
  if (!n.idx) return ex(b.pre, b.v, b.pure);
  var i = this.expr(n.idx, scope);
  var s = this.seqArgs([b, i]);
  return ex(s.pre, "(" + s.vals[0] + " + " + s.vals[1] + ")", false);
};
var BIN_JS = { "+": "+", "-": "-", "*": "*", "/": "/", "<": "<", ">": ">", "<=": "<=", ">=": ">=", "===": "===", "!==": "!==" };
// EEL2 folds operations on constants at compile time and filters the result like an
// assignment (1/0 is 0, not inf)
function fold(n) {
  var a = constOf(n.a), b = constOf(n.b), v;
  if (a == null || b == null) return null;
  switch (n.op) {
    case "+": v = a + b; break;
    case "-": v = a - b; break;
    case "*": v = a * b; break;
    case "/": v = a / b; break;
    default: return null;
  }
  return (v >= 2.2250738585072014e-308 || v <= -2.2250738585072014e-308) && v - v === 0 ? v : 0;
}
Compiler.prototype.binary = function (n, scope) {
  var op = n.op;
  var fv = fold(n);
  if (fv != null) return ex(null, num(fv), true);
  if (op === "&&" || op === "||") {
    var a = this.expr(n.a, scope), b = this.expr(n.b, scope);
    if (!b.pre.length) {
      return ex(a.pre, "(" + this.truth(a.v) + " " + op + " " + this.truth(b.v) + " ? 1 : 0)", false);
    }
    var t = this.newTmp();
    var pre = a.pre.concat([t + " = " + this.truth(a.v) + " ? 1 : 0;\n",
      "if (" + (op === "&&" ? "" : "!") + t + ") {\n" + b.pre.join("") + t + " = " + this.truth(b.v) + " ? 1 : 0;\n}\n"]);
    return ex(pre, t, true);
  }
  var ra = this.expr(n.a, scope), rb = this.expr(n.b, scope);
  var s = this.seqArgs([ra, rb]), x = s.vals[0], y = s.vals[1];
  var v;
  if (BIN_JS[op]) v = "(" + x + " " + BIN_JS[op] + " " + y + ")";
  if (op === "<" || op === ">" || op === "<=" || op === ">=" || op === "===" || op === "!==") v = "(" + v + " ? 1 : 0)";
  else if (op === "==") v = "(R.eq(" + x + ", " + y + ") ? 1 : 0)";
  else if (op === "!=") v = "(R.eq(" + x + ", " + y + ") ? 0 : 1)";
  else if (op === "^") v = "R.pow(" + x + ", " + y + ")";
  else if (op === "%") v = "R.mod(" + x + ", " + y + ")";
  else if (op === "<<") v = "R.shl(" + x + ", " + y + ")";
  else if (op === ">>") v = "R.shr(" + x + ", " + y + ")";
  else if (op === "&") v = "R.and(" + x + ", " + y + ")";
  else if (op === "|") v = "R.or(" + x + ", " + y + ")";
  else if (op === "~") v = "R.xor(" + x + ", " + y + ")";
  if (!v) throw new Error("operator " + op);
  return ex(s.pre, v, false);
};
Compiler.prototype.ifelse = function (n, scope) {
  var c = this.expr(n.c, scope);
  var a = n.a ? this.expr(n.a, scope) : ex(null, "0", true);
  var b = n.b ? this.expr(n.b, scope) : ex(null, "0", true);
  if (!a.pre.length && !b.pre.length) return ex(c.pre, "(" + this.truth(c.v) + " ? " + a.v + " : " + b.v + ")", false);
  var t = this.newTmp();
  var code = "if (" + this.truth(c.v) + ") {\n" + a.pre.join("") + t + " = " + a.v + ";\n} else {\n" + b.pre.join("") + t + " = " + b.v + ";\n}\n";
  return ex(c.pre.concat([code]), t, true);
};
// EEL2 filters denormals / inf / NaN on "=" only when the value may be one; it decides
// that at compile time like this (nseel-compiler.c, canHaveDenormalOutput)
var WONT_MAKE = { sin: 1, cos: 1, sqrt: 1, abs: 1, atan: 1, asin: 1, acos: 1, sqr: 0, min: 1, max: 1 };
var CLEARS = { floor: 1, ceil: 1, sign: 1, rand: 1, strlen: 1, invsqrt: 0 };
function constOf(n) { return n.t === "num" ? n.v : n.t === "un" && n.op === "-" && n.a.t === "num" ? -n.a.v : null; }
function canDenorm(n) {
  if (n.t === "bin" && fold(n) != null) return false;
  switch (n.t) {
    case "num": return n.v !== 0 && Math.abs(n.v) < 2.2250738585072014e-308;
    case "var": case "mem": case "str": case "sid": case "__raw": case "asg": return false;
    case "seq": return canDenorm(n.items[n.items.length - 1]);
    case "un": return n.op === "-" ? canDenorm(n.a) : false;
    case "if": return (n.a ? canDenorm(n.a) : false) || (n.b ? canDenorm(n.b) : false);
    case "bin": {
      var ca = constOf(n.a), cb = constOf(n.b);
      switch (n.op) {
        case "+": case "-":
          return !((ca != null && Math.abs(ca) > 1e-50) || (cb != null && Math.abs(cb) > 1e-50));
        case "*":
          if (ca != null && Math.abs(ca) >= 1) return canDenorm(n.b);
          if (cb != null && Math.abs(cb) >= 1) return canDenorm(n.a);
          return true;
        case "/":
          if (ca != null && Math.abs(ca) <= 1) return canDenorm(n.b);
          if (cb != null && Math.abs(cb) <= 1) return canDenorm(n.a);
          return true;
        case "^": return true;
        default: return false; // comparisons, logic, integer ops give 0/1 or whole numbers
      }
    }
    case "call": {
      var nm = n.name.toLowerCase();
      if (nm === "loop" || nm === "while") return false;
      if (CLEARS[nm]) return false;
      if (WONT_MAKE[nm]) return n.args.some(canDenorm);
      return true;
    }
  }
  return true;
}
Compiler.prototype.assign = function (n, scope) {
  var target = n.target, op = n.op;
  while (target.t === "seq" && target.items.length === 1) target = target.items[0];
  if (target.t === "if") {
    // (c ? a : b) = v
    var cc = this.expr(target.c, scope);
    var t = this.newTmp();
    var va = this.expr(n.value, scope);
    var pre = cc.pre.concat(va.pre, [t + " = " + va.v + ";\n"]);
    var ra = target.a ? this.assign({ t: "asg", op: op, target: target.a, value: { t: "__raw", v: t } }, scope) : null;
    var rb = target.b ? this.assign({ t: "asg", op: op, target: target.b, value: { t: "__raw", v: t } }, scope) : null;
    var t2 = this.newTmp();
    pre.push("if (" + this.truth(cc.v) + ") {\n" + (ra ? ra.pre.join("") + t2 + " = " + ra.v + ";\n" : t2 + " = 0;\n") +
      "} else {\n" + (rb ? rb.pre.join("") + t2 + " = " + rb.v + ";\n" : t2 + " = 0;\n") + "}\n");
    return ex(pre, t2, true);
  }
  var val = n.value.t === "__raw" ? ex(null, n.value.v, true) : this.expr(n.value, scope);
  var filter = canDenorm(n.value);
  if (target.t === "var") {
    var js = this.resolveName(target.name, scope).js;
    var rhs = this.compound(op, js, val.v);
    if (filter && op === "=") rhs = "R.d(" + rhs + ")";
    return ex(val.pre, "(" + js + " = " + rhs + ")", false);
  }
  if (target.t === "mem") {
    var idx = this.memIndex(target, scope);
    var s = this.seqArgs([idx, val]);
    var tI = this.newTmp();
    // the index as EEL2 rounds it, or -1 (the shared scratch slot) when out of range (memMax slots)
    var pre2 = s.pre.concat([tI + " = " + s.vals[0] + " + 0.00001; " + tI + " = " + tI + " > -1 && " + tI + " < " + this.memMax + " ? " + tI + " | 0 : -1;\n"]);
    var cur = "(" + tI + " >= 0 && " + tI + " < 65536 ? M0[" + tI + "] : MEM.g(" + tI + "))";
    var r2 = this.compound(op, cur, s.vals[1]);
    if (filter && op === "=") r2 = "R.d(" + r2 + ")";
    return ex(pre2, "(" + tI + " >= 0 && " + tI + " < 65536 ? (M0[" + tI + "] = " + r2 + ") : MEM.w(" + tI + ", " + r2 + "))", false);
  }
  if (target.t === "call") {
    var fnr = this.call(target, scope, true);
    if (fnr.lvalue) return fnr.lvalue(op, val, filter);
  }
  throw new Error("cannot assign to " + target.t);
};
Compiler.prototype.compound = function (op, cur, v) {
  switch (op) {
    case "=": return v;
    case "+=": return cur + " + " + v;
    case "-=": return cur + " - " + v;
    case "*=": return cur + " * " + v;
    case "/=": return cur + " / " + v;
    case "%=": return "R.mod(" + cur + ", " + v + ")";
    case "|=": return "R.or(" + cur + ", " + v + ")";
    case "&=": return "R.and(" + cur + ", " + v + ")";
    case "~=": return "R.xor(" + cur + ", " + v + ")";
    case "^=": return "R.pow(" + cur + ", " + v + ")";
  }
  throw new Error("op " + op);
};

Compiler.prototype.call = function (n, scope, wantLvalue) {
  var self = this, name = n.name.toLowerCase(), args = n.args;
  // user functions (namespaced or not) shadow nothing built in: EEL2 checks its own table first
  if (name === "loop") {
    if (args.length !== 2) throw new Error("loop() needs 2 parameters");
    var cnt = this.expr(args[0], scope), i = this.newTmp();
    var bodyS = this.stmts(args[1], scope);
    return ex(cnt.pre.concat([
      "for (" + i + " = R.loopCount(" + cnt.v + "); " + i + " > 0; " + i + "--) {\n" + bodyS + "}\n"]), "0", true);
  }
  if (name === "while") {
    if (args.length === 1) {
      var w = this.expr(args[0], scope), g = this.newTmp();
      return ex(["for (" + g + " = 1048576; " + g + " > 0; " + g + "--) {\n" + w.pre.join("") + "if (!" + this.truth(w.v) + ") break;\n}\n"], "0", true);
    }
    var cnd = this.expr(args[0], scope), g2 = this.newTmp();
    var b2 = this.stmts(args[1], scope);
    return ex(["for (" + g2 + " = 1048576; " + g2 + " > 0; " + g2 + "--) {\n" + cnd.pre.join("") + "if (!" + this.truth(cnd.v) + ") break;\n" + b2 + "}\n"], "0", true);
  }
  var callName = this.resolveCallName(name, scope);
  var user = this.findFunction(callName);
  if (user) {
    var def = user.def;
    if (def.params.length !== args.length && !(def.params.length === 0 && args.length === 0)) {
      throw new Error(def.name + "() needs " + def.params.length + " parameters, got " + args.length);
    }
    var refs = {}, valArgs = [];
    def.params.forEach(function (p, i) {
      if (def.refs && def.refs[p.toLowerCase()]) {
        var a = args[i];
        while (a.t === "seq" && a.items.length === 1) a = a.items[0];
        var en = a.t === "var" ? self.eelName(a.name, scope) : null;
        if (en == null) throw new Error(def.name + "(): parameter " + p + "* needs a variable");
        refs[p.toLowerCase()] = en;
      } else valArgs.push(args[i]);
    });
    var sp = this.specialize(def, user.ns, refs);
    var rs = valArgs.map(function (a) { return self.expr(a, scope); });
    var s = this.seqArgs(rs);
    return ex(s.pre, sp.js + "(" + s.vals.join(", ") + ")", false);
  }
  var b = BUILTINS[name];
  if (!b) throw new Error("unknown function " + n.name + "()");
  this.usedBuiltins[name] = true;
  return b.call(this, args, scope, n, wantLvalue);
};

// helpers for builtins
function plainArgs(self, args, scope) {
  var rs = args.map(function (a) { return self.expr(a, scope); });
  return self.seqArgs(rs);
}
function fixed(nargs, fmt) {
  return function (args, scope, n) {
    if (nargs >= 0 && args.length !== nargs) throw new Error(n.name + "() needs " + nargs + " parameters");
    var s = plainArgs(this, args, scope);
    return ex(s.pre, fmt.replace(/\$(\d)/g, function (_, k) { return s.vals[+k - 1]; }).replace("$*", s.vals.join(", ")), false);
  };
}
function varTarget(self, node, scope) {
  if (node.t !== "var") throw new Error("expected a variable");
  return self.resolveName(node.name, scope).js;
}

var BUILTINS = {};
Object.keys(MATH1).forEach(function (k) { BUILTINS[k] = fixed(1, MATH1[k] + "($1)"); });
Object.keys(MATH2).forEach(function (k) { BUILTINS[k] = fixed(2, MATH2[k] + "($1, $2)"); });
BUILTINS.rand = function (args, scope) {
  this.usesRand = true;
  var s = plainArgs(this, args.length ? args : [{ t: "num", v: 1 }], scope);
  return ex(s.pre, "R.rand(" + s.vals[0] + ")", false);
};
BUILTINS.memset = fixed(3, "MEM.set($1, $2, $3)");
BUILTINS.memcpy = fixed(3, "MEM.copy($1, $2, $3)");
BUILTINS.freembuf = fixed(1, "MEM.free($1)");
BUILTINS.mem_set_values = fixed(-1, "MEM.setValues($*)");
BUILTINS.fft = fixed(2, "R.fft(MEM, $1, $2)");
BUILTINS.ifft = fixed(2, "R.ifft(MEM, $1, $2)");
BUILTINS.fft_real = fixed(2, "R.fftReal(MEM, $1, $2)");
BUILTINS.ifft_real = fixed(2, "R.ifftReal(MEM, $1, $2)");
BUILTINS.fft_permute = fixed(2, "R.fftPermute(MEM, $1, $2)");
BUILTINS.fft_ipermute = fixed(2, "R.fftIpermute(MEM, $1, $2)");
BUILTINS.convolve_c = fixed(3, "R.convolveC(MEM, $1, $2, $3)");
BUILTINS.time_precise = fixed(0, "H.timePrecise()");
BUILTINS.time = fixed(0, "H.time()");
// sliderchange(sliderX) / slider_automate(sliderX): a slider variable stands for its bit
// (slider1 = 1, slider2 = 2, slider3 = 4...), anything else is a mask; both return 0
function sliderMask(method) {
  return function (args, scope, n) {
    if (args.length !== 1) throw new Error(n.name + "() needs 1 parameter");
    var a = args[0];
    if (a.t === "var") {
      var js = this.resolveName(a.name, scope).js, hit = null;
      this.effect.header.sliders.forEach(function (sl) { if (jsIdent("v_", (sl.varName || ("slider" + sl.index)).toLowerCase()) === js) hit = sl; });
      if (hit) return ex([], "H." + method + "(" + Math.pow(2, hit.index - 1) + ")", false);
    }
    var s = plainArgs(this, args, scope);
    return ex(s.pre, "H." + method + "(Math.round(Math.abs(" + s.vals[0] + ")))", false);
  };
}
BUILTINS.sliderchange = sliderMask("sliderChange");
BUILTINS.slider_automate = sliderMask("sliderAutomate");
BUILTINS.slider_show = fixed(-1, "H.sliderShow($*)");
// spl(n) / slider(n) read and write channel / slider n
BUILTINS.spl = function (args, scope, n, wantLvalue) {
  var s = plainArgs(this, args, scope);
  var self = this;
  var r = ex(s.pre, "IO.getSpl(" + s.vals[0] + ")", false);
  r.lvalue = function (op, val, filter) {
    var t = self.newTmp(), sv = self.seqArgs([ex(null, s.vals[0], false), val]);
    var rhs = self.compound(op, "IO.getSpl(" + t + ")", sv.vals[1]);
    if (filter && op === "=") rhs = "R.d(" + rhs + ")";
    return ex(s.pre.concat(sv.pre, [t + " = " + sv.vals[0] + ";\n"]), "IO.setSpl(" + t + ", " + rhs + ")", false);
  };
  return r;
};
BUILTINS.slider = function (args, scope, n) {
  var s = plainArgs(this, args, scope);
  var self = this;
  var r = ex(s.pre, "IO.getSlider(" + s.vals[0] + ")", false);
  r.lvalue = function (op, val, filter) {
    var t = self.newTmp(), sv = self.seqArgs([ex(null, s.vals[0], false), val]);
    var rhs = self.compound(op, "IO.getSlider(" + t + ")", sv.vals[1]);
    if (filter && op === "=") rhs = "R.d(" + rhs + ")";
    return ex(s.pre.concat(sv.pre, [t + " = " + sv.vals[0] + ";\n"]), "IO.setSlider(" + t + ", " + rhs + ")", false);
  };
  return r;
};
// strings
BUILTINS.strlen = fixed(1, "S.len($1)");
BUILTINS.strcpy = fixed(2, "S.cpy($1, $2)");
BUILTINS.strcat = fixed(2, "S.cat($1, $2)");
BUILTINS.strcmp = fixed(2, "S.cmp($1, $2)");
BUILTINS.stricmp = fixed(2, "S.icmp($1, $2)");
BUILTINS.strncmp = fixed(3, "S.ncmp($1, $2, $3)");
BUILTINS.strncpy = fixed(3, "S.ncpy($1, $2, $3)");
BUILTINS.strcpy_substr = fixed(-1, "S.substr($*)");
BUILTINS.strcpy_from = fixed(3, "S.from($1, $2, $3)");
BUILTINS.str_getchar = fixed(-1, "S.getchar($*)");
BUILTINS.str_setchar = fixed(-1, "S.setchar($*)");
BUILTINS.sprintf = fixed(-1, "S.sprintf($*)");
BUILTINS.strcat = fixed(2, "S.cat($1, $2)");
BUILTINS.str_setlen = fixed(2, "S.setlen($1, $2)");
// gfx (the page draws; see runtime/null-gfx.js)
["gfx_line:-1", "gfx_rect:-1", "gfx_roundrect:-1", "gfx_circle:-1", "gfx_arc:-1", "gfx_setfont:-1",
  "gfx_drawstr:-1", "gfx_drawnumber:2", "gfx_drawchar:1", "gfx_lineto:-1", "gfx_setpixel:3", "gfx_triangle:-1",
  "gfx_gradrect:-1", "gfx_set:-1", "gfx_getchar:-1", "gfx_muladdrect:-1", "gfx_showmenu:1", "gfx_setcursor:-1"].forEach(function (d) {
  var p = d.split(":");
  BUILTINS[p[0]] = fixed(+p[1], "G." + p[0].slice(4) + "($*)");
});
// gfx_measurestr(str, w, h) and gfx_measurechar(c, w, h) write w and h
function measureLike(method) {
  return function (args, scope) {
    if (args.length !== 3) throw new Error("gfx_" + method + "() needs 3 parameters");
    var w = varTarget(this, args[1], scope), h = varTarget(this, args[2], scope);
    var s = plainArgs(this, [args[0]], scope);
    return ex(s.pre, "(G." + method + "(" + s.vals[0] + "), " + w + " = G.mw, " + h + " = G.mh, 1)", false);
  };
}
BUILTINS.gfx_measurestr = measureLike("measurestr");
BUILTINS.gfx_measurechar = measureLike("measurechar");
// files (Convolution's IR): the host hands over decoded audio
BUILTINS.file_open = function (args, scope) { this.usesFile = true; var s = plainArgs(this, args, scope); return ex(s.pre, "H.fileOpen(" + s.vals[0] + ")", false); };
BUILTINS.file_close = fixed(1, "H.fileClose($1)");
BUILTINS.file_avail = fixed(1, "H.fileAvail($1)");
BUILTINS.file_rewind = fixed(1, "H.fileRewind($1)");
BUILTINS.file_mem = fixed(3, "H.fileMem(MEM, $1, $2, $3)");
BUILTINS.file_riff = function (args, scope) {
  if (args.length !== 3) throw new Error("file_riff() needs 3 parameters");
  var nch = varTarget(this, args[1], scope), sr = varTarget(this, args[2], scope);
  var s = plainArgs(this, [args[0]], scope);
  return ex(s.pre, "(H.fileRiff(" + s.vals[0] + "), " + nch + " = H.riffCh, " + sr + " = H.riffSr, 1)", false);
};
BUILTINS.file_var = function (args, scope) {
  var v = varTarget(this, args[1], scope);
  var s = plainArgs(this, [args[0]], scope);
  return ex(s.pre, "(" + v + " = H.fileVar(" + s.vals[0] + ", " + v + "), 1)", false);
};
BUILTINS.file_text = fixed(1, "0");
// __denormal_likely(x) etc. are hints only
BUILTINS.__denormal_likely = fixed(1, "$1");
BUILTINS.__denormal_unlikely = fixed(1, "$1");
// MIDI: the host queues the block's events (offset in the block, status, data 1, data 2);
// midirecv takes the next one. The 3-parameter forms pack the data bytes as msg2 + msg3 * 256.
BUILTINS.midirecv = function (args, scope) {
  if (args.length !== 3 && args.length !== 4) throw new Error("midirecv() needs 3 or 4 parameters");
  this.usesMidi = true;
  var self = this, t = args.map(function (a) { return varTarget(self, a, scope); });
  var set = args.length === 4
    ? t[0] + " = H.mo, " + t[1] + " = H.m1, " + t[2] + " = H.m2, " + t[3] + " = H.m3"
    : t[0] + " = H.mo, " + t[1] + " = H.m1, " + t[2] + " = H.m2 + H.m3 * 256";
  return ex([], "(H.midiRecv() ? (" + set + ", 1) : 0)", false);
};
BUILTINS.midisend = function (args, scope) {
  if (args.length !== 3 && args.length !== 4) throw new Error("midisend() needs 3 or 4 parameters");
  this.usesMidi = true;
  var s = plainArgs(this, args, scope);
  return ex(s.pre, args.length === 4 ? "H.midiSend(" + s.vals.join(", ") + ")"
    : "H.midiSend(" + s.vals[0] + ", " + s.vals[1] + ", " + s.vals[2] + " % 256, Math.floor(" + s.vals[2] + " / 256))", false);
};

// ---------------------------------------------------------------- effect ---------------

function idOf(file) { return path.basename(file).replace(/\.jsfx(-inc)?$/, ""); }

function collectCode(effect) {
  // imported files' sections run first (functions from imports are defined before use)
  var out = { init: [], slider: [], block: [], sample: [], gfx: [], serialize: [] };
  function walk(e) {
    e.imported.forEach(walk);
    Object.keys(out).forEach(function (k) {
      e.sections[k].forEach(function (chunk) { out[k].push({ file: e.file, line: chunk.line, text: chunk.text }); });
    });
  }
  walk(effect);
  return out;
}

function translate(file, opts) {
  opts = opts || {};
  var effect = readJsfx(file);
  var hdr = effect.header;
  var c = new Compiler(effect, opts);
  var code = collectCode(effect);
  var uid = 0;
  var compiled = {};
  ["init", "slider", "block", "sample", "gfx", "serialize"].forEach(function (sec) {
    var parts = code[sec];
    var nodes = [];
    parts.forEach(function (chunk) {
      // keep line numbers in error messages: pad with newlines
      var src = new Array(chunk.line + 1).join("\n") + chunk.text;
      var p = new Parser(src, path.relative(process.cwd(), chunk.file) + " @" + sec);
      var prog = p.program();
      p.functions.forEach(function (f) { f.uid = uid++; c.fnDefs[f.name.toLowerCase()] = f; }); // functions defined in this chunk
      nodes.push({ node: prog, fns: p.functions, where: chunk.file });
    });
    compiled[sec] = nodes;
  });
  // Functions are defined while parsing, so all are known before code is generated; EEL2
  // needs a function defined before its first use, which holds for working JSFX.
  var scope = { ns: "", fn: null, locals: {}, params: {}, refs: {}, instance: {}, globals: {} };
  var bodies = {};
  ["init", "slider", "block", "sample", "gfx", "serialize"].forEach(function (sec) {
    bodies[sec] = compiled[sec].map(function (x) { return c.stmts(x.node, scope); }).join("");
    bodies[sec + "Temps"] = c.temps; c.temps = [];
  });
  // slider variables and pins
  hdr.sliders.forEach(function (s) { c.useVar(s.varName || ("slider" + s.index)); });
  var nIn = hdr.inPins.length, nOut = hdr.outPins.length, nSpl = Math.max(nIn, nOut, 2);
  for (var k = 0; k < Math.max(nSpl, 2); k++) c.useVar("spl" + k);
  RESERVED_HOST.forEach(function (h) { c.useVar(h); });
  return emit(c, effect, bodies, { nIn: nIn, nOut: nOut, nSpl: nSpl, id: opts.id || idOf(file), share: opts.share });
}

// vars @gfx reads that the audio code writes: the page gets them from the audio thread
function liveVars(c, bodies) {
  var reads = {}, writesDsp = {};
  var re = /\b(v_[A-Za-z0-9_$]+)\b/g, m;
  while ((m = re.exec(bodies.gfx))) reads[m[1]] = true;
  // function bodies used by @gfx also read; approximate with all function code
  var dspCode = bodies.sample + bodies.block;
  var asg = /\b(v_[A-Za-z0-9_$]+) = /g;
  while ((m = asg.exec(dspCode))) writesDsp[m[1]] = true;
  // functions called from @sample/@block assign inside their own bodies
  c.specOrder.forEach(function (key) {
    var sp = c.specs[key];
    if (dspCode.indexOf(sp.js + "(") >= 0) { var a2 = /\b(v_[A-Za-z0-9_$]+) = /g; while ((m = a2.exec(sp.code))) writesDsp[m[1]] = true; }
    if (bodies.gfx.indexOf(sp.js + "(") >= 0) { var r2 = /\b(v_[A-Za-z0-9_$]+)\b/g; while ((m = r2.exec(sp.code))) reads[m[1]] = true; }
  });
  return Object.keys(reads).filter(function (v) { return writesDsp[v]; }).sort();
}

// whether the effect has an EEL2 variable of this (lower case) name
function hasVar(c, name) { return Object.keys(c.vars).some(function (js) { return c.vars[js] === name; }); }

function emit(c, effect, bodies, io) {
  var hdr = effect.header;
  var id = io.id;
  var sliders = hdr.sliders.map(function (s) {
    var o = { i: s.index, name: s.name, def: s.def, min: s.min, max: s.max, inc: s.inc };
    if (s.enums) o.enums = s.enums;
    if (s.shape !== "lin") { o.shape = s.shape; if (s.shapeArg != null) o.shapeArg = s.shapeArg; }
    if (s.hidden) o.hidden = true;
    if (s.path) { o.path = s.path; o.defFile = s.defFile; }
    return o;
  });
  var meta = {
    id: id, name: hdr.desc.replace(/^NULL\s+/, "").replace(/\s+GFX$/, ""), desc: hdr.desc, category: hdr.category, tags: hdr.tags,
    sliders: sliders, inPins: hdr.inPins, outPins: hdr.outPins, gfx: hdr.gfx,
    usesRand: c.usesRand, usesFile: c.usesFile
  };
  // instruments (no audio input, played by MIDI) and plugins that save state of their own
  if (c.usesMidi) meta.usesMidi = true;
  if (bodies.serialize.trim()) meta.serialize = true;
  // state @gfx and the audio share in REAPER, which a host running them apart passes over
  // (opts.share, from tools/build.js): pokes = variables @gfx sets and the audio takes and
  // clears; flash = memory the audio writes and @gfx reads and fades ([variable, length])
  if (io.share && hdr.gfx) {
    (io.share.pokes || []).forEach(function (v) { if (!hasVar(c, v)) throw new Error(id + ": no variable " + v); });
    (io.share.flash || []).forEach(function (f) { if (!hasVar(c, f[0])) throw new Error(id + ": no variable " + f[0]); });
    if (io.share.pokes && io.share.pokes.length) meta.pokes = io.share.pokes.slice();
    if (io.share.flash && io.share.flash.length) meta.flash = io.share.flash.map(function (f) { return [f[0], f[1]]; });
  }
  var live = hdr.gfx ? liveVars(c, bodies) : [];
  var L = [];
  L.push("// Generated by web/tools/eel2js.js from " + path.basename(effect.file) + ". Do not edit: run `node web/tools/build.js`.");
  L.push("// NULL JSFX v3.0 • https://nulljsfx.tech • MIT License");
  L.push("(function (g) {");
  L.push("\"use strict\";");
  L.push("var FX = " + JSON.stringify(meta) + ";");
  L.push("FX.strings = " + JSON.stringify(c.strings) + ";");
  L.push("FX.live = " + JSON.stringify(live.map(function (v) { return c.vars[v]; })) + ";");
  L.push("FX.create = function (R, H, G) {");
  L.push("var MEM = R.memory(" + (c.memMax === 8388608 ? "" : c.memMax) + "), M0 = MEM.b[0], S = R.strings(FX.strings), IO;");
  // variables
  // every EEL2 variable is a slot in one Float64Array (V[k]): plain doubles, no number boxing
  var declAt = L.length;
  L.push("");
  c.specOrder.forEach(function (k) { L.push(c.specs[k].code.trim()); });
  var sliderVar = {};
  hdr.sliders.forEach(function (s) { sliderVar[s.index] = c.useVar(s.varName || ("slider" + s.index)); });
  function sw(getter) {
    var out = [];
    hdr.sliders.forEach(function (s) { out.push("case " + s.index + ": " + getter(sliderVar[s.index]) + ";"); });
    return out.join(" ");
  }
  var spl = [];
  for (var k = 0; k < io.nSpl; k++) spl.push(c.useVar("spl" + k));
  L.push("IO = {");
  L.push("  getSpl: function (n) { n = Math.floor(n); switch (n) { " + spl.map(function (v, k) { return "case " + k + ": return " + v + ";"; }).join(" ") + " } return 0; },");
  L.push("  setSpl: function (n, v) { n = Math.floor(n); switch (n) { " + spl.map(function (v, k) { return "case " + k + ": " + v + " = v; break;"; }).join(" ") + " } return v; },");
  L.push("  getSlider: function (n) { switch (Math.floor(n + 0.00001)) { " + sw(function (v) { return "return " + v; }) + " } return 0; },");
  L.push("  setSlider: function (n, v) { switch (Math.floor(n + 0.00001)) { " + sw(function (v) { return v + " = v; break"; }) + " } return v; }");
  L.push("};");
  var gvars = {};
  ["gfx_r", "gfx_g", "gfx_b", "gfx_a", "gfx_x", "gfx_y", "gfx_w", "gfx_h", "gfx_texth", "gfx_ext_retina", "gfx_mode", "gfx_clear",
    "mouse_x", "mouse_y", "mouse_cap", "mouse_wheel"].forEach(function (v) { gvars[v] = c.useVar(v); });
  var host = {};
  ["srate", "samplesblock", "num_ch", "tempo", "play_state", "play_position", "beat_position", "ts_num", "ts_denom",
    "pdc_delay", "pdc_bot_ch", "pdc_top_ch", "trigger", "ext_noinit"].forEach(function (v) { host[v] = c.useVar(v); });
  function tv(sec) { var t = bodies[sec + "Temps"] || []; return t.length ? "var " + t.join(", ") + ";\n" : ""; }
  L.push("var inst = {");
  L.push("  FX: FX,");
  L.push("  init: function () {\n" + host.pdc_delay + " = 0; " + host.pdc_bot_ch + " = 0; " + host.pdc_top_ch + " = 0;\n" + tv("init") + bodies.init + "},");
  L.push("  slider: function () {\n" + tv("slider") + bodies.slider + "},");
  L.push("  block: function () {\n" + tv("block") + bodies.block + "},");
  // the per-sample loop, with @sample inlined
  var ins = io.nIn, outs = io.nOut;
  var loadIns = [], storeOuts = [];
  for (var a = 0; a < Math.max(ins, 0); a++) loadIns.push(spl[a] + " = (" + a + " < nin ? I[" + a + "][i] : 0);");
  for (var b = ins; b < io.nSpl; b++) loadIns.push(spl[b] + " = 0;");
  for (var o = 0; o < outs; o++) storeOuts.push("O[" + o + "][i] = " + spl[o] + ";");
  L.push("  // I: input channels (arrays), O: output channels; nin: how many inputs carry signal");
  // a caller that takes fewer outputs than the pins (the main pair of a multi-out instrument)
  // gets the rest written to scratch channels
  var padOuts = outs > 2 ? "if (O.length < " + outs + ") { O = O.slice(); while (O.length < " + outs + ") O.push(new Float64Array(n)); }\n" : "";
  L.push("  sample: function (I, O, n, nin) {\nvar i;\n" + padOuts + tv("sample") + "for (i = 0; i < n; i++) {\n" + loadIns.join(" ") + "\n" + bodies.sample + storeOuts.join(" ") + "\n}\n},");
  L.push("  gfx: function () {\n" + tv("gfx") + bodies.gfx + "},");
  L.push("  serialize: function () {\n" + tv("serialize") + bodies.serialize + "},");
  L.push("  getSlider: IO.getSlider, setSlider: IO.setSlider,");
  L.push("  setHost: function (k, v) { switch (k) { " + Object.keys(host).map(function (k2) { return "case \"" + k2 + "\": " + host[k2] + " = v; break;"; }).join(" ") + " } },");
  L.push("  getHost: function (k) { switch (k) { " + Object.keys(host).map(function (k2) { return "case \"" + k2 + "\": return " + host[k2] + ";"; }).join(" ") + " } return 0; },");
  L.push("  setGfx: function (k, v) { switch (k) { " + Object.keys(gvars).map(function (k2) { return "case \"" + k2 + "\": " + gvars[k2] + " = v; break;"; }).join(" ") + " } },");
  L.push("  getGfx: function (k) { switch (k) { " + Object.keys(gvars).map(function (k2) { return "case \"" + k2 + "\": return " + gvars[k2] + ";"; }).join(" ") + " } return 0; },");
  L.push("  getLive: function (out) { " + live.map(function (v, j) { return "out[" + j + "] = " + v + ";"; }).join(" ") + " return out; },");
  L.push("  setLive: function (a) { " + live.map(function (v, j) { return v + " = a[" + j + "];"; }).join(" ") + " },");
  L.push("  mem: MEM, str: S,");
  L.push("  getVar: function (name) { var k = FX.vars.indexOf(String(name).toLowerCase()); return k < 0 ? 0 : V[k]; },");
  L.push("  setVar: function (name, v) { var k = FX.vars.indexOf(String(name).toLowerCase()); if (k >= 0) V[k] = v; }");
  L.push("};");
  var bindAt = L.length;
  L.push("");
  L.push("return inst;");
  L.push("};");
  var names = c.varOrder, slot = {};
  names.forEach(function (n, k) { slot[n] = k; });
  // the canvas shim (runtime/null-gfx.js) reads and writes gfx_* and mouse_* in V directly
  var gslots = {};
  Object.keys(gvars).forEach(function (k2) { gslots[k2] = slot[gvars[k2]]; });
  L[bindAt] = "if (G.bind) G.bind(V, " + JSON.stringify(gslots) + ", S);";
  L[declAt] = "var V = new Float64Array(" + Math.max(names.length, 1) + ");";
  // rewrite v_ identifiers in the code (the JSON lines above FX.create carry names, not code)
  var createAt = L.indexOf("FX.create = function (R, H, G) {");
  for (var li = createAt + 1; li < L.length; li++) {
    L[li] = L[li].replace(/(^|[^A-Za-z0-9_$."])(v_[A-Za-z0-9_$]+)(?![A-Za-z0-9_$])/g, function (all, pre, n) {
      if (slot[n] == null) throw new Error("unknown variable " + n);
      return pre + "V[" + slot[n] + "]";
    });
  }
  L.splice(createAt, 0, "FX.vars = " + JSON.stringify(names.map(function (n) { return c.vars[n]; })) + ";");
  L.push("(g.NULLFX || (g.NULLFX = {}))[FX.id] = FX;");
  L.push("})(typeof globalThis !== \"undefined\" ? globalThis : this);");
  return { id: id, meta: meta, code: L.join("\n") + "\n" };
}

module.exports = { readJsfx: readJsfx, translate: translate, Parser: Parser, Compiler: Compiler };
