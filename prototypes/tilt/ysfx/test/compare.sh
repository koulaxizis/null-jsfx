#!/usr/bin/env bash
# Renders the same test signal through (a) the built CLAP plugin and (b) the
# JSFX run directly by the ysfx library in tests/.build/ysfx, for several
# Amount values, and reports the max abs difference.
#   test/compare.sh <build-dir> <file.jsfx> [values...]
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
build="$(cd "${1:?build dir}" && pwd)"
jsfx="${2:?jsfx file}"
shift 2
[ $# -gt 0 ] && values=("$@") || values=(-100 -50 0 37 100)
repo="$(cd "$here/../../../.." && pwd)"
ylib="$repo/tests/.build/ysfx"
[ -f "$ylib/build/libysfx.a" ] || { echo "run tests/run.sh first to build $ylib"; exit 1; }
out="$build/compare"; mkdir -p "$out"
g++ -O2 -std=c++17 -I "$ylib/include" "$here/ysfx_render.cpp" "$ylib/build/libysfx.a" -lpthread -ldl -o "$out/ysfx_render"
clap="$(find "$build" -name '*.clap' -path '*Release*' | head -1)"
status=0
for v in "${values[@]}"; do
  "$build/clap_render" "$clap" Amount "$v" "$out/clap_$v.f32" > "$out/clap_$v.log"
  "$out/ysfx_render" "$jsfx" Amount "$v" "$out/ref_$v.f32" > /dev/null
  python3 - "$out/clap_$v.f32" "$out/ref_$v.f32" "$v" <<'PY' || status=1
import sys, array
a = array.array('f'); b = array.array('f')
a.frombytes(open(sys.argv[1], 'rb').read()); b.frombytes(open(sys.argv[2], 'rb').read())
assert len(a) == len(b) and len(a) > 0, (len(a), len(b))
d = max(abs(x - y) for x, y in zip(a, b))
peak = max(abs(x) for x in b)
ok = d <= 1e-6
print(f"Amount={sys.argv[3]:>5}: frames={len(a)//2} max|diff|={d:.3g} (ref peak {peak:.3f}) {'OK' if ok else 'MISMATCH'}")
sys.exit(0 if ok else 1)
PY
done
exit $status
