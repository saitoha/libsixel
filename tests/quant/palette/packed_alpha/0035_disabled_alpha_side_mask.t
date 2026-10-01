#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify packed alpha: disabled alpha side mask.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/packed_alpha/0035_disabled_alpha_side_mask || {
    echo "not ok 1 - packed alpha disabled alpha side mask"
    exit 0
}

echo "ok 1 - packed alpha disabled alpha side mask"
exit 0
