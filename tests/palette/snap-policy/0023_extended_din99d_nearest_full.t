#!/bin/sh
# Verify native snap: extended din99d nearest full.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/snap_float/0001_snap_contract unit 0 4 1 0 || {
    printf '\n%s\n' "not ok 1 - extended din99d nearest full"
    exit 0
}

printf '\n%s\n' "ok 1 - extended din99d nearest full"
exit 0
