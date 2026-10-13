#!/usr/bin/env bash
# Checks a Linux or macOS installer made by tools/build_installer.py: installs everything, a
# subset of formats and plugins, and FX chains only with their plugins, each into a scratch home,
# and removes it all again with --uninstall. Used by .github/workflows/native.yml.
#   tests/installer_test.sh build-installer/NULL-JSFX-Installer-Linux.sh
set -eu
installer="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
unset XDG_CONFIG_HOME XDG_DATA_HOME

fail() { echo "FAIL: $*" >&2; exit 1; }
count() { if [ -d "$1" ]; then find "$1" -maxdepth 1 -name "$2" | wc -l | tr -d ' '; else echo 0; fi; }
expect() { [ "$2" = "$3" ] || fail "$1: expected $3, got $2"; echo "ok   $1: $2"; }

if [ "$(uname -s)" = Darwin ]; then
  reaper='Library/Application Support/REAPER' vst3='Library/Audio/Plug-Ins/VST3' clap='Library/Audio/Plug-Ins/CLAP'
else
  reaper='.config/REAPER' vst3='.vst3' clap='.clap'
fi

run() {  # run HOME-NAME ARGS...: the installer with no terminal, in its own scratch home
  h="$work/$1"; shift
  mkdir -p "$h"
  HOME="$h" bash "$installer" "$@" < /dev/null > "$h.log" 2>&1 || { cat "$h.log"; fail "installer $*"; }
}

# what the package holds
plugins=$(bash "$installer" --list | grep -c '^  ')
[ "$plugins" -gt 80 ] || fail "only $plugins plugins listed"
one=$(bash "$installer" --list | awk '/^One Slider:/{f=1;next} /^[A-Z]/{f=0} f' | wc -l | tr -d ' ')
echo "package: $plugins plugins, $one One Slider"

# everything
run all --all --yes
h="$work/all"
expect "all: JSFX" "$(count "$h/$reaper/Effects/null_jsfx" '*.jsfx')" "$plugins"
expect "all: GFX" "$(count "$h/$reaper/Effects/null_jsfx/gfx" '*_gfx.jsfx')" "$plugins"
expect "all: preset files (JSFX + GFX)" "$(count "$h/$reaper/presets" 'js-null_jsfx_*.ini')" "$((plugins * 2))"
expect "all: VST3 (with Rack)" "$(count "$h/$vst3/NULL JSFX" '*.vst3')" "$((plugins + 1))"
expect "all: CLAP (with Rack)" "$(count "$h/$clap/NULL JSFX" '*.clap')" "$((plugins + 1))"
[ -f "$h/$reaper/Effects/null_jsfx/gfx/null_gfx.jsfx-inc" ] || fail "all: null_gfx.jsfx-inc missing"
[ -f "$h/$reaper/reaper_www_root/null_jsfx.html" ] || fail "all: remote page missing"
[ -f "$h/$reaper/Scripts/null_jsfx_remote_bridge.lua" ] || fail "all: remote bridge missing"
chains=$(count "$h/$reaper/FXChains/NULL JSFX" '*.RfxChain')
expect "all: GFX chains" "$(count "$h/$reaper/FXChains/NULL JSFX GFX" '*.RfxChain')" "$chains"
[ "$chains" -gt 20 ] || fail "all: only $chains FX chains"
cmp -s "$h/$reaper/Effects/null_jsfx/tilt.jsfx" "$(dirname "$0")/../DATA/Effects/null_jsfx/tilt.jsfx" ||
  fail "all: tilt.jsfx differs from DATA"

HOME="$h" bash "$installer" --uninstall > /dev/null
# fc-cache keeps its own cache in ~/.cache after the fonts are installed; that is not ours
find "$h" -type f ! -path "$h/.cache/*" | sed "s|^$h/|left: |"
left=$(find "$h" -type f ! -path "$h/.cache/*" | wc -l | tr -d ' ')
expect "all: files left after --uninstall" "$left" 0

# presets the user saved stay, ours are added after them, and they stay after --uninstall
h="$work/merge" ini="js-null_jsfx_tilt_jsfx.ini"
mkdir -p "$h/$reaper/presets"
printf '[General]\nNbPresets=1\n\n[Preset0]\nData=31202D2074657374E5\nLen=8\nName=Mine\n' > "$h/$reaper/presets/$ini"
run merge --formats jsfx --plugins tilt --extras none --yes
names=$(grep '^Name=' "$h/$reaper/presets/$ini" | tr '\n' '|')
case "$names" in "Name=Mine|Name=Default|"*) ;; *) fail "merge: got $names" ;; esac
expect "merge: NbPresets" "$(sed -n 's/^NbPresets=//p' "$h/$reaper/presets/$ini")" "$(grep -c '^Name=' "$h/$reaper/presets/$ini")"
run merge --formats jsfx --plugins tilt --extras none --yes
expect "merge again: no duplicates" "$(grep '^Name=' "$h/$reaper/presets/$ini" | sort | uniq -d | wc -l | tr -d ' ')" 0
HOME="$h" bash "$installer" --uninstall > /dev/null
grep -q '^Name=Mine$' "$h/$reaper/presets/$ini" || fail "merge: --uninstall removed the user's presets"
echo "ok   merge: the user's presets kept"

# a subset: CLAP only, One Slider plus Delay; chains need JSFX or GFX so none come
run some --formats clap --groups one-slider --plugins delay --extras chains,fonts --yes
h="$work/some"
expect "some: CLAP" "$(count "$h/$clap/NULL JSFX" '*.clap')" "$((one + 1))"
expect "some: VST3" "$(count "$h/$vst3/NULL JSFX" '*.vst3')" 0
expect "some: JSFX" "$(count "$h/$reaper/Effects/null_jsfx" '*.jsfx')" 0
expect "some: chains" "$(count "$h/$reaper/FXChains/NULL JSFX" '*.RfxChain')" 0
[ -e "$h/$clap/NULL JSFX/null_jsfx_delay.clap" ] || fail "some: Delay missing"
[ -e "$h/$clap/NULL JSFX/null_jsfx_rack.clap" ] && fail "some: Rack was not asked for"

# FX chains come only with every plugin they use
run chains --formats jsfx --groups essentials --extras chains --yes
h="$work/chains"
n=0
for c in "$h/$reaper/FXChains/NULL JSFX/"*.RfxChain; do
  [ -f "$c" ] || continue
  n=$((n + 1))
  for js in $(sed -n 's|^<JS null_jsfx/\([a-z_0-9]*\)\.jsfx.*|\1|p' "$c"); do
    [ -f "$h/$reaper/Effects/null_jsfx/$js.jsfx" ] || fail "chains: $(basename "$c") needs $js, not installed"
  done
done
[ "$n" -gt 0 ] && [ "$n" -lt "$chains" ] || fail "chains: $n of $chains chains for Essentials only"
echo "ok   chains: $n of $chains, each with its plugins"

# mistakes stop it before anything is written
if HOME="$work/bad" bash "$installer" --plugins no_such_plugin --yes < /dev/null > /dev/null 2>&1; then
  fail "an unknown plugin was accepted"
fi
[ ! -e "$work/bad" ] || [ -z "$(find "$work/bad" -type f)" ] || fail "bad: wrote files"
echo "ok   unknown plugin refused"
echo "installer OK"
