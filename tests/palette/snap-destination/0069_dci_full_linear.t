#!/bin/sh
# dci full grid in linear coordinates.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/destination_snap/0001_destination grid 1 4 1 || {
    echo "not ok 1 - dci full grid in linear coordinates"
    exit 0
}

echo "ok 1 - dci full grid in linear coordinates"
exit 0
