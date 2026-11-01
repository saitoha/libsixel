#!/bin/sh
# Policy: docs/encoder/palette-construction/snap-policy/README.md
# Verify native snap: gamma smptec partial.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/snap_float/0001_snap_contract encode 0 0 1 9 || {
    echo "not ok 1 - gamma smptec partial"
    exit 0
}

echo "ok 1 - gamma smptec partial"
exit 0
