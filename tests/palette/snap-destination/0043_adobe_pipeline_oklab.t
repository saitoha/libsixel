#!/bin/sh
# adobe nearest pipeline in oklab: float entries and exact wire codes.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/destination_snap/0001_destination pipeline 0 0 2 0 0 || {
    printf '\n%s\n' "not ok 1 - adobe nearest pipeline in oklab: float entries and exact wire codes"
    exit 0
}

printf '\n%s\n' "ok 1 - adobe nearest pipeline in oklab: float entries and exact wire codes"
exit 0
