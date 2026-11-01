#!/bin/sh
# adobe direct grid in cielab: independent nearest/reversible values.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/destination_snap/0001_destination grid 0 1 3 || {
    echo "not ok 1 - adobe direct grid in cielab: independent nearest/reversible values"
    exit 0
}

echo "ok 1 - adobe direct grid in cielab: independent nearest/reversible values"
exit 0
