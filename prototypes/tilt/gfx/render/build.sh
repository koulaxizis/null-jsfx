#!/usr/bin/env bash
# Builds ysfx with graphics (once) and gfx_render. Usage: build.sh [build-dir]  (default: tests/.build/ysfx-gfx)
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../../.." && pwd)"
out="${1:-$root/tests/.build/ysfx-gfx}"
ysfx_rev=8077347ccf4115567aed81400281dca57acbb0cc
if [ ! -f "$out/build/libysfx.a" ]; then
  if [ ! -d "$out" ]; then
    git clone --quiet https://github.com/jpcima/ysfx.git "$out"
    git -C "$out" checkout --quiet "$ysfx_rev"
    git -C "$out" submodule update --quiet --init --recursive
  fi
  cmake -S "$out" -B "$out/build" -DYSFX_PLUGIN=OFF -DYSFX_GFX=ON -DCMAKE_BUILD_TYPE=Release >/dev/null
  cmake --build "$out/build" -j"$(nproc 2>/dev/null || echo 2)" >/dev/null
fi
g++ -O2 -std=c++17 -I "$out/include" -I "$out/thirdparty/stb" "$here/gfx_render.cpp" "$out/build/libysfx.a" \
  -lfreetype -lfontconfig -lpthread -ldl -o "$out/../gfx_render"
echo "$out/../gfx_render"
