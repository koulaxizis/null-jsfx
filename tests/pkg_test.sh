#!/bin/bash
# Checks the macOS installer package made by tools/build_installer.py: installs everything (with a
# preset file of the user's own already there), then a few parts only, checks the files, the FX
# chains (only with their plugins) and the presets, and removes it all with the uninstaller.
#   tests/pkg_test.sh PARTS [PKG]
# PARTS is the folder of the .pkg parts (build_installer.py --pkg-root, or <out>/pkg-build).
# With PKG (on a Mac) it installs the real package into this user's home with `installer`;
# without, it copies the parts into a scratch home and runs installer/macos/postinstall itself,
# as Installer does (so the layout and the scripts are checked on Linux too).
# Used by .github/workflows/native.yml.
set -eu
parts="$(cd "$1" && pwd)"
pkg="${2:-}"
[ -z "$pkg" ] || pkg="$(cd "$(dirname "$pkg")" && pwd)/$(basename "$pkg")"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

fail() { echo "FAIL: $*" >&2; exit 1; }
count() { if [ -d "$1" ]; then find "$1" -maxdepth 1 -name "$2" | wc -l | tr -d ' '; else echo 0; fi; }
expect() { [ "$2" = "$3" ] || fail "$1: expected $3, got $2"; echo "ok   $1: $2"; }

R='Library/Application Support/REAPER'
N='Library/Application Support/NULL JSFX'
V='Library/Audio/Plug-Ins/VST3/NULL JSFX'
C='Library/Audio/Plug-Ins/CLAP/NULL JSFX'
ALL='base jsfx.one-slider jsfx.essentials jsfx.advanced jsfx.instruments jsfx.midi gfx.one-slider
     gfx.essentials gfx.advanced gfx.instruments gfx.midi vst3.one-slider vst3.essentials vst3.advanced
     vst3.instruments vst3.midi vst3.rack clap.one-slider clap.essentials clap.advanced clap.instruments
     clap.midi clap.rack remote fonts chains'
plugins=$(cat "$parts"/roots/jsfx.*/"$R"/Effects/null_jsfx/*.jsfx | grep -c '^desc:')
natives=$(find "$parts"/roots/vst3.*/"$V" -maxdepth 1 -name 'null_jsfx_*.vst3' ! -name '*rack*' | wc -l | tr -d ' ')
echo "package: $plugins plugins, $natives with VST3/CLAP"

install() {  # install HOME PART...: those parts and not the rest; "default": as the package picks
  h="$1"; shift
  # shellcheck disable=SC2086
  want=" $(echo $ALL | sed 's/ remote / /') "  # the remote is off unless picked
  [ "$*" = default ] || want=" base $* "
  if [ -n "$pkg" ] && [ "$*" = default ]; then
    installer -pkg "$pkg" -target CurrentUserHomeDirectory > "$work/installer.log" 2>&1 ||
      { cat "$work/installer.log"; fail "installer"; }
  elif [ -n "$pkg" ]; then
    {
      echo '<?xml version="1.0" encoding="UTF-8"?>'
      echo '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">'
      echo '<plist version="1.0"><array>'
      for p in $ALL; do
        [ "$p" = base ] && continue
        case "$want" in *" $p "*) on=1 ;; *) on=0 ;; esac
        echo "<dict><key>choiceIdentifier</key><string>$p</string><key>choiceAttribute</key><string>selected</string>"
        echo "<key>attributeSetting</key><integer>$on</integer></dict>"
      done
      echo '</array></plist>'
    } > "$work/choices.xml"
    installer -pkg "$pkg" -target CurrentUserHomeDirectory -applyChoiceChangesXML "$work/choices.xml" > "$work/installer.log" 2>&1 ||
      { cat "$work/installer.log"; fail "installer"; }
  else
    for p in $ALL; do
      case "$want" in *" $p "*) ;; *) continue ;; esac
      cp -R "$parts/roots/$p/." "$h/"
      DSTROOT="$h" HOME="$h" bash "$parts/scripts/postinstall" "$parts" "$h" "$h" || fail "postinstall $p"
    done
  fi
}

uninstall() {
  echo y | HOME="$1" bash "$1/$N/Uninstall NULL JSFX.command" > "$work/uninstall.log" || { cat "$work/uninstall.log"; fail "uninstall"; }
  for d in "$R/Effects/null_jsfx" "$R/FXChains/NULL JSFX" "$R/FXChains/NULL JSFX GFX" "$V" "$C" "$N" \
           "$R/reaper_www_root/null_jsfx.html" "$R/Scripts/null_jsfx_remote_bridge.lua"; do
    [ ! -e "$1/$d" ] || fail "left after uninstalling: $d"
  done
  if [ -n "$pkg" ] && pkgutil --volume "$1" --pkgs='tech\.nulljsfx\.pkg\..*' > /dev/null 2>&1; then
    fail "pkgutil still knows the parts"
  fi
  echo "ok   uninstalled"
}

if [ -n "$pkg" ]; then
  h="$HOME"
  for d in "$R/Effects/null_jsfx" "$V" "$C" "$N"; do [ ! -e "$h/$d" ] || fail "$h/$d is there before installing"; done
else
  h="$work/home"
  mkdir -p "$h"
fi

# a preset file the user already has: theirs stay first, ours come after, and they stay on uninstall
ini='js-null_jsfx_tilt_jsfx.ini'
mkdir -p "$h/$R/presets"
printf '[General]\nNbPresets=1\n\n[Preset0]\nData=1 2 3\nLen=6\nName=My Own Tilt\n\n' > "$h/$R/presets/$ini"

install "$h" default
expect "all: JSFX" "$(count "$h/$R/Effects/null_jsfx" '*.jsfx')" "$plugins"
expect "all: GFX" "$(count "$h/$R/Effects/null_jsfx/gfx" '*_gfx.jsfx')" "$plugins"
expect "all: preset files" "$(count "$h/$R/presets" 'js-null_jsfx_*.ini')" "$((plugins * 2))"
expect "all: VST3 (with Rack)" "$(count "$h/$V" '*.vst3')" "$((natives + 1))"
expect "all: CLAP (with Rack)" "$(count "$h/$C" '*.clap')" "$((natives + 1))"
[ -f "$h/$R/Effects/null_jsfx/gfx/null_gfx.jsfx-inc" ] || fail "all: null_gfx.jsfx-inc missing"
[ ! -e "$h/$R/reaper_www_root/null_jsfx.html" ] || fail "all: the remote is off unless picked"
[ -f "$h/Library/Fonts/NotoSans-Regular.ttf" ] || fail "all: font missing"
[ ! -e "$h/$N/staging" ] || fail "all: staging folder left"
chains=$(count "$h/$R/FXChains/NULL JSFX" '*.RfxChain')
[ "$chains" -gt 20 ] || fail "all: only $chains FX chains"
expect "all: GFX chains" "$(count "$h/$R/FXChains/NULL JSFX GFX" '*.RfxChain')" "$chains"
head -7 "$h/$R/presets/$ini" | grep -q '^Name=My Own Tilt$' || fail "all: the user's preset is not first"
grep -q '^NbPresets=1$' "$h/$R/presets/$ini" && fail "all: no factory presets added to $ini"
[ "$(grep -c '^Name=' "$h/$R/presets/$ini")" -gt 2 ] || fail "all: factory presets missing in $ini"
if [ -n "$pkg" ]; then
  mine=$(find "$h/$R/Effects/null_jsfx" "$h/$V" "$h/$N" "$h/$R/presets" ! -user "$(id -u)" | head -3)
  [ -z "$mine" ] || fail "not owned by the user: $mine"
  echo "ok   all: owned by the user"
fi
uninstall "$h"
grep -q '^Name=My Own Tilt$' "$h/$R/presets/$ini" || fail "uninstall removed the user's preset"

# a few parts: JSFX One Slider, VST3 Essentials, the remote and the chains
rm -rf "$h/$R/presets"
install "$h" jsfx.one-slider vst3.essentials remote chains
ones=$(count "$h/$R/Effects/null_jsfx" '*.jsfx')
[ "$ones" -gt 0 ] && [ "$ones" -lt "$plugins" ] || fail "some: $ones JSFX"
echo "ok   some: $ones JSFX (One Slider)"
expect "some: GFX" "$(count "$h/$R/Effects/null_jsfx/gfx" '*')" 0
expect "some: CLAP" "$(count "$h/$C" '*.clap')" 0
[ "$(count "$h/$V" '*.vst3')" -gt 0 ] || fail "some: no VST3"
[ ! -e "$h/$V/null_jsfx_rack.vst3" ] || fail "some: Rack not picked"
[ -f "$h/$R/reaper_www_root/null_jsfx.html" ] || fail "some: remote page missing"
expect "some: GFX chains" "$(count "$h/$R/FXChains/NULL JSFX GFX" '*.RfxChain')" 0
some=$(count "$h/$R/FXChains/NULL JSFX" '*.RfxChain')
[ "$some" -lt "$chains" ] || fail "some: $some FX chains, all of them"
# each chain's plugins are there
for f in "$h/$R/FXChains/NULL JSFX/"*.RfxChain; do
  [ -f "$f" ] || continue
  for u in $(sed -n 's|^<JS null_jsfx/\([a-z0-9_]*\)\.jsfx.*|\1|p' "$f"); do
    [ -f "$h/$R/Effects/null_jsfx/$u.jsfx" ] || fail "some: $(basename "$f") without $u"
  done
done
echo "ok   some: $some FX chains, each with its plugins"
uninstall "$h"
echo "pkg test passed"
