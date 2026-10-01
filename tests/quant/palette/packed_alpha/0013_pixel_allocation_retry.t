#!/bin/sh
# Verify packed alpha: pixel allocation retry.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/packed_alpha/0013_pixel_allocation_retry || {
    echo "not ok 1 - packed alpha pixel allocation retry"
    exit 0
}

echo "ok 1 - packed alpha pixel allocation retry"
exit 0
