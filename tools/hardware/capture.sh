#!/usr/bin/env bash
# The inputs of the 3D hardware renders (Bible II.8): for each plugin, its VST3/CLAP interface
# at 2x (<out>/<name>.png) and the controls it drew (<out>/<name>.json, from layout.py). Builds
# the JACK standalones once, runs each under Xvfb with a dummy JACK server and NULL_UI_LAYOUT set
# (native/common/NullUI.hpp writes what it drew), as native/scripts/screenshot.sh does.
#
#   tools/hardware/capture.sh <out dir> [plugin ...]     (default: every plugin but the Rack)
#
# Needs: xvfb jackd2 xdotool imagemagick, plus the usual native build dependencies.
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
out="$(realpath -m "$1")"; shift
mkdir -p "$out"
if [ $# -gt 0 ]; then plugins=("$@"); else
  mapfile -t plugins < <(cd "$root/native/plugins" && ls -d */ | tr -d / | grep -vx rack)
fi
build="$root/native/build-hardware"
list="$(IFS=';'; echo "${plugins[*]}")"
cmake -S "$root/native" -B "$build" -G Ninja -DNULL_JACK=ON -DNULL_VST3=OFF -DNULL_CLAP=OFF -DNULL_LV2=OFF -DNULL_PLUGINS="$list" >/dev/null
cmake --build "$build" >/dev/null

disp=":$(( 90 + RANDOM % 100 ))"
Xvfb "$disp" -screen 0 2400x2400x24 >/dev/null 2>&1 & xvfb=$!
export DISPLAY="$disp"
sleep 1
jackd -n "hw$$" -d dummy -r 48000 >/dev/null 2>&1 & jack=$!
export JACK_DEFAULT_SERVER="hw$$"
trap 'kill $jack $xvfb 2>/dev/null || true' EXIT
sleep 1
for p in "${plugins[@]}"; do
  info="$root/native/plugins/$p/DistrhoPluginInfo.h"
  uw=$(awk '/DISTRHO_UI_DEFAULT_WIDTH/{print $3}' "$info"); uh=$(awk '/DISTRHO_UI_DEFAULT_HEIGHT/{print $3}' "$info")
  raw="$out/$p.raw.json"; rm -f "$raw"
  NULL_UI_LAYOUT="$raw" "$build/bin/null_jsfx_$p" >/dev/null 2>&1 & app=$!
  win=""
  for _ in $(seq 60); do
    win=$(xdotool search --pid "$app" 2>/dev/null | tail -1 || true)
    [ -n "$win" ] && break
    sleep 0.3
  done
  if [ -z "$win" ]; then echo "$p: no window" >&2; kill $app 2>/dev/null || true; continue; fi
  xdotool windowmove "$win" 0 0 windowsize "$win" $(( uw * 2 )) $(( uh * 2 ))
  sleep 1.5
  import -window "$win" "$out/$p.png"
  kill $app 2>/dev/null || true; wait $app 2>/dev/null || true
  python3 "$root/tools/hardware/layout.py" "$raw" "$out/$p.json" "$p" && rm -f "$raw"
  echo "$p"
done
