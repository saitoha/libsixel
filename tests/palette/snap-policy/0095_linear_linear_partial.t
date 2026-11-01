#!/bin/sh
# Policy: docs/encoder/palette-construction/snap-policy/README.md
# Verify native snap: linear linear partial.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/snap_float/0001_snap_contract encode 0 1 1 7 || {
    echo "not ok 1 - linear linear partial"
    exit 0
}

echo "ok 1 - linear linear partial"
exit 0
