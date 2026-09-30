#!/bin/sh
# Verify native snap: kmedoids oklab partial.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/snap_float/0001_snap_contract encode 2 2 1 1 || {
    echo "not ok 1 - kmedoids oklab partial"
    exit 0
}

echo "ok 1 - kmedoids oklab partial"
exit 0
