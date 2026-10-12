#!/usr/bin/env bash
# Checks the web version of NULL JSFX:
#   1. web/fx is up to date with DATA/ (node web/tools/build.js --check)
#   2. runtime checks (node --test web/test/)
#   3. null test: every translated effect against the JSFX in ysfx, sample by sample
# Builds ysfx (through tests/run.sh's settings) and web/test/render_ref.cpp once.
# Usage: web/test/run.sh [plugin.jsfx ...]
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
build="$root/tests/.build"
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
if [ ! -x "$build/render_ref" ] || [ "$here/render_ref.cpp" -nt "$build/render_ref" ]; then
  g++ -O2 -std=c++17 -I "$build/ysfx/include" "$here/render_ref.cpp" "$build/ysfx/build/libysfx.a" -lpthread -ldl -o "$build/render_ref"
fi

node "$root/web/tools/build.js" --check
node --test "$here/"
node "$here/nulltest.js" "$@"
