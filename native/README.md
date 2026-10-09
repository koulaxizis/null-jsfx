# NULL JSFX: GFX, VST3 and CLAP versions

Every NULL JSFX plugin can have two extra versions next to the original JSFX in
`DATA/Effects/null_jsfx/`:

| Version | Where | Name in the host |
|---|---|---|
| JSFX GFX | `DATA/Effects/null_jsfx/gfx/<name>_gfx.jsfx` | `JS: NULL <Name> GFX` |
| VST3 / CLAP | `native/plugins/<name>/` | `<Name>` by `NULL JSFX` |

All three sound the same. CI checks it: `gfx_<name>` must match the JSFX exactly and
`compare_<name>` (the CLAP build) to within 1e-5 at 44.1 and 96 kHz.

They also look the same. Both draw the same 360x240 design: the nulljsfx.tech dark palette
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

## Building

```sh
native/scripts/build.sh                 # every plugin, VST3 + CLAP -> native/dist/{VST3,CLAP}/NULL JSFX/
native/scripts/build.sh --test          # also the comparison tests (Linux/macOS; builds ysfx if needed)
native/scripts/build.sh --plugins "a;b" # only some plugins
native/scripts/build.sh --dpf DIR       # offline: a local DPF checkout (with dgl/src/pugl-upstream)
```

Requirements: CMake 3.15+, a C++17 compiler, Git. Linux also needs `libgl1-mesa-dev libx11-dev
libxcursor-dev libxext-dev libxrandr-dev libdbus-1-dev`; Windows needs Visual Studio 2022 and Git
Bash. `.github/workflows/native.yml` builds all three OSes and uploads a `NULL-JSFX-<os>`
package with the JSFX, VST3 and CLAP versions and `INSTALL.txt`.

DPF ([DISTRHO Plugin Framework](https://github.com/DISTRHO/DPF), ISC) is fetched at a pinned
commit. The font is DejaVu Sans, which DPF embeds. On MSVC the Khronos GL headers in
`third_party/khronos` (MIT) fill in the GL 2.0 types Windows lacks.
