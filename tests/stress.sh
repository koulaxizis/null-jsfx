#!/usr/bin/env bash
# Builds the ysfx engine (once, the same pinned revision as tests/run.sh) and runs tests/stress_test
# (numerics under hard signals, memory past 8M slots at 384 kHz, the cost of a slider change) on
# the plain JSFX, several plugins at a time. GFX versions share the audio code (tolerance 0).
# Usage: tests/stress.sh [--no-numerics] [--no-memory] [--no-cost] [plugin.jsfx ...]
#        (default: every plain plugin)
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="$(dirname "$here")"
build="$here/.build"
ysfx_rev="$(sed -n 's/^ysfx_rev=//p' "$here/run.sh")"

if [ ! -f "$build/ysfx/build/libysfx.a" ]; then
  mkdir -p "$build"
  if [ ! -d "$build/ysfx" ]; then
    git clone --quiet https://github.com/jpcima/ysfx.git "$build/ysfx"
    git -C "$build/ysfx" checkout --quiet "$ysfx_rev"
    git -C "$build/ysfx" submodule update --quiet --init --recursive
  fi
  cmake -S "$build/ysfx" -B "$build/ysfx/build" -DYSFX_PLUGIN=OFF -DYSFX_GFX=OFF -DCMAKE_BUILD_TYPE=Release >/dev/null
  cmake --build "$build/ysfx/build" -j"$(nproc 2>/dev/null || echo 2)" >/dev/null
fi
if [ ! -x "$build/stress_test" ] || [ "$here/stress_test.cpp" -nt "$build/stress_test" ]; then
  g++ -O2 -std=c++17 -I "$build/ysfx/include" "$here/stress_test.cpp" "$build/ysfx/build/libysfx.a" -lpthread -ldl -o "$build/stress_test"
fi

opts=()
while [ $# -gt 0 ] && [ "${1#--}" != "$1" ]; do opts+=("$1"); shift; done
if [ $# -eq 0 ]; then set -- "$root"/DATA/Effects/null_jsfx/*.jsfx; fi

# one plugin per process, as many processes as cores; the slider cost is timed, so it runs
# afterwards on its own, one plugin at a time
out="$(mktemp)"
trap 'rm -f "$out"' EXIT
jobs="$(nproc 2>/dev/null || echo 2)"
printf '%s\0' "$@" | xargs -0 -P "$jobs" -n 1 "$build/stress_test" --no-cost "${opts[@]}" > "$out" || true
case " ${opts[*]-} " in
  *" --no-cost "*) ;;
  *) for f in "$@"; do "$build/stress_test" --no-numerics --no-memory "${opts[@]}" "$f" >> "$out" || true; done ;;
esac
grep -v -e '^$' -e 'plugins failed$' "$out" | awk '/^(FAIL|ok  |warn) /{name=$2} {print name "\t" $0}' | sort -s -k1,1 | cut -f2-
failed="$(grep -c '^FAIL' "$out" || true)"
echo
echo "$failed failing result(s) over $# plugins"
[ "$failed" -eq 0 ]
