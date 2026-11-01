# NULL JSFX Android remotes

One shell, two apps (Christos, 2026-11-01; the DAW app is one app for every DAW, "Remote for DAWs"):

| App | applicationId | Tab code | Release asset |
|---|---|---|---|
| Remote for DAWs | `tech.nulljsfx.remote.daws` | `web/daws/` (REAPER; Ardour/Mixbus and Bitwig on the shared desk `daws/core/`) | `NULL-JSFX-Remote-DAWs-Android.apk` |
| Remote for Mixers | `tech.nulljsfx.remote.mixers` | `web/mixers/` | `NULL-JSFX-Remote-Mixers-Android.apk` |

The application ids never change once released (Bible R3). Each app comes in two store
flavors: `github` (the APK on our releases, also what Obtainium installs; tells the user about
a new version) and `fdroid` (built and updated by F-Droid and Droidify; no update notice).
Variants are `<app><Store>`, e.g. `dawsGithubRelease`.

## How it works

- `app/` (Kotlin): one Activity with one WebView. The pages are the app's assets, served at
  `https://appassets.androidplatform.net/app/` (`WebViewAssetLoader`).
- `web/core/`: the start page, the tabs (`remote.js`), Settings, the look (dark, silver
  `#c4c4c4`, Noto Sans, the NULL JSFX header) and `shell.js`, which wraps the phone's side as
  `window.NullShell` (the API is listed at the top of the file).
- **REAPER.** A request a page makes to `/_/...` on its own origin is forwarded by the app to
  the REAPER web interface the user connected to (`MainActivity.reaper`, with HTTP Basic auth
  when a user name is set). So the tablet page `remote/reaper_www_root/null_jsfx.html` runs in
  the app unchanged: the build copies it into the assets as `daws/reaper/null_jsfx.html`, with the
  fonts from `native/common/fonts` (task `sharedAssets`); neither is committed twice.
  `Find REAPER` probes `/_/TRANSPORT` on the phone's /24 (`Shell.scan`).
- **Mixers.** `NullShell.udpOpen/udpSend/udpBroadcast/udpClose` with base64 packets; received
  packets arrive as `{type: "udp", id, host, port, data}` (`NullShell.on('udp', fn)`), through a
  WebMessagePort the app opens on every page load.
- **Version.** `versionName` is the suite version, read from `FOOTER` in `tests/lint.py`, and
  `versionCode` comes from it (3.1 → 30100), so a version step needs no change here.

## Build and test

```
cd android && ./gradlew testDawsGithubDebugUnitTest assembleDebug      # needs the Android SDK (API 35)
node tests/remote/android_test.cjs                                       # the pages, in Chromium
```

CI: `.github/workflows/android.yml` (pages, unit tests, lint, every debug APK as an artifact;
on a published release the signed APKs of `RELEASED_APPS`).

## Signing

Release APKs are signed with the orOS keystore (Christos, 2026-11-01), given to the build only
through the environment: `NULL_REMOTE_KEYSTORE` (base64 of the `.jks`),
`NULL_REMOTE_KEYSTORE_PASSWORD`, `NULL_REMOTE_KEY_ALIAS`, `NULL_REMOTE_KEY_PASSWORD`, which the
workflow fills from the repository secrets `ANDROID_KEYSTORE`, `ANDROID_KEYSTORE_PASSWORD`,
`ANDROID_KEY_ALIAS` and `ANDROID_KEY_PASSWORD`. No key, password or keystore file is ever
committed (`.gitignore` blocks `*.jks` and `*.keystore`).

## F-Droid

`fdroid/tech.nulljsfx.remote.daws.yml` is a draft recipe for a merge request to fdroiddata;
the store texts are in `app/src/daws/fastlane/metadata/android/en-US/`. F-Droid builds the
`fdroid` flavor from the release tag and signs it with its own key unless the build is shown to
be reproducible, so users cannot switch between the F-Droid and GitHub APKs without
reinstalling.
