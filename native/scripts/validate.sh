#!/usr/bin/env bash
# NULL JSFX VST3/CLAP - run an official plugin validator on every plugin of a package.
#
#   scripts/validate.sh pluginval <package dir> <reports dir>
#   scripts/validate.sh clap      <package dir> <reports dir>
#
# <package dir> holds VST3/NULL JSFX/*.vst3 and CLAP/NULL JSFX/*.clap (native/dist or a
# NULL-JSFX-VST3-CLAP-<OS> package). The validator binaries come from $PLUGINVAL and
# $CLAP_VALIDATOR (default: on PATH).
#
#   pluginval       pluginval validate --strictness-level 5 --skip-gui-tests per .vst3,
#                   a JUnit report and a log per plugin in <reports>/pluginval/. Each plugin gets
#                   its own random seed (or $PLUGINVAL_SEED, hex 0x.. or decimal, for all of
#                   them), printed in the job log with the failed checks, so a failure seen once
#                   can be replayed with --random-seed
#   clap            clap-validator validate --only-failed --json per .clap (out of process),
#                   the JSON and a log per plugin in <reports>/clap-validator/
#
# Every plugin is checked even when one fails. The list of failures (for clap, also the failed
# tests grouped by clap_report.py; $PYTHON, default python3) is printed and, on GitHub Actions,
# added to the job summary. Exit code 1 when any plugin fails.
set -uo pipefail

if [ $# -ne 3 ]; then
  echo "usage: $0 pluginval|clap <package dir> <reports dir>" >&2
  exit 2
fi
kind="$1"
pkg="$2"
reports="$3"

case "$kind" in
  pluginval) dir="$pkg/VST3/NULL JSFX"; ext=vst3; out="$reports/pluginval"; tool="${PLUGINVAL:-pluginval}" ;;
  clap)      dir="$pkg/CLAP/NULL JSFX"; ext=clap; out="$reports/clap-validator"; tool="${CLAP_VALIDATOR:-clap-validator}" ;;
  *) echo "unknown validator: $kind" >&2; exit 2 ;;
esac

mkdir -p "$out"
shopt -s nullglob
plugins=("$dir"/*."$ext")
if [ ${#plugins[@]} -eq 0 ]; then
  echo "no .$ext plugins in $dir" >&2
  exit 2
fi

failed=()
for p in "${plugins[@]}"; do
  name="$(basename "$p" ."$ext")"
  echo "::group::$name.$ext"
  start=$SECONDS
  if [ "$kind" = pluginval ]; then
    seed="${PLUGINVAL_SEED:-$(printf '0x%x' $(( (RANDOM << 15 | RANDOM) + 1 )))}"
    echo "seed $seed"
    "$tool" validate --strictness-level 5 --skip-gui-tests --random-seed "$seed" \
      --junit-xml "$out/$name.xml" "$p" > "$out/$name.log" 2>&1
    rc=$?
    tail -n 5 "$out/$name.log"
    if [ $rc -ne 0 ]; then
      # the failed checks with the test they belong to (the tail shows only the last test)
      grep -E '^(Starting tests in|!!! )' "$out/$name.log" | grep -B1 '^!!! ' | grep -v '^--$'
      echo "replay: pluginval validate --strictness-level 5 --skip-gui-tests --random-seed $seed $name.$ext"
    fi
  else
    "$tool" validate --only-failed --json "$p" > "$out/$name.json" 2> "$out/$name.log"
    rc=$?
    cat "$out/$name.log"
  fi
  echo "exit $rc, $((SECONDS - start)) s"
  echo "::endgroup::"
  if [ $rc -ne 0 ]; then
    failed+=("$name")
    echo "::warning title=$kind::$name.$ext failed (exit $rc)${seed:+, seed $seed}"
  fi
done

summary="### $kind: $(( ${#plugins[@]} - ${#failed[@]} ))/${#plugins[@]} passed"
if [ ${#failed[@]} -gt 0 ]; then
  summary+=$'\n\nFailed: '"${failed[*]}"
  if [ "$kind" = clap ]; then
    summary+=$'\n\n'"$("${PYTHON:-python3}" "$(dirname "$0")/clap_report.py" "$out")"
  fi
fi
echo "$summary"
if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
  printf '%s\n\n' "$summary" >> "$GITHUB_STEP_SUMMARY"
fi
[ ${#failed[@]} -eq 0 ]
