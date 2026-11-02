#!/usr/bin/env bash
# NULL JSFX VST3/CLAP - one-shot build for local use and GitHub Actions (Linux / macOS / Windows Git Bash).
#
#   scripts/build.sh                    Release build of every plugin, VST3 + CLAP (+ GUI)
#   scripts/build.sh --test             also build and run the JSFX comparison tests (Linux/macOS)
#   scripts/build.sh --plugins "a;b"    only these plugins (folder names in plugins/)
#   scripts/build.sh --build-dir <dir>  build directory (default: native/build)
#   scripts/build.sh --dpf <dir>        use a local DPF checkout instead of fetching the pinned SHA
#
# Output: <build>/bin/null_jsfx_<name>.vst3 and .clap (and .lv2 on Linux), copied to
# native/dist/{VST3,CLAP,LV2}/NULL JSFX/.
set -euo pipefail

here="$(cd "$(dirname "$0")/.." && pwd)"
repo="$(cd "$here/.." && pwd)"
build="$here/build"
tests=OFF
extra=()

while [ $# -gt 0 ]; do
  case "$1" in
    --test)      tests=ON ;;
    --plugins)   shift; extra+=("-DNULL_PLUGINS=$1") ;;
    --build-dir) shift; build="$1" ;;
    --dpf)       shift; extra+=("-DFETCHCONTENT_SOURCE_DIR_DPF=$1") ;;
    *) echo "unknown option: $1" >&2; exit 2 ;;
  esac
  shift
done

case "$(uname -s)" in
  Linux*)               os=linux ;;
  Darwin*)              os=macos ;;
  MINGW*|MSYS*|CYGWIN*) os=windows ;;
  *)                    os=unknown ;;
esac

gen=()
if [ "$os" = windows ]; then
  # Windows on Arm builds ARM64 plugins (DPF puts them in Contents/arm64-win); x64 everywhere else.
  # No -G: CMake takes the newest Visual Studio it finds (2022 on windows-2022, 2026 on
  # windows-11-arm), so a runner image that moves to a new Visual Studio still builds.
  case "${PROCESSOR_ARCHITECTURE:-}" in
    ARM64) gen=(-A ARM64) ;;
    *)     gen=(-A x64) ;;
  esac
  tests=OFF   # the comparison test uses dlopen; the DSP is identical on every OS
elif command -v ninja >/dev/null 2>&1; then
  gen=(-G Ninja)
fi

if [ "$tests" = ON ] && [ ! -f "$repo/tests/.build/ysfx/build/libysfx.a" ]; then
  echo "== building ysfx via tests/run.sh"
  bash "$repo/tests/run.sh" --build-only
fi

cmake -S "$here" -B "$build" ${gen[@]+"${gen[@]}"} -DCMAKE_BUILD_TYPE=Release -DNULL_TESTS="$tests" ${extra[@]+"${extra[@]}"}
cmake --build "$build" --config Release --parallel

if [ "$tests" = ON ]; then
  # the comparisons are independent processes that only read files: run them on every core
  (cd "$build" && ctest --output-on-failure -C Release --parallel "$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)")
fi

dist="$here/dist"
rm -rf "$dist"
mkdir -p "$dist/VST3/NULL JSFX" "$dist/CLAP/NULL JSFX"
cp -R "$build"/bin/*.vst3 "$dist/VST3/NULL JSFX/"
cp -R "$build"/bin/*.clap "$dist/CLAP/NULL JSFX/"
if compgen -G "$build/bin/*.lv2" >/dev/null; then
  mkdir -p "$dist/LV2/NULL JSFX"
  cp -R "$build"/bin/*.lv2 "$dist/LV2/NULL JSFX/"
fi
echo "== done: $dist"
