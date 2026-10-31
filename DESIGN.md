# DESIGN.md — NULL JSFX

Design language for NULL JSFX in every form: GFX JSFX, VST3/CLAP (DPF), the Rack, the Hub app, the web plugins in orOS Studio, and nulljsfx.tech. Source of truth: NULL_JSFX_BIBLE.md (R8 "The look", II.8 site). Where this file and the Bible differ, the Bible wins and this file gets fixed.

## Identity

- Free, MIT audio plugins. Motto: **AUDIO WITHOUT NOISE** (NULL JSFX only, never a personal motto).
- Minimal, technical, quiet. One idea per plugin; the display shows what the plugin does.
- In DAWs: the bare plugin name, maker "NULL JSFX". JSFX descriptions keep the "NULL " prefix.

## Plugin UI (GFX, VST3, CLAP, Rack, Hub)

- Dark only.
- Accent: silver `#c4c4c4`. Never purple: purple is Noxpress and no visual association is wanted.
- Header: logo, "NULL JSFX", plugin name, category, "AUDIO WITHOUT NOISE".
- One Slider plugins: one horizontal slider, `@gfx 360 240`.
- Other plugins: knobs and selectors in rows of 6, tabs beyond 2 rows; 480 wide (height 294–468); instruments and meters wider.
- Every display draws something real: a curve, meters or a scope.
- Windows resize.
- Font: Noto Sans everywhere (vendored, OFL), including the native and web versions.
- `null_gfx.jsfx-inc` stays in step with `native/common/NullUI.hpp` and `NullTheme.hpp`: a change to the look lands in all three.
- Keyboard fine-tuning and double-click reset on every control.

## Site (nulljsfx.tech)

- Dark default with a light option. Tokens (assets/css/style.css): `--bg #1b1a18`, `--bg-secondary #131211`, `--text #e0e0e0`, `--muted #999999`, `--border #333333`, `--card-bg #232220`.
- Accent: silver, the same as the plugins (decision of Christos, 2026-10-31). Purple stays Noxpress only. Dark theme `--accent #c4c4c4`; light theme `--accent #555555` (dark silver, because `#c4c4c4` on white is 1.7:1 and fails WCAG AA). `--accent-ring` is the accent at 15% for focus rings.
- Font: `'Segoe UI', system-ui, sans-serif`; monospace stack for code. If Noto Sans is wanted on the site too, it is self-hosted.
- Tabs Effects / Instruments / MIDI / FX Chains; generated plugin cards and pages (never hand-edit generated blocks).
- Real screenshots of the GFX (assets/img/gfx/), never mock-ups.

## Video and demos

- Presentation videos: English only, subtitles, no voice, the NULL JSFX loop as music.
- Audio demos from dry stems (10–15″ WAV 24-bit) supplied by Christos.

## Do / don't

- Do: silver on near-black, thin precise lines, readable labels at small sizes, honest meters.
- Don't: purple in plugins, skeuomorphic textures, glow-heavy "gamer" styling, ads or nags, external graphics libraries in JSFX.
