#!/usr/bin/env bash
# NULL Tilt Native - one-shot build for local use and GitHub Actions (Linux / macOS / Windows Git Bash).
#
#   scripts/build.sh                 Release build, VST3 + CLAP (+ GUI)
#   scripts/build.sh --no-ui         headless DSP-only build
#   scripts/build.sh --test          also build + run the JSFX comparison test (Linux/macOS; needs ysfx,
#                                    built automatically via the repo's tests/run.sh if missing)
#   scripts/build.sh --dpf <dir>     use a local DPF checkout instead of fetching the pinned SHA
#
# Output: build/bin/null_tilt_native.vst3, build/bin/null_tilt_native.clap, and dist/ (zipped per OS).
set -euo pipefail

here="$(cd "$(dirname "$0")/.." && pwd)"
repo="$(cd "$here/../../.." && pwd)"
build="$here/build"
ui=ON
tests=OFF
extra=()

while [ $# -gt 0 ]; do
  case "$1" in
    --no-ui) ui=OFF ;;
    --test)  tests=ON ;;
    --dpf)   shift; extra+=("-DFETCHCONTENT_SOURCE_DIR_DPF=$1") ;;
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
  gen=(-G "Visual Studio 17 2022" -A x64)
  tests=OFF   # comparison test uses dlopen; DSP is identical on every OS
elif command -v ninja >/dev/null 2>&1; then
  gen=(-G Ninja)
fi

if [ "$tests" = ON ] && [ ! -f "$repo/tests/.build/ysfx/build/libysfx.a" ]; then
  echo "== building ysfx via tests/run.sh"
  bash "$repo/tests/run.sh" >/dev/null || true
fi

if [ "$os" = macos ]; then
  extra+=("-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64")
fi

cmake -S "$here" -B "$build" "${gen[@]}" -DCMAKE_BUILD_TYPE=Release \
  -DNULLTILT_UI="$ui" -DNULLTILT_TESTS="$tests" "${extra[@]}"
cmake --build "$build" --config Release --parallel

if [ "$tests" = ON ]; then
  (cd "$build" && ctest --output-on-failure -C Release)
fi

# Package
dist="$here/dist"
rm -rf "$dist"; mkdir -p "$dist"
cp -R "$build/bin/null_tilt_native.vst3" "$build/bin/null_tilt_native.clap" "$dist/"
(cd "$dist" && cmake -E tar cf "../null-tilt-native-$os.zip" --format=zip null_tilt_native.vst3 null_tilt_native.clap)
mv "$here/null-tilt-native-$os.zip" "$dist/"
echo "== done: $dist/null-tilt-native-$os.zip"
