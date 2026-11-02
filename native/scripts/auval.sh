#!/usr/bin/env bash
# NULL JSFX Audio Unit - run Apple's auval on every AU of a package (macOS only).
#
#   scripts/auval.sh <package dir> <reports dir>
#
# <package dir> holds AU/NULL JSFX/*.component (native/dist or a NULL-JSFX-VST3-CLAP-macOS
# package). Each component is installed in ~/Library/Audio/Plug-Ins/Components (the folder
# Logic, GarageBand and MainStage scan), the component registry is restarted, and
# `auval -strict -v <type> <subtype> <maker>` runs on it, with its codes read from the
# component's Info.plist. A log per plugin goes to <reports>/auval/. Every plugin is checked
# even when one fails; the failures are printed and, on GitHub Actions, added to the job
# summary. Exit code 1 when any plugin fails.
set -uo pipefail

if [ $# -ne 2 ]; then
  echo "usage: $0 <package dir> <reports dir>" >&2
  exit 2
fi
dir="$1/AU/NULL JSFX"
out="$2/auval"
mkdir -p "$out"
shopt -s nullglob
plugins=("$dir"/*.component)
if [ ${#plugins[@]} -eq 0 ]; then
  echo "no .component plugins in $dir" >&2
  exit 2
fi

dest="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$dest"
for p in "${plugins[@]}"; do
  rm -rf "$dest/$(basename "$p")"
  cp -R "$p" "$dest/"
done
# the registry caches the components it has seen; restart it so it reads the new ones
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
sleep 2

pb=/usr/libexec/PlistBuddy
failed=()
for p in "${plugins[@]}"; do
  name="$(basename "$p" .component)"
  plist="$p/Contents/Info.plist"
  type="$($pb -c 'Print :AudioComponents:0:type' "$plist" 2>/dev/null)"
  sub="$($pb -c 'Print :AudioComponents:0:subtype' "$plist" 2>/dev/null)"
  maker="$($pb -c 'Print :AudioComponents:0:manufacturer' "$plist" 2>/dev/null)"
  echo "::group::$name.component ($type $sub $maker)"
  if [ -z "$type" ] || [ -z "$sub" ] || [ -z "$maker" ]; then
    echo "no AudioComponents entry in $plist" | tee "$out/$name.log"
    rc=1
  else
    auval -strict -v "$type" "$sub" "$maker" > "$out/$name.log" 2>&1
    rc=$?
    tail -n 5 "$out/$name.log"
  fi
  echo "::endgroup::"
  if [ $rc -ne 0 ]; then
    failed+=("$name")
    echo "FAIL $name"
  fi
done

echo "auval: ${#failed[@]} of ${#plugins[@]} plugins failed"
if [ ${#failed[@]} -gt 0 ]; then
  printf '  %s\n' "${failed[@]}"
  if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
    {
      echo "### auval: ${#failed[@]} of ${#plugins[@]} Audio Units failed"
      printf -- '- %s\n' "${failed[@]}"
    } >> "$GITHUB_STEP_SUMMARY"
  fi
  exit 1
fi
