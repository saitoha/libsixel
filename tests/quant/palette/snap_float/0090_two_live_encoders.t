#!/bin/sh
# Verify native snap: two live encoders.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/snap_float/0001_snap_contract encode 0 0 1 5 || {
    echo "not ok 1 - two live encoders"
    exit 0
}

echo "ok 1 - two live encoders"
exit 0
