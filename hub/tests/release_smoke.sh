#!/usr/bin/env bash
# Installs, checks and removes plugins from a real GitHub release with the Hub's core (hub_cli),
# into a temporary folder. The catalog comes from this checkout (tools/build_catalog.py) and is
# served locally; the packages come from the release itself, over range requests.
# Usage: hub/tests/release_smoke.sh [release tag, default: the latest release]
set -euo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
root="$(cd "$here/.." && pwd)"
tag="${1:-$(curl -fsSL -o /dev/null -w '%{url_effective}' https://github.com/koulaxizis/null-jsfx/releases/latest | sed 's#.*/tag/##')}"
work="$(mktemp -d)"
trap 'kill $server 2>/dev/null || true; rm -rf "$work"' EXIT

python3 "$root/tools/build_catalog.py" --version "$tag" --out "$work/catalog.json"
port=8765
python3 - "$work/catalog.json" "$port" <<'PY' &
import http.server, sys
cat = open(sys.argv[1], 'rb').read()
class H(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path.endswith('/NULL-JSFX-Catalog.json'):
            self.send_response(200); self.send_header('Content-Length', str(len(cat))); self.end_headers(); self.wfile.write(cat)
        else:
            self.send_response(302); self.send_header('Content-Length', '0')
            self.send_header('Location', 'https://github.com/koulaxizis/null-jsfx/releases' + self.path); self.end_headers()
    def log_message(self, *a): pass
http.server.HTTPServer(('127.0.0.1', int(sys.argv[2])), H).serve_forever()
PY
server=$!
sleep 1

cd "$here"
cargo build -q -p null_hub_core --example hub_cli
cli="$here/target/debug/examples/hub_cli"
export NULL_JSFX_HUB_DATA="$work/data" NULL_JSFX_HUB_RELEASES="http://127.0.0.1:$port"
"$cli" --root "$work/root" install tilt:all delay:vst3 | tee "$work/out.txt"
grep -q "release $tag: 5 installed, 0 to update" "$work/out.txt"
test -f "$work/root/REAPER/Effects/null_jsfx/tilt.jsfx"
test -f "$work/root/REAPER/Effects/null_jsfx/gfx/null_gfx.jsfx-inc"
test -d "$work/root/VST3/NULL JSFX/null_jsfx_delay.vst3/Contents"
test -e "$work/root/CLAP/NULL JSFX/null_jsfx_tilt.clap"
if [ -d "$work/data/cache/$tag" ]; then
  echo "the whole package was downloaded; only parts were needed" >&2
  exit 1
fi
# check and repair: a damaged VST3 file is found from the zip's directory and put back
"$cli" --root "$work/root" repair | tee "$work/out.txt"
if grep -q "damaged" "$work/out.txt"; then exit 1; fi
so=$(find "$work/root/VST3/NULL JSFX/null_jsfx_delay.vst3/Contents" -type f | head -n 1)
printf 'x' >> "$so"
"$cli" --root "$work/root" repair | tee "$work/out.txt"
grep -q "damaged: delay:vst3" "$work/out.txt"
"$cli" --root "$work/root" repair | tee "$work/out.txt"
if grep -q "damaged" "$work/out.txt"; then exit 1; fi
# the extras: Filter's FX chain comes with Filter, factory presets land in REAPER's presets
"$cli" --root "$work/root" install filter:jsfx rack:vst3 extra:chains extra:presets | tee "$work/out.txt"
grep -q "extra chains" "$work/out.txt"
test -f "$work/root/REAPER/FXChains/NULL JSFX/Auto Filter.RfxChain"
test -f "$work/root/REAPER/presets/js-null_jsfx_filter_jsfx.ini"
test -d "$work/root/VST3/NULL JSFX/null_jsfx_rack.vst3/Contents"
"$cli" --root "$work/root" remove filter:jsfx | tee "$work/out.txt"
test ! -e "$work/root/REAPER/FXChains/NULL JSFX/Auto Filter.RfxChain"
test ! -e "$work/root/REAPER/presets/js-null_jsfx_filter_jsfx.ini"   # only factory presets in it
"$cli" --root "$work/root" remove everything | tee "$work/out.txt"
test ! -e "$work/root/REAPER/FXChains/NULL JSFX"
test -z "$(ls -A "$work/root/REAPER/presets")"
grep -q "0 installed" "$work/out.txt"
if grep -q "  extra" "$work/out.txt"; then exit 1; fi  # (a "!" line would not stop set -e)
test ! -e "$work/root/VST3/NULL JSFX"
echo "release smoke test: ok ($tag)"
