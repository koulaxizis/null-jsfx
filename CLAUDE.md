# CLAUDE.md — working on NULL JSFX

Instructions for AI assistants working in this repository. The owner is Christos Koulaxizis.

## Sources of truth
- `NULL_JSFX_BIBLE.md` (once it exists) is the project handbook and changelog. Read it before changing anything.
- Until then, and alongside it: `README.md`, `native/README.md`, `web/README.md`, `hub/README.md`, `tests/lint.py` and `.github/workflows/`.
- Never guess the contents of a file. Read it, or ask for it.

## Working agreement
- Replies to the owner are in Greek. Code, comments, docs and changelogs are in English.
- Design first. Discuss and agree on the idea before writing code.
- Ask all open questions together, in one numbered list.
- Offer improvements as numbered proposals that can be approved or rejected one by one.
- If you disagree with something or see a risk, say so before you proceed.
- Changes reach the owner as pull requests. In chat, use OLD -> NEW patches: OLD byte-exact with exactly one match, NEW in its own fenced block. A set of 10 or more blocks is delivered as the whole file.
- Touch only what the task needs.

## Hard rules
- One sound, four forms. Plain JSFX, GFX JSFX, VST3/CLAP and the Rack module must sound identical.
  - GFX audio code matches the plain JSFX exactly (tolerance 0).
  - Native ports match within 1e-5. Exceptions go in `compare_args.txt`, with the reason.
  - The web runtime matches within 1e-6.
- Lines 1–5 of every `.jsfx` file follow `tests/lint.py` exactly (desc, author, tags, `//category:`, footer).
- A One Slider plugin has exactly one slider.
- GFX code never calls `slider()`. It uses `ng_slider_set`. `null_gfx.jsfx-inc` stays in step with `native/common/NullUI.hpp` and `NullTheme.hpp`.
- The GFX look uses a silver accent, never the site purple.
- Native DSP:
  - Compute in double, in the same order of operations as the EEL2.
  - Allocate in `setSampleRate`, never in `process`.
  - `rand()` is an exact Park-Miller step.
- Rack:
  - Parameter indices are fixed forever; new parameters are appended.
  - A new feature is silent at its default.
  - State strings never change meaning.
  - `RackEngineV1.hpp` is frozen.
- Generated files are never edited by hand. These include `web/fx/*.js`, `plugins/<name>/` pages, the plugin lists in `README.md` and `llms.txt`, `.rpl` files and `FactoryPresets.hpp`. Edit the source, then regenerate.
- Free forever:
  - Nothing released for free moves behind a paywall.
  - No licence keys, ads or nags.
  - News goes out on Ko-fi only.

## After a change
- Plugin added, or sliders changed: run `python3 tools/build_site.py`. CI runs `--check`.
- Anything in `DATA/` changed: run `node web/tools/build.js`.
- Presets changed: run `tools/build_presets.py`.
- Run the tests the change touches:
  - `tests/run.sh`
  - `native/scripts/build.sh --test`
  - `web/test/run.sh`
  - the Hub tests
  - `tests/remote/remote_test.cjs`

## Versions
- The version appears in every `.jsfx` footer, in `FOOTER` in `tests/lint.py`, in the `Version:` line of `llms.txt` and on the site. Change them all together. `tests/lint.py` fails if the `llms.txt` or `index.html` version differs from `FOOTER`.
- Propose a small (patch) or a big step. The owner decides.
- Publishing a GitHub release with a numeric tag (for example `2.6`) builds and attaches every package.

## Every delivery ends with
1. What changed, and why.
2. How it was verified. Say what was actually run and what was only read.
3. A changelog entry, in English (in the Bible once it exists).
