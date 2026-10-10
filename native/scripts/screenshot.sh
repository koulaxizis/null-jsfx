#!/usr/bin/env bash
# Screenshot of a plugin's VST3/CLAP interface (Linux): builds its JACK standalone, runs it under
# Xvfb with a dummy JACK server, sets the value with mouse-wheel steps (One Slider plugins only)
# and captures at 2x the default UI size.
#
#   scripts/screenshot.sh <plugin> <out.png> [value] [--dpf <dir>]
#
# Needs: xvfb jackd2 xdotool imagemagick, plus the usual build dependencies.
set -euo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
plugin="$1"; out="$(realpath -m "$2")"; value="${3:-}"
shift $(( $# < 3 ? $# : 3 ))
extra=()
while [ $# -gt 0 ]; do
  case "$1" in
    --dpf) shift; extra+=("-DFETCHCONTENT_SOURCE_DIR_DPF=$1") ;;
  esac
  shift
done
build="$here/build-shot-$plugin"
cmake -S "$here" -B "$build" -G Ninja -DNULL_JACK=ON -DNULL_VST3=OFF -DNULL_CLAP=OFF -DNULL_PLUGINS="$plugin" ${extra[@]+"${extra[@]}"} >/dev/null
cmake --build "$build" >/dev/null

disp=":$(( 90 + RANDOM % 100 ))"
Xvfb "$disp" -screen 0 1024x768x24 >/dev/null 2>&1 & xvfb=$!
export DISPLAY="$disp"
sleep 1
jackd -n "shot$$" -d dummy -r 48000 >/dev/null 2>&1 & jack=$!
export JACK_DEFAULT_SERVER="shot$$"
sleep 1
"$build/bin/null_jsfx_$plugin" >/dev/null 2>&1 & app=$!
trap 'kill $app $jack $xvfb 2>/dev/null || true' EXIT
for _ in $(seq 20); do
  win=$(xdotool search --pid "$app" 2>/dev/null | tail -1 || true)
  [ -n "$win" ] && break
  sleep 0.3
done
info="$here/plugins/$plugin/DistrhoPluginInfo.h"
uw=$(awk '/DISTRHO_UI_DEFAULT_WIDTH/{print $3}' "$info"); uh=$(awk '/DISTRHO_UI_DEFAULT_HEIGHT/{print $3}' "$info")
xdotool windowmove "$win" 0 0 windowsize "$win" $(( uw * 2 )) $(( uh * 2 ))
sleep 1
if [ -n "$value" ]; then
  # wheel steps of 1 from the default, on the slider (design 180,210 -> 2x)
  def=$(grep -o '"[A-Za-z0-9 ]*", "[a-z_0-9]*", [-0-9.]*f, [-0-9.]*f, [-0-9.]*f' "$here/plugins/$plugin/Plugin.cpp" | head -1 | awk -F', ' '{print $5}' | tr -d f)
  steps=$(python3 -c "print(round(float('$value') - float('$def')))")
  xdotool mousemove 360 420
  sleep 0.2
  if [ "$steps" -gt 0 ]; then xdotool click --repeat "$steps" --delay 15 4; fi
  if [ "$steps" -lt 0 ]; then xdotool click --repeat "$(( -steps ))" --delay 15 5; fi
  xdotool mousemove 700 20
  sleep 0.5
fi
import -window "$win" "$out"
echo "$out"
