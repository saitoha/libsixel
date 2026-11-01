#!/bin/sh
# Verify native snap: kcenter oklab partial.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/snap_float/0001_snap_contract encode 1 2 1 1 || {
    echo "not ok 1 - kcenter oklab partial"
    exit 0
}

echo "ok 1 - kcenter oklab partial"
exit 0
