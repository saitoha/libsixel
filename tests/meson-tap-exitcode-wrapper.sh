#!/bin/sh

set -eu

if test $# -lt 1; then
  printf '%s\n' "error: missing TAP command script" >&2
  exit 2
fi

artifact_dir=${ARTIFACT_LOCAL_DIR:-.}
mkdir -p "$artifact_dir"

run_script=$1
shift

stdout_log=$(mktemp "$artifact_dir/.meson-tap-stdout.XXXXXX")
stderr_log=$(mktemp "$artifact_dir/.meson-tap-stderr.XXXXXX")

trap 'rm -f "$stdout_log" "$stderr_log"' EXIT HUP INT TERM

if sh "$run_script" "$@" >"$stdout_log" 2>"$stderr_log"; then
  run_status=0
else
  run_status=$?
fi

cat "$stdout_log"
cat "$stderr_log" >&2

if test "$run_status" -ne 0; then
  exit "$run_status"
fi

tap_has_result=0
tap_has_failure=0
tap_skip_plan=0

while IFS= read -r line; do
  case "$line" in
    'ok '*) tap_has_result=1 ;;
    'ok') tap_has_result=1 ;;
    'not ok '*) tap_has_result=1; tap_has_failure=1 ;;
    'not ok') tap_has_result=1; tap_has_failure=1 ;;
    '1..0 # SKIP'*) tap_skip_plan=1 ;;
  esac
done < "$stdout_log"

if test "$tap_skip_plan" -eq 1 && test "$tap_has_result" -eq 0; then
  exit 77
fi

if test "$tap_has_result" -eq 0; then
  printf '%s\n' "error: no TAP result lines found in xfail wrapper output" >&2
  exit 1
fi

if test "$tap_has_failure" -eq 1; then
  exit 1
fi

exit 0
