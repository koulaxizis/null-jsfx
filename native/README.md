# NULL JSFX: GFX, VST3 and CLAP versions

Every NULL JSFX plugin can have two extra versions next to the original JSFX in
`DATA/Effects/null_jsfx/`:

| Version | Where | Name in the host |
|---|---|---|
| JSFX GFX | `DATA/Effects/null_jsfx/gfx/<name>_gfx.jsfx` | `JS: NULL <Name> GFX` |
| VST3 / CLAP | `native/plugins/<name>/` | `<Name>` by `NULL JSFX` |

All three sound the same. CI checks it: `gfx_<name>` must match the JSFX exactly and
`compare_<name>` (the CLAP build) to within 1e-5 at 44.1 and 96 kHz.

They also look the same. One Slider plugins draw the same 360x240 design: the nulljsfx.tech dark palette
with a silver accent (never the site purple), a header with the logo, "NULL JSFX" above the
plugin name and the category on the right. One Slider plugins have a display card on top and
a horizontal slider card below. The look lives in two files that must stay in step:

- `DATA/Effects/null_jsfx/gfx/null_gfx.jsfx-inc` for the GFX JSFX (`ng_*` functions, `NG_*` colours)
- `native/common/NullUI.hpp` and `NullTheme.hpp` for VST3/CLAP (the same helpers by the same names)

`plugins/tilt` and `gfx/tilt_gfx.jsfx` are the reference for everything below.

## Adding a One Slider plugin

### 1. GFX JSFX: `DATA/Effects/null_jsfx/gfx/<name>_gfx.jsfx`

- Start from the plain JSFX. The audio code (`@init`, `@slider`, `@block`, `@sample`) stays
  **identical**. The `gfx_<name>` test renders both versions and compares them with tolerance 0.
- Header: `desc:NULL <Name> GFX`, add `gui` to the tags, the same category and footer lines,
  then `import null_gfx.jsfx-inc` after the footer. Give the slider label a `-` prefix
  (`slider1:50<0,100,1>-Amount`) so REAPER hides its own slider and shows the interface.
- Set `gfx_ext_retina = 1;` in `@init`. Move the `@slider` code into `function update()` and
  call `update();` from `@slider`, so the interface can re-run it after a change.
- Meters and scopes may read audio in `@sample` (`ng_tap(spl0, spl1)`, peak followers into
  new variables), but must never change what the plugin outputs. Use variables of your own
  (not ones the audio code uses) and a memory area the plugin does not use for `ng_tap_init`.
- `@gfx 360 240`, then:
  ```
  ng_one_slider_begin("NAME");      // header + display card (interior 24..336 x 62..156)
  ... the display ...
  ng_set_slider(ng_one_slider_control("LABEL", #caption, #value, "LEFT", "RIGHT",
                slider1, lo, hi, default, bipolar), step) ? update();
  ng_end();
  ```
- The display shows what the plugin does, and every plugin shows something meaningful: a
  frequency response (`ng_plot`, `ng_grid_freq`, `ng_freq_x`, `ng_lin_y`, `ng_line_to`), a
  transfer curve (`ng_grid`), level and gain-reduction meters (`ng_meter`, `ng_gr_meter`), a
  waveform (`ng_scope`) or a vectorscope for stereo plugins (`ng_vector`).

### 2. VST3/CLAP: `native/plugins/<name>/`

- `DistrhoPluginInfo.h`: copy Tilt's and change the name (the plugin name without "NULL"),
  URI `https://nulljsfx.tech/plugins/<name>`, CLAP id `tech.nulljsfx.<name>`, a unique
  4-character `DISTRHO_PLUGIN_UNIQUE_ID` (not used by any other plugin in `plugins/`), and the
  VST3 categories and CLAP features. Keep `DISTRHO_PLUGIN_WANT_DIRECT_ACCESS 1`. If the JSFX
  sets `pdc_delay`, set `DISTRHO_PLUGIN_WANT_LATENCY 1` and report the same latency.
- `<Name>DSP.hpp`: a framework-free C++ port of the JSFX audio code, computed in `double` like
  EEL2, with the same order of operations so the output matches. Things that bite:
  - EEL2 `==` and `!=` are fuzzy (|a - b| < 0.00001); `===` is exact. `x % y` works on
    integers (both sides truncated). `a ^ b` is pow. Memory and unset variables start at 0.
  - `@init` runs at load and on every sample-rate change or reset; `@slider` runs after it
    and whenever the slider moves; `@block` before every block (ysfx uses 512-sample blocks
    in the test); `@sample` per sample.
  - `srate` is the sample rate; `$pi` is `3.141592653589793`.
  - Arrays in JSFX memory become `std::vector<double>` (or fixed arrays) sized for the
    largest sample rate the code supports; allocate in `setSampleRate`, never in `process`.
- `Plugin.cpp`: derive from `NullOneSliderPlugin` (`common/NullPlugin.hpp`) with the slider's
  name, symbol, min, max, default and unit, exactly as in the JSFX. Implement `getLabel`,
  `getDescription`, `getUniqueId`, `valueChanged`, `activate`, `sampleRateChanged` and `run`.
  In `run`, also fill `taps` (`taps.push(l, r)`, `inLevel`, `outLevel`, `gr`) if the
  display needs them.
- `UI.cpp`: derive from `NullOneSliderUI` (`common/NullUI.hpp`) and draw the **same display**
  as the GFX JSFX in `drawDisplay()` with the matching helpers (`setPlot`, `gridFreq`,
  `freqX`, `linY`, `curve`, `grid`, `meter`, `grMeter`, `scope`, `vectorscope`). Set
  `animated = true` in the Info when the display shows live audio. Override `formatCaption`
  and `formatValue` to match the GFX JSFX's texts.
- The default comparison tolerance is 1e-5. If a plugin cannot reach it for a reason that
  is not a bug (say, a float-only library function), put the arguments in
  `plugins/<name>/compare_args.txt` (e.g. `--tol 1e-4`) with a comment line explaining why.

### 3. Check

```sh
tests/run.sh DATA/Effects/null_jsfx/gfx/<name>_gfx.jsfx            # lint + render the GFX JSFX
native/scripts/build.sh --test --plugins "<name>"                  # build + compare_<name> + gfx_<name>
tools/gfx_render/build.sh                                          # once; prints the renderer path
tests/.build/gfx_render DATA/Effects/null_jsfx/gfx/<name>_gfx.jsfx out.png 720 480 <value>
native/scripts/screenshot.sh <name> out.png <value>                # the VST3/CLAP interface (Linux)
```

Look at both screenshots side by side: they should match.

## Adding an Essentials or Advanced plugin (knob layout)

`plugins/delay` and `gfx/delay_gfx.jsfx` are the reference. The layout is a header, a display
card (height `display`) and a controls card of rows of 6 cells: knobs for numeric sliders,
selectors (pill with arrows) for enum sliders. More than 2 rows of controls go on tabs.

### 1. GFX JSFX: `DATA/Effects/null_jsfx/gfx/<name>_gfx.jsfx`

Copy `<name>.jsfx`, prefix every slider label with `-`, `import null_gfx.jsfx-inc`, move the
`@slider` code into `function update()` (called from `@slider`), then:

```
@gfx 480 294
ng_knobs_begin("DELAY", "ESSENTIALS", 480, 120, 1, 0);    // name, category, width, display h, rows, tabs
// draw the display in ng_vx, ng_vy, ng_vw, ng_vh
ng_row(5);
ng_knob_ctl(1, "TIME", #v, 0, 2000, 250, 1, 0) ? update(); // slider index, label, value text, range, default, step, bipolar
ng_select_ctl(4, "SYNC", "OFF|ON", 0) ? update();
```

`ng_tabs("BAND 1|BAND 2", sel)` before the rows switches pages. Never call `slider()` directly
(the lint rejects it); write sliders with `ng_slider_set(idx, v)`.

### 2. VST3/CLAP: `native/plugins/<name>/`

- `<Name>Sliders.hpp`: one `SliderInfo` per JSFX slider in order (`step`, and `options` for enums).
- `<Name>DSP.hpp`: the JSFX maths in C++, in double where the JSFX computes. Snap 0.1-step
  parameters back to the JSFX grid in double before use (a float 0.3 detunes LFOs).
- `Plugin.cpp`: a `NullMultiPlugin`; `valueChanged` re-runs the `@slider` code; `setLatency`
  where the JSFX sets `pdc_delay`; `DISTRHO_PLUGIN_WANT_TIMEPOS` for tempo; values for the
  display go through `taps` (or a plugin-local struct for more than the Taps hold).
- `UI.cpp`: a `NullKnobsUI` with a `Layout` (title, category, width, display, tabs, rows,
  sliders, controls) and the same display in `drawDisplay()`; `formatValue` for custom text.
- `DistrhoPluginInfo.h`: plain name, unique ID, categories, UI size = the `@gfx` size.

`rand()` cannot be matched natively: use an exact Park-Miller step in doubles, as `riser.jsfx`.

### 3. Check

`compare_<name>` sets every slider (defaults, each at min and max, random sets) at 44.1 and
96 kHz. Build with `cmake -S native -B native/build-x -DNULL_TESTS=ON -DNULL_PLUGINS="<name>"`
and `ctest`, then compare `gfx_render` (at 2x the `@gfx` size) with `screenshot.sh <name> out.png ""`.

## Adding an instrument (`drum_machine` is the model)

An instrument is a JSFX with `//category: Instruments`, `in_pin:none` and stereo `out_pin`s that
plays from MIDI (`midirecv` in `@block`, queued and played at its offset in `@sample`) and,
where it has a sequencer, from the transport (`play_state`, `beat_position`, `tempo`). REAPER
re-runs `@init` at every transport start, so state the user edits (a pattern) is guarded by a
flag and kept with `@serialize`. The lint accepts the category, the pins and the MIDI builtins;
`jsfx_test` sends instruments a GM note every 1/8 s; the web build and orOS leave them out.

The VST3/CLAP sets `DISTRHO_PLUGIN_IS_SYNTH`, `WANT_MIDI_INPUT`, `WANT_TIMEPOS` and, for edited
data, `WANT_STATE` (`NullMultiPlugin(sliders, count, states)`, the state as text). `run()` turns
the transport into a beat position like the JSFX and calls `reset()` on a play start, as REAPER
re-runs `@init`. The UI reads live values from `NullTaps::aux[]`/`extra` and asks the DSP for
one-off actions (a preview) through `NullTaps::request`. `compare_<name>` detects an instrument
by its pins and plays notes into both builds with the transport running at 120 bpm. An
instrument gets no Rack module.

## Adding a MIDI effect (`chords` and `scale` are the models)

A MIDI effect is a JSFX with `//category: MIDI` and the four standard pins: MIDI in, MIDI out,
the audio passes through untouched (`spl0 = spl0;` so the lint sees an output). All its work is in
`@block`: every message from `midirecv` gets an absolute time (samples since `@init` plus its
offset), what it causes goes into a queue ordered by time (stable for equal times), and the
messages due before the end of the block go out with `midisend` at their offsets. So a strum or a
delay can reach past the block, and the result does not depend on the host's block size. Every
note-on sent is remembered with the key (channel * 128 + note) that made it: the key's note-off
ends exactly those notes (a strum note still waiting is dropped instead), a pitch two keys share
starts again and ends with the last key, all notes off (CC 120/123) releases the channel's keys
first; everything else passes through in order. `ext_noinit = 1` keeps notes in flight across a
transport start. `native/common/NullMidiNotes.hpp` is that engine in C++ (the same steps as the
JSFX's `cd_*`/`sc_*` functions), `NullMidiTaps.hpp` sends its output through DPF (SysEx, which a
JSFX never sees, passes through at its frame) and puts the sounding notes and held keys into
`Taps::aux[]` as bits, and `NullKeyboardUI.hpp` draws the display's keyboard and chips.

The VST3/CLAP sets `DISTRHO_PLUGIN_WANT_MIDI_INPUT` and `_OUTPUT`, keeps 2 inputs and 2 outputs
(hosts that only load effects into an effect slot accept it), VST3 category `Fx|Tools` and CLAP
features `note-effect`, `utility`. `compare_<name>`, `presets_<name>` and `gfx_<name>` detect the
category and send a stream of MIDI (overlapping notes on three channels, repeated note-ons,
velocity-0 note-offs, pressure, pitch bend, sustain, program changes, all notes off) and compare
every message sent, byte for byte and at the same sample, besides the audio; the web null test
(`web/test/nulltest.js`) does the same against the translated JavaScript. The web build lists MIDI
effects apart in `web/fx/index.json` (`"midi"`). A MIDI effect gets no Rack module.

## Factory presets (`tools/presets/<name>.json`)

Every plugin's presets live in `tools/presets/<name>.json` as `[["Preset Name", {"Slider": value}], ...]`
(slider names as in the `.jsfx` without the unit, enum sliders by option label, values on the
slider's step; anything not given keeps the default; a "Default" preset comes first by itself).
`tools/build_presets.py` writes from them `<name>.jsfx.rpl` and `gfx/<name>_gfx.jsfx.rpl` (REAPER's
preset menu), the preset block at the end of `@init` of the GFX JSFX plus the `ng_presets()` call
at the end of its `@gfx` (the selector in the header, `ng_pre_*` in `null_gfx.jsfx-inc`), and
`plugins/<name>/FactoryPresets.hpp`, which `NullUI` picks up for the same selector in the VST3/CLAP.
CI runs `build_presets.py --check`; `presets_<name>` renders every preset in the JSFX and the CLAP.
A plugin UI that overrides `onMouse` calls `presetMouse(ev)` first, and one that draws over the
base in `onNanoDisplay` calls `drawPresetMenu()` at the end.

## The Rack (`plugins/rack/`)

"Rack" by NULL JSFX is a VST3/CLAP host for up to 8 NULL modules, like Kilohearts Snap Heap;
REAPER users get the same chains as FX chains (`tools/build_chains.py`). There is no rack.jsfx.
Each slot has On, Mix, Band (Full, Low, Mid, High on the rack's two crossovers), Channel
(Stereo, Mid, Side, Left, Right) and Lane (A, B); 8 macros, 2 LFOs, an envelope follower and a
16-step sequencer drive up to 32 modulation routes.

- **Lanes**: with every slot on lane A (the default) the slots run in series. Put slots on lane
  B and lane A and lane B each run the rack input through their own slots, in slot order; the
  shorter lane is delayed to line up with the longer one (the rack reports the longer lane's
  latency) and Lane Mix blends them (0 = A, 100 = B). With no slot on B, Lane Mix does nothing.
- **Feedback loop**: Feedback (0..95 %) sends the output of slot Feedback To back into the input
  of slot Feedback From (either order), one 128-sample processing chunk later, through a DC
  blocker, a 40 Hz low cut, a 12 kHz high cut and a soft clip. At 0 nothing changes.
- **Step sequencer**: source `seq`: 16 step values (0..1), Steps (1..16), Rate (Hz) or Sync
  (the LFO note values; synced, the steps follow the host's beat), Glide (a ramp from the
  previous step over that share of each step). The UI draws the steps as bars to drag over.
- **Parameters** are numbered for good (hosts store them by index): 22 globals, then 29 per
  slot (On, Mix, Band, Channel, Value 1-25), then, appended after them, Seq Steps, Seq Rate,
  Seq Sync, Seq Glide, Seq Step 1-16, Lane Mix, Feedback, Feedback From, Feedback To and
  S1-S8 Lane (254..285). New parameters only ever go at the end, and every new feature must
  sound like nothing at its default, so older sessions load and sound the same.
- **State**: "slots" (8 module ids, comma separated), "routes" (`source>slot.value:depth`,
  `;` separated, e.g. `seq>2.1:0.5`) and "preset". New features add keys or sources, never
  change what an existing string means.
- `modules/<id>.hpp`: one adapter per plugin (`nulljsfx::Module`, `common/NullModule.hpp`)
  that drives the plugin's DSP exactly like its `Plugin.cpp`. CMake lists them in
  `generated/NullModules.hpp`; `-DNULL_MODULES="a;b"` limits the list for a quick build.
  `module_<id>` checks each adapter against its JSFX like `compare_<id>`. A new plugin gets
  a module unless it is a meter or needs buses other than a sidechain. A plugin with a
  sidechain input also derives from
  `rack::KeyedModule` (`RackKeyed.hpp`): the Rack gives it the rack input (the dry signal,
  lined up with the module) as its key, and `module_<id>` feeds the JSFX the track on its
  sidechain pins (Sidechain Ducker). A file slider is not a module value (Convolution: the
  built-in rooms only; build_chains.py numbers module values without file sliders).
- `RackEngine.hpp`: the engine, framework-free; `Plugin.cpp` and `UI.cpp` around it.
- `RackPresets.hpp`: generated by `tools/build_chains.py` from the FX chains plus its
  `RACK_PRESETS` (routing and macro presets REAPER chains cannot hold); `--check` in CI. A
  `RACK_PRESETS` entry can put slots on `lane="B"`, set the globals `lane_mix`, `feedback`,
  `fb_from`, `fb_to`, `seq_steps`, `seq_rate`, `seq_sync`, `seq_glide` and `seq` (a list of up
  to 16 step values) and route from `seq`; a preset that uses lanes, the loop or the sequencer
  gets no .RfxChain (Rack only, like `RACK_ONLY`). A plugin that is not a Rack module (Loudness
  Meter) can be a slot: the REAPER chain keeps it, the Rack preset leaves it out. Macro 1 of
  the multi-module presets is the amount of the effect and Macro 2 a second character control
  (named in `tools/site_chains.json`); a REAPER chain holds one route per slider.
- `test/rack_test.cpp` (`rack_test`): every preset renders, a single slot is bit-exact with
  its module, band splits sum flat, channel routing leaves the other side untouched, modulation,
  lanes line up, the loop repeats and decays, the sequencer steps, the ducker's key lines up.
  `test/RackEngineV1.hpp` is a frozen copy of the 2.1 engine: every preset that does not use
  the newer features must render bit for bit like it (also with Lane Mix, the loop's ends and
  the sequencer moved), which keeps older sessions sounding the same.

## Building

```sh
native/scripts/build.sh                 # every plugin, VST3 + CLAP -> native/dist/{VST3,CLAP}/NULL JSFX/
native/scripts/build.sh --test          # also the comparison tests (Linux/macOS; builds ysfx if needed)
native/scripts/build.sh --plugins "a;b" # only some plugins
native/scripts/build.sh --dpf DIR       # offline: a local DPF checkout (with dgl/src/pugl-upstream)
```

Requirements: CMake 3.15+, a C++17 compiler, Git. Linux also needs `libgl1-mesa-dev libx11-dev
libxcursor-dev libxext-dev libxrandr-dev libdbus-1-dev`; Windows needs Visual Studio 2022 and Git
Bash. `.github/workflows/native.yml` builds all three OSes and uploads `NULL-JSFX-VST3-CLAP-<OS>`
(VST3 and CLAP) and `NULL-JSFX-JSFX` (plain and GFX JSFX), each with `INSTALL.txt`; a published
release gets them as zips.

DPF ([DISTRHO Plugin Framework](https://github.com/DISTRHO/DPF), ISC) is fetched at a pinned
commit. The font is Noto Sans (SIL OFL 1.1, `common/fonts/`),
embedded at configure time as `NullFonts.h`; the GFX JSFX ask REAPER for "Noto Sans" with a
negative size so every OS uses the em size, and the JSFX package ships the fonts to install. On MSVC the Khronos GL headers in
`third_party/khronos` (MIT) fill in the GL 2.0 types Windows lacks.
