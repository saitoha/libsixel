#!/bin/sh
# Verify packed alpha: short mask atomic.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/packed_alpha/0012_short_mask_atomic || {
    echo "not ok 1 - packed alpha short mask atomic"
    exit 0
}

echo "ok 1 - packed alpha short mask atomic"
exit 0
