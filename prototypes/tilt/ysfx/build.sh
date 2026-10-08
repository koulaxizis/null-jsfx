#!/usr/bin/env bash
# Build "NULL Tilt YS" (VST3 + CLAP, + AU on macOS) on Linux, macOS (universal
# arm64+x86_64) or Windows (MSVC, run from Git Bash / GitHub Actions `shell: bash`).
#
#   ./build.sh [--jsfx FILE] [--build-dir DIR] [--config Release|Debug] [--test] [-- extra cmake args]
#
# Bundles are copied to dist/<platform>/.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
jsfx="$here/../gfx/null_tilt_gfx.jsfx"
build="$here/build"
config=Release
run_tests=0
extra=()
while [ $# -gt 0 ]; do
  case "$1" in
    --jsfx) jsfx="$2"; shift 2 ;;
    --build-dir) build="$2"; shift 2 ;;
    --config) config="$2"; shift 2 ;;
    --test) run_tests=1; shift ;;
    --) shift; extra=("$@"); break ;;
    *) echo "unknown option: $1" >&2; exit 2 ;;
  esac
done
[ -f "$jsfx" ] || { echo "JSFX not found: $jsfx" >&2; exit 1; }
jsfx="$(cd "$(dirname "$jsfx")" && pwd)/$(basename "$jsfx")"

case "$(uname -s)" in
  Linux*)  platform=linux ;;
  Darwin*) platform=macos ;;
  MINGW*|MSYS*|CYGWIN*) platform=windows ;;
  *) echo "unsupported OS: $(uname -s)" >&2; exit 1 ;;
esac

args=(-DCMAKE_BUILD_TYPE="$config" -DNULL_JSFX_FILE="$jsfx")
case "$platform" in
  linux)
    command -v ninja >/dev/null && args+=(-G Ninja) ;;
  macos)
    command -v ninja >/dev/null && args+=(-G Ninja)
    args+=(-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13) ;;
  windows)
    args+=(-G "Visual Studio 17 2022" -A x64)
    # EEL2's x64 code needs NASM with MSVC (choco install nasm).
    if ! command -v nasm >/dev/null; then
      for n in "/c/Program Files/NASM/nasm.exe" "C:/Program Files/NASM/nasm.exe"; do
        [ -f "$n" ] && { args+=(-DNASM_PROGRAM="$(cygpath -m "$n" 2>/dev/null || echo "$n")"); break; }
      done
    fi ;;
esac
[ "$run_tests" = 1 ] && [ "$platform" = linux ] && args+=(-DNULL_BUILD_TESTS=ON)

cmake -S "$here" -B "$build" "${args[@]}" ${extra[@]+"${extra[@]}"}
jobs="$( (nproc || sysctl -n hw.ncpu || echo 4) 2>/dev/null | head -1)"
cmake --build "$build" --config "$config" --parallel "$jobs"

art="$build/NullTiltYS_artefacts/$config"
dist="$here/dist/$platform"
rm -rf "$dist"; mkdir -p "$dist"
for fmt in VST3 CLAP AU; do
  [ -d "$art/$fmt" ] && cp -R "$art/$fmt/." "$dist/"
done
if [ "$platform" = linux ] && [ "$config" = Release ]; then
  find "$dist" \( -name '*.so' -o -name '*.clap' \) -type f -exec strip --strip-unneeded {} \;
fi
echo "== outputs in $dist:"; ls -1 "$dist"

if [ "$run_tests" = 1 ] && [ "$platform" = linux ]; then
  if [ -f "$here/../../../tests/.build/ysfx/build/libysfx.a" ]; then
    "$here/test/compare.sh" "$build" "$jsfx"
  else
    echo "skip compare: run tests/run.sh first to build the reference ysfx library"
  fi
fi
