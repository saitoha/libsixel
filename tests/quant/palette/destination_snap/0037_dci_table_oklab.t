#!/bin/sh
# dci table grid in oklab: independent nearest/reversible values.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/destination_snap/0001_destination grid 1 2 2 || {
    echo "not ok 1 - dci table grid in oklab: independent nearest/reversible values"
    exit 0
}

echo "ok 1 - dci table grid in oklab: independent nearest/reversible values"
exit 0
