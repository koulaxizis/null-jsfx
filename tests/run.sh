#!/usr/bin/env bash
# Builds the ysfx engine (once) and runs the NULL JSFX checks:
#   1. tests/lint.py      - header and code conventions
#   2. tests/jsfx_test    - compiles and renders every plugin at 44.1/48/96 kHz
# Usage: tests/run.sh [plugin.jsfx ...]   (default: every plugin)
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="$(dirname "$here")"
build="$here/.build"
ysfx_rev=8077347ccf4115567aed81400281dca57acbb0cc

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
if [ ! -x "$build/jsfx_test" ] || [ "$here/jsfx_test.cpp" -nt "$build/jsfx_test" ]; then
  g++ -O2 -std=c++17 -I "$build/ysfx/include" "$here/jsfx_test.cpp" "$build/ysfx/build/libysfx.a" -lpthread -ldl -o "$build/jsfx_test"
fi

if [ $# -eq 0 ]; then set -- "$root"/DATA/Effects/null_jsfx/*.jsfx; fi
status=0
python3 "$here/lint.py" "$@" || status=1
"$build/jsfx_test" "$@" || status=1
exit $status
