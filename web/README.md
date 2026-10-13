# NULL JSFX for the web

The NULL JSFX effects as plain JavaScript, for browsers (AudioWorklet) and Node. The
effects are not rewritten by hand: `tools/eel2js.js` translates each JSFX's EEL2 code, so
the web version always follows the JSFX in `DATA/`. The first user is the Studio app of
[orOS](https://github.com/koulaxizis/oros).

## What is here

| Path | What it is |
| --- | --- |
| `tools/eel2js.js` | EEL2 → JavaScript translator (JSFX header, sliders, `@init`/`@slider`/`@block`/`@sample`/`@gfx`, functions, namespaces, `local`/`instance`, pass-by-reference) |
| `tools/build.js` | Translates every effect into `fx/<id>.js` and writes `fx/index.json`. `--check` fails when `fx/` is out of date |
| `fx/` | Generated effects (do not edit). Each registers `globalThis.NULLFX[id]` |
| `runtime/eel-rt.js` | EEL2 runtime: maths with EEL2's rounding rules, the 8M-slot memory, strings, FFT in WDL's order |
| `runtime/null-fx.js` | Runs one effect the way REAPER does (`@init` on load and at transport start, `@slider` on change, latency) |
| `runtime/null-worklet.js` | AudioWorklet processor `null-fx-chain`: a chain of effects with bypass, sliders, tempo/transport and IR files |
| `runtime/null-node.js` | Page side of the worklet: loads the files into an `AudioContext` and wraps the node |
| `runtime/null-gfx.js` | Draws `@gfx` on a `<canvas>` like REAPER's LICE, and `View`: the interface of an effect with mouse and wheel |
| `test/` | Null test against ysfx, runtime tests, `run.sh` |

## Use

```html
<script src="web/runtime/null-node.js"></script>
<script>
var ctx = new AudioContext();
NULLFXNode.load(ctx, "web/", ["tilt", "reverb"]).then(function () {
  var chain = new NULLFXNode.Chain(ctx, { chain: [
    { uid: "1", id: "tilt", sliders: { 1: 40 } },
    { uid: "2", id: "reverb" }
  ] });
  source.connect(chain.node);
  chain.node.connect(ctx.destination);
  chain.setSlider("1", 1, -20);       // slider1 of the first effect
  chain.setTime({ playing: true, tempo: 120, position: 0 });
});
</script>
```

An effect's interface: load `eel-rt.js`, `null-fx.js`, `null-gfx.js` and `fx/<id>.js` on the
page, then `new NULLGfx.View(NULLFX[id], canvas, { srate, sliders, onSlider })`. Call
`view.setLive(await chain.peek(uid))` and `view.draw()` on each animation frame; the view runs
its own copy of `@init`/`@slider` for what `@gfx` computes, and the worklet sends the values
the audio produces (meters, scopes).

## How close to REAPER

`test/nulltest.js` renders every effect (and GFX version) in ysfx, the JSFX engine REAPER's
code is built on, and in JavaScript: defaults, every slider at its minimum and maximum and
seeded random settings, at 44.1 and 96 kHz. The largest difference must stay below 1e-6
(−120 dB); most effects are bit-identical. The interfaces were compared with the reference
images in `assets/img/gfx/` (rendered by `tools/render_gfx_shots.sh`); the only differences
are from font hinting.

The translator keeps EEL2's own rules: `==` within 0.00001, `%` and bit operators on integers,
memory indexes rounded by +0.00001, denormals flushed where EEL2 flushes them, `rand()` from
the same Mersenne Twister and seed, `^` as power, and the operator precedence of EEL2's
grammar. Nothing is evaluated at runtime: effects are translated ahead of time into static
files.

Not supported yet: keyboard input in `@gfx` (`gfx_getchar`), `gfx_showmenu`, images
(`gfx_blit` and friends), MIDI, and `@serialize` (sliders are saved by the host).

## Check

```sh
web/test/run.sh          # build check, runtime tests, null test (builds ysfx once)
node web/tools/build.js  # after changing anything in DATA/ or tools/
```

MIT License, like NULL JSFX.
