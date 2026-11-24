#!/bin/sh
# Verify native snap: p3 reversible values.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/snap_float/0001_snap_contract encode 0 0 2 3 || {
    printf '\n%s\n' "not ok 1 - p3 reversible values"
    exit 0
}

printf '\n%s\n' "ok 1 - p3 reversible values"
exit 0
