#!/bin/sh
# Verify kmeans byte eligibility.
# Policy: docs/encoder/palette-construction/merge-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/merge_float/0001_merge_contract byte 0 0 0 || {
    echo "not ok 1 - kmeans byte eligibility"
    exit 0
}

echo "ok 1 - kmeans byte eligibility"
exit 0
