# CLAUDE.md — working on NULL JSFX

Instructions for AI assistants working in this repository. The owner is Christos Koulaxizis.

## Read the Bible first
`NULL_JSFX_BIBLE.md` is the single source of truth for this project and its changelog. At the start of every session read its Part 0 (working agreement), Part I (rules R1–R26) and Part VIII (open items) before you touch anything. Every delivery updates it: a changelog entry in Part X and the delta in its own Part.

Never guess the contents of a file. Read it from the repo, or ask for it.

## Hard rules in short (the Bible has the full text)
- Replies to the owner are in Greek. Code, comments, docs, the Bible and changelogs are in English.
- Design first. Ask open questions in one numbered list, offer improvements as numbered proposals, and raise risks before you proceed. Touch only what the task needs.
- One sound, four forms: plain JSFX, GFX JSFX, VST3/CLAP and the Rack module sound identical (GFX tolerance 0, native 1e-5, web 1e-6).
- Lines 1–5 of every `.jsfx` follow `tests/lint.py`. A One Slider plugin has exactly one slider.
- GFX never sets a slider through `slider()` (it uses `ng_slider_set`), the accent is silver and never the site purple, and `null_gfx.jsfx-inc` stays in step with `NullUI.hpp` and `NullTheme.hpp`.
- Native code computes in double in the EEL2 order, allocates in `setSampleRate` and never in `process`, and implements `rand()` as an exact Park-Miller step.
- Rack parameter indices are fixed forever, new features are silent at their default, state strings never change meaning, and `RackEngineV1.hpp` is frozen.
- Released plugin identities (name, unique id, CLAP id, URI) never change.
- Generated files are never edited by hand (Bible Part II.9). Run the generators and tests the change touches (Bible R15, Part V).
- The version changes everywhere at once (Bible Part VI.1); `tests/lint.py` fails if `llms.txt` or `index.html` shows another version than `FOOTER`. Releases are published by the owner.
- Free forever: no paywalls, licence keys, ads or nags. Support and news go through Ko-fi only.

## Every delivery ends with
1. What changed, and why.
2. How it was verified: what was actually run and what was only read.
3. A changelog entry in the Bible's Part X, plus the Bible delta.
