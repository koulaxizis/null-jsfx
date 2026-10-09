# NULL Tilt prototypes

NULL Tilt (`DATA/Effects/null_jsfx/tilt.jsfx`) as the template for the suite's extra formats. Both
produce output bit-identical to the JSFX and share one look: the nulljsfx.tech dark theme with a
silver accent.

| Folder | Name | Format | Interface |
|---|---|---|---|
| `gfx/` | NULL Tilt GFX | JSFX (REAPER) | `@gfx` knob and response curve, drawn by the shared `null_gfx.jsfx-inc` |
| `dpf/` | NULL Tilt Native | VST3, CLAP | C++ rewrite with [DPF](https://github.com/DISTRHO/DPF), NanoVG interface |

The palette lives in two places that must match: `gfx/null_gfx.jsfx-inc` (`NG_*`) and
`dpf/plugin/Theme.hpp`.

`.github/workflows/prototypes.yml` builds both for Windows, macOS and Linux and uploads one
`NULL-Tilt-prototypes-<os>` artifact per OS (see `INSTALL.txt`). The ysfx-wrapped version was tried
and dropped (2026-10-09): the native build is smaller and faster for the same sound.
