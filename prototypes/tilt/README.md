# NULL Tilt prototypes

The same plugin (NULL Tilt, `DATA/Effects/null_jsfx/tilt.jsfx`) in three forms, to decide how the
suite goes beyond native-slider JSFX. All three produce output bit-identical to the JSFX.

| Folder | Name | Format | Interface |
|---|---|---|---|
| `gfx/` | NULL Tilt GFX | JSFX (REAPER) | custom `@gfx` knob and response curve |
| `ysfx/` | NULL Tilt YS | VST3, CLAP (+AU on macOS) | the GFX JSFX embedded in a [ysfx](https://github.com/JoepVanlier/ysfx)-based plugin |
| `dpf/` | NULL Tilt Native | VST3, CLAP | C++ rewrite with [DPF](https://github.com/DISTRHO/DPF), NanoVG interface |

`.github/workflows/prototypes.yml` builds all three for Windows, macOS and Linux and uploads one
`NULL-Tilt-prototypes-<os>` artifact per OS (see `INSTALL.txt`). Each folder's README covers
building it locally.
