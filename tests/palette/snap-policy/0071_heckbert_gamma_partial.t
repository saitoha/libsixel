#!/bin/sh
# Verify native snap: heckbert gamma partial.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/snap_float/0001_snap_contract encode 3 0 1 1 || {
    printf '\n%s\n' "not ok 1 - heckbert gamma partial"
    exit 0
}

printf '\n%s\n' "ok 1 - heckbert gamma partial"
exit 0
