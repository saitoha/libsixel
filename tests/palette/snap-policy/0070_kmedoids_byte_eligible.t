#!/bin/sh
# Verify native snap: kmedoids byte eligible.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/snap_float/0001_snap_contract encode 2 0 1 2 || {
    printf '\n%s\n' "not ok 1 - kmedoids byte eligible"
    exit 0
}

printf '\n%s\n' "ok 1 - kmedoids byte eligible"
exit 0
