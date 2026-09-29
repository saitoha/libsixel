#!/bin/sh
# Verify kmedoids p3 wire.
# Policy: docs/encoder/palette-construction/merge-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/merge_float/0001_merge_contract wire 2 1 1 || {
    echo "not ok 1 - kmedoids p3 wire"
    exit 0
}

echo "ok 1 - kmedoids p3 wire"
exit 0
