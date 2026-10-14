# NULL JSFX Hub

One place to install, update and remove the NULL JSFX plugins, in any format (JSFX, JSFX GFX,
VST3, CLAP), on Windows, Linux and macOS. It reads what the latest GitHub release holds,
shows every plugin with its screenshot and guide text, and tells which installed plugins have a
newer version. Downloads come only from our GitHub releases.

Free and MIT like the plugins: no account, no tracking. To look for a new release it downloads
`NULL-JSFX-Catalog.json` from the latest release and sends nothing about the user.

## How it works

| Part | What |
|---|---|
| `tools/build_catalog.py` | Writes `NULL-JSFX-Catalog.json` for a release: plugins (from the .jsfx headers and `tools/site_content.json`), FX chains, extras, package names, release notes. CI attaches it to every release. |
| `hub/core` | The engine, no window: fetches the catalog, finds what is installed (its own records in `hub-installed.json`, else the JSFX footer version or the installer's version), installs, updates and removes. |
| `hub/app` | The Tauri 2 app: hands the engine to the page and reports progress. |
| `hub/ui` | The page: plain HTML, CSS and JS, the plugins' dark palette with the silver accent. |

Things worth knowing:

- **Only what is needed is downloaded.** The VST3/CLAP packages are 200 to 470 MB, so the core
  reads the zip's directory over HTTP range requests and fetches just the plugins asked for
  (`package::RemoteZip`). When a request needs more than half the package (say, Install all), it
  downloads the whole zip once and keeps it until the next release.
- **Same folders as the installers** (`installer/`): REAPER's resource folder for JSFX and GFX,
  `NULL JSFX` inside the VST3 and CLAP folders. On Windows those are in Program Files, so the Hub
  runs the copy steps that need it in a second copy of itself with administrator rights
  (`--apply-ops`, one UAC prompt per change). The folders can be changed in Settings.
- **Versions.** A plugin counts as outdated when the release it came from is older than the
  latest one. Plugins put there by the installers or by hand are found too.
- **The GFX include** (`null_gfx.jsfx-inc`) comes with the first GFX version and goes with the
  last.
- **Extras** (`core/src/extras.rs`). FX chains follow the plugins: a chain comes once all its plugins
  are installed in that format and goes when one of them goes. Factory presets are added after the
  user's own in REAPER's preset files; when they are turned off or their plugin goes, only the
  factory presets the user left unchanged are taken out (`extras::unmerge_presets`). The tablet remote and the Noto Sans
  font come and go as files; the font is registered for the user on Windows. The Rack is a plugin
  in the catalog with only VST3 and CLAP.
- **Removing.** A plugin's card (×) and its page remove it in every format; "Remove N plugins"
  above the list removes all shown (a group, a search, Installed); Settings > Uninstall removes
  everything, extras included. Each asks first. The format toggles above the list ("Install as")
  are Settings > Formats: bulk install adds the picked formats that a shown plugin is missing,
  so a whole group can get its VST3 version after its JSFX one.
- **A running DAW** (`core/src/hosts.rs`). Before VST3/CLAP files change, the page asks the user
  to close the DAWs it finds running (REAPER, Live, FL Studio, Bitwig, Cubase and others): Windows
  can't replace a loaded plugin file, and the others load the new one only after a restart.
- **Check and repair** (`Hub::verify`, Settings). Compares every installed plugin file that is
  at the current release with the CRC-32 and size in the release zip's directory (read over range
  requests, no download), and reinstalls the plugins that differ.
- **Audio demos.** A plugin's page plays a short mix through the plugin with one of its factory
  presets, and the same mix dry, switching between them in step. They live on the site
  (`assets/audio/demos/`, made by `tools/render_demos.py` with the real JSFX in ysfx) and the
  catalog points to them; the page's CSP allows media from nulljsfx.tech only.
- **Rolling back** (`Request::from`). A plugin's page can put it back to the release before
  (the catalog's `previous`, from the repo's tags since 2.1, the first release with these
  packages). It stays there, held in `Settings::held`, without update notices, until its page's
  Update brings it forward again.
- **Another computer** (Settings). "Save the list" writes `NULL-JSFX-Selection.json` (`Selection`:
  plugins with their formats, and the extras; no folders) to Downloads; "Install from a list" on
  the other computer installs what is missing there.
- **Updating the Hub** (`app/src/selfupdate.rs`). "Update the Hub" downloads this system's Hub
  from the release and puts it in place: the Windows setup in passive mode, the macOS app copied
  out of the disk image, the AppImage replaced. A Hub installed from the .deb shows the file.
- **Tray and notifications** (opt-in: "Start with the computer" in Settings). The Hub starts hidden
  with `--hidden`, stays in the tray, looks for a new release once a day, and shows one system
  notification per release when there are updates, new plugins or a new Hub (`Hub::notice`).

## Build and test

Needs Rust (stable), Node 22, and on Linux `libwebkit2gtk-4.1-dev librsvg2-dev libxdo-dev`.

```bash
cd hub
cargo test -p null_hub_core          # the engine, against a local release server
npm ci && node tests/ui_test.cjs     # the page in Chromium against a stand-in
tests/release_smoke.sh               # install/remove from the latest real release
npx tauri dev                        # run the app
npx tauri build                      # the installers for this OS
```

`cargo run -p null_hub_core --example hub_cli -- --root /tmp/x install tilt:all` does the same
work without a window. `NULL_JSFX_HUB_DATA` and `NULL_JSFX_HUB_RELEASES` point the Hub at
another data folder and another release server (tests use them).

CI (`.github/workflows/hub.yml`) runs all of that on Windows, Linux and macOS, and on a release
attaches `NULL-JSFX-Hub-Windows-Setup.exe`, `NULL-JSFX-Hub-Linux.AppImage`,
`NULL-JSFX-Hub-Linux.deb`, `NULL-JSFX-Hub-macOS.dmg` and `NULL-JSFX-Catalog.json`.
