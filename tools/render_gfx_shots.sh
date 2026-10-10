#!/usr/bin/env bash
# Renders the interface of every GFX JSFX to assets/img/gfx/<name>.png for the website guides
# (720x480, slider a little past its default so the display shows something). Run it after
# adding or changing a GFX plugin, then tools/build_site.py.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
render="$root/tests/.build/gfx_render"
[ -x "$render" ] || "$root/tools/gfx_render/build.sh"
mkdir -p "$root/assets/img/gfx"
for f in "$root"/DATA/Effects/null_jsfx/gfx/*_gfx.jsfx; do
  name="$(basename "$f" _gfx.jsfx)"
  value="$(grep -m1 '^slider1:' "$f" | sed -E 's/^slider1:([-0-9.]+)<([-0-9.]+),([-0-9.]+).*/\1 \2 \3/' |
           awk '{ v = $1 + ($3 - $2) * 0.35; if (v > $3) v = $3; print v }')"
  "$render" "$f" "$root/assets/img/gfx/$name.png" 720 480 "$value" >/dev/null
  echo "$name ($value)"
done
