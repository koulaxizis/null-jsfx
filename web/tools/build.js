// Translates every NULL JSFX effect into web/fx/<id>.js and writes web/fx/index.json.
// Usage: node web/tools/build.js          (write)
//        node web/tools/build.js --check  (fail if web/fx is out of date; CI runs this)
// The plain JSFX is the sound; when a GFX version exists (DATA/Effects/null_jsfx/gfx/<id>_gfx.jsfx)
// its @gfx section and layout come along in the same file. Instruments (category Instruments: no
// audio input, played by MIDI) are translated the same way and listed apart in index.json
// ("instruments"), so a host that only knows effects does not offer them as effects; so are the
// MIDI effects (category MIDI: MIDI in, MIDI out, the audio passes through), listed as "midi".
"use strict";
var fs = require("fs"), path = require("path");
var T = require("./eel2js.js");

var root = path.resolve(__dirname, "../..");
var src = path.join(root, "DATA/Effects/null_jsfx");
var out = path.join(root, "web/fx");
var check = process.argv.indexOf("--check") >= 0;

var CATEGORY_ORDER = ["One Slider", "Essentials", "Advanced"];

// What an interface and its audio share through REAPER's common memory, which the web runs
// apart (the page draws, a worklet plays): variables @gfx sets for the audio to take (pokes),
// and memory the audio writes for @gfx to fade (flash: [variable holding the address, length]).
var SHARE = {
  drum_machine: { pokes: ["dm_preview"], flash: [["hit", 10]] },
  percussion: { pokes: ["pc_preview"], flash: [["hit", 11]] },
  sampler: { pokes: ["sm_preview"] }
};

function isInstrument(file) { return /^\/\/category: Instruments$/m.test(fs.readFileSync(file, "utf8")); }
function isMidi(file) { return /^\/\/category: MIDI$/m.test(fs.readFileSync(file, "utf8")); }

function build() {
  var files = {};
  var ids = fs.readdirSync(src).filter(function (f) { return /\.jsfx$/.test(f); }).map(function (f) { return f.replace(/\.jsfx$/, ""); }).sort();
  var index = [], instruments = [], midi = [];
  ids.forEach(function (id) {
    var gfxFile = path.join(src, "gfx", id + "_gfx.jsfx");
    var hasGfx = fs.existsSync(gfxFile);
    // the GFX version carries the same audio code plus @gfx (CI checks both sound the same)
    var r = T.translate(hasGfx ? gfxFile : path.join(src, id + ".jsfx"), { id: id, share: SHARE[id] });
    files[id + ".js"] = r.code;
    var plain = path.join(src, id + ".jsfx");
    (isInstrument(plain) ? instruments : isMidi(plain) ? midi : index).push({
      id: id, name: r.meta.name, category: r.meta.category, tags: r.meta.tags,
      gfx: r.meta.gfx ? [r.meta.gfx.w, r.meta.gfx.h] : null,
      ins: r.meta.inPins.length, outs: r.meta.outPins.length,
      sliders: r.meta.sliders.filter(function (s) { return !s.hidden || r.meta.gfx; }).length,
      bytes: Buffer.byteLength(r.code)
    });
  });
  index.sort(function (a, b) {
    var ca = CATEGORY_ORDER.indexOf(a.category), cb = CATEGORY_ORDER.indexOf(b.category);
    return ca !== cb ? ca - cb : a.name.localeCompare(b.name);
  });
  instruments.sort(function (a, b) { return a.name.localeCompare(b.name); });
  midi.sort(function (a, b) { return a.name.localeCompare(b.name); });
  var all = { version: "2.0", effects: index, instruments: instruments };
  if (midi.length) all.midi = midi;
  files["index.json"] = JSON.stringify(all, null, 1) + "\n";
  return files;
}

var files = build();
var stale = [];
Object.keys(files).forEach(function (name) {
  var p = path.join(out, name);
  var cur = fs.existsSync(p) ? fs.readFileSync(p, "utf8") : null;
  if (cur !== files[name]) stale.push(name);
});
var extra = fs.existsSync(out) ? fs.readdirSync(out).filter(function (f) { return !files[f]; }) : [];
if (check) {
  if (stale.length || extra.length) {
    console.error("web/fx is out of date: " + stale.concat(extra).join(", ") + "\nRun: node web/tools/build.js");
    process.exit(1);
  }
  console.log("web/fx is up to date (" + (Object.keys(files).length - 1) + " plugins)");
} else {
  if (!fs.existsSync(out)) fs.mkdirSync(out, { recursive: true });
  stale.forEach(function (name) { fs.writeFileSync(path.join(out, name), files[name]); });
  extra.forEach(function (name) { fs.unlinkSync(path.join(out, name)); });
  console.log("wrote " + stale.length + " file(s), removed " + extra.length + "; " + (Object.keys(files).length - 1) + " plugins");
}
