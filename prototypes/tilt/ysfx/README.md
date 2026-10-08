# NULL Tilt YS (ysfx wrapper)

A single-purpose **VST3 + CLAP** plugin (also **AU** on macOS) that runs one JSFX, embedded in
the binary at build time. Under the hood it uses
[JoepVanlier/ysfx](https://github.com/JoepVanlier/ysfx) (Apache-2.0, JUCE 8 +
clap-juce-extensions), so the JSFX `@gfx` GUI works. JSFX without `@gfx` get ysfx's
generic slider panel.

- Name "NULL Tilt YS", vendor "NULL JSFX", copyright "Christos Koulaxizis", version 2.0.0
- Codes: plugin `NtYs`, manufacturer `NulJ`, CLAP id `tech.nulljsfx.tilt-ys`
  (all of them are cache variables in `CMakeLists.txt`, so the same project can build other NULL plugins)
- Embedded JSFX: `-DNULL_JSFX_FILE=...`. The default is `../gfx/null_tilt_gfx.jsfx`.
- Host parameters: one per JSFX slider, up to the highest `sliderN:` in the file (here, Amount).
  Slider defaults are used as parameter defaults.
- No Load/Recent/Edit/Preset header and no file drag-and-drop. The editor shows the `@gfx` (or the sliders) only.
- At startup the JSFX bytes are written once to a per-user cache file and loaded from there. ysfx compiles
  from a path. The cache is `<user app data>/NULL JSFX/embedded/<name>-<hash>/<file>.jsfx`, which is
  `~/.config/…` on Linux, `~/Library/Application Support/…` on macOS and `%APPDATA%\…` on Windows.
  It falls back to the temp directory. Saved sessions always reload the embedded JSFX.

## How it is put together

| File | Purpose |
|---|---|
| `CMakeLists.txt` | Fetches ysfx at **pinned commit `5c3452f`** (with its pinned submodules dr_libs + clap-juce-extensions) and JUCE 8.0.6 (the version upstream uses). It builds only the ysfx library from upstream and defines its own `juce_add_plugin` + `clap_juce_extensions_plugin` target from ysfx's `plugin/` sources. |
| `patches/ysfx-embedded-jsfx.patch` | About 80 lines, all behind `#if NULL_EMBEDDED_JSFX`, applied by `cmake/apply_patch.cmake` (idempotent) |
| `src/null_embedded.{h,cpp}` | Writes the embedded bytes to the cache file and returns its path |
| `cmake/embed_file.cmake` | Turns the JSFX into a C++ byte array at build time (rebuilds when the JSFX changes) |
| `test/` | Headless CLAP render host, ysfx reference renderer, Xvfb GUI smoke test, `compare.sh` |

What the patch does:
1. Loads the embedded JSFX synchronously in the processor constructor. `setStateInformation` always uses it.
2. Registers only the first N sliders as host parameters. Slider defaults become parameter defaults.
3. Uses one stereo in/out bus instead of 15 optional buses. No MIDI ports.
4. Hides the editor header (`m_headerSize = 0`) and turns off file drops and the "locate file" prompt.
5. Fixes CLAP automation. clap-juce-extensions applies parameter events with `setValue()` and runs the
   listeners later on the main thread, but ysfx's `processSliderChanges()` had already reverted the
   value by then, so CLAP automation never reached the JSFX. Pending host values are now synced at block
   start and before saving state.
6. After a state load, asks the host to rescan parameter values.
7. Names the single program "Default" (the VST3 validator requires a name) and adds `ScopedNoDenormals`.

Upstream CMake is not patched. ysfx's own `ysfx-s FX/instrument` targets are not built.
On Linux the plugin is linked with `--exclude-libs,ALL`, so the exports are only
`GetPluginFactory/ModuleEntry/ModuleExit` (VST3) and `clap_entry` (CLAP).

## Build

```bash
./build.sh                       # Release, embeds ../gfx/null_tilt_gfx.jsfx
./build.sh --jsfx ../../../DATA/Effects/null_jsfx/tilt.jsfx
./build.sh --test                # Linux: also builds test hosts and runs test/compare.sh
./build.sh -- -DNULL_PLUGIN_NAME="NULL Foo YS" ...   # extra CMake args
```

Outputs are copied to **`dist/<linux|macos|windows>/`**:
- `NULL Tilt YS.vst3` (bundle directory)
- `NULL Tilt YS.clap` (a file on Linux and Windows, a bundle on macOS)
- `NULL Tilt YS.component` (macOS only)

The raw build artefacts are in `build/NullTiltYS_artefacts/Release/{VST3,CLAP,AU}/`.
The first configure clones ysfx and JUCE over git. GitHub release tarballs are deliberately
not used. A full build takes about 3–4 minutes on 4 cores.

### Dependencies per OS (CI)

**Linux (ubuntu-22.04 recommended; the binary needs the build machine's glibc or newer)**
```bash
sudo apt-get update && sudo apt-get install -y \
  build-essential cmake ninja-build git pkg-config \
  libasound2-dev libfreetype-dev libfontconfig1-dev libgl1-mesa-dev \
  libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev
# for --test / GUI smoke test only:  xvfb imagemagick
```
WebKit, GTK and curl are **not** needed (`JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0`).
At runtime the plugin links libfreetype6, libfontconfig1 and libstdc++.

**macOS (macos-14 / Xcode 15+)**: `brew install cmake ninja`. `build.sh` builds a universal binary
(`CMAKE_OSX_ARCHITECTURES="arm64;x86_64"`, deployment target 10.13). Code signing and notarization are not covered.

**Windows (windows-2022, MSVC 2022, `shell: bash`)**: `choco install nasm -y` (EEL2's x64 code).
`build.sh` also finds `C:\Program Files\NASM\nasm.exe` when it is not on PATH. It uses
"Visual Studio 17 2022" x64 and the static CRT.

### GitHub Actions sketch

```yaml
strategy:
  matrix:
    os: [ubuntu-22.04, macos-14, windows-2022]
runs-on: ${{ matrix.os }}
defaults: { run: { shell: bash } }
steps:
  - uses: actions/checkout@v4
  - if: runner.os == 'Linux'
    run: sudo apt-get update && sudo apt-get install -y ninja-build libasound2-dev libfreetype-dev libfontconfig1-dev libgl1-mesa-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev
  - if: runner.os == 'macOS'
    run: brew install ninja
  - if: runner.os == 'Windows'
    run: choco install nasm -y
  - run: prototypes/tilt/ysfx/build.sh
  - uses: actions/upload-artifact@v4
    with: { name: null-tilt-ys-${{ runner.os }}, path: prototypes/tilt/ysfx/dist }
```

## Tests (Linux)

```bash
./build.sh --test   # needs tests/.build/ysfx (from tests/run.sh) for the reference renderer
test/compare.sh build ../gfx/null_tilt_gfx.jsfx            # CLAP plugin vs ysfx lib, Amount -100..100
xvfb-run -a -s "-screen 0 1024x768x24" build/clap_gui "dist/linux/NULL Tilt YS.clap" gui.png
```
- `test/clap_render.cpp` is a minimal CLAP host. It sets "Amount" via `text_to_value` plus a param
  event and renders a fixed test signal.
- `test/ysfx_render.cpp` renders the same signal through `tests/.build/ysfx` directly.
- External validators: [clap-validator](https://github.com/free-audio/clap-validator) 0.4.1 and the
  Steinberg `validator` from vst3sdk v3.8.0. Build the validator with VSTGUI off and remove
  `public.sdk/samples/vst-hosting/{editorhost,audiohost}` so it doesn't need GTK.
