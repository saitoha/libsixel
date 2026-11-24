#!/bin/sh
# Policy: docs/encoder/palette-construction/snap-policy/README.md
# Verify native snap: cielab smptec partial.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/snap_float/0001_snap_contract encode 0 3 1 9 || {
    printf '\n%s\n' "not ok 1 - cielab smptec partial"
    exit 0
}

printf '\n%s\n' "ok 1 - cielab smptec partial"
exit 0
