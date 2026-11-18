#!/bin/sh
# Check replay registration without executing workflow implementation code.
set -eux
echo '1..1'
set -v
src_root=$1
test -f "$src_root/.github/workflows/triage.yml" -a \
    -f "$src_root/.github/workflows/fuzzing.yml" || {
    echo 'ok 1 # SKIP workflow checkout required'
    exit 0
}
awk -f "$src_root/tests/_static/awk/fuzz-triage-registration.awk" \
    "$src_root/.github/workflows/triage.yml" \
    "$src_root/.github/workflows/fuzzing.yml" >&2 || {
    echo 'not ok 1 - fuzz jobs have complete triage replay registration'
    exit 0
}
echo 'ok 1 - fuzz jobs have complete triage replay registration'
exit 0
