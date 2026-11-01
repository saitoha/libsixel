#!/bin/sh
# adobe nearest pipeline in linear: float entries and exact wire codes.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/destination_snap/0001_destination pipeline 0 0 1 0 0 || {
    echo "not ok 1 - adobe nearest pipeline in linear: float entries and exact wire codes"
    exit 0
}

echo "ok 1 - adobe nearest pipeline in linear: float entries and exact wire codes"
exit 0
