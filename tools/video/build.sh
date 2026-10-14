#!/usr/bin/env bash
# Builds gfx_video (with the ysfx that tools/gfx_render/build.sh builds). Prints its path.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
ysfx="$root/tests/.build/ysfx-gfx"
[ -f "$ysfx/build/libysfx.a" ] || "$root/tools/gfx_render/build.sh" >/dev/null
g++ -O2 -std=c++17 -I "$ysfx/include" "$here/gfx_video.cpp" "$ysfx/build/libysfx.a" \
  -lfreetype -lfontconfig -lpthread -ldl -o "$root/tests/.build/gfx_video"
echo "$root/tests/.build/gfx_video"
