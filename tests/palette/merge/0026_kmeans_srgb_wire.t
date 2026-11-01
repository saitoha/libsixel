#!/bin/sh
# Verify kmeans srgb wire.
# Policy: docs/encoder/palette-construction/merge-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/merge_float/0001_merge_contract wire 0 1 0 || {
    echo "not ok 1 - kmeans srgb wire"
    exit 0
}

echo "ok 1 - kmeans srgb wire"
exit 0
