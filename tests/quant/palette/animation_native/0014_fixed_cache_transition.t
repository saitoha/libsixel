#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify generated animation: fixed cache transition.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/animation_native/0014_fixed_cache_transition || {
    echo "not ok 1 - animation fixed cache transition"
    exit 0
}

echo "ok 1 - animation fixed cache transition"
exit 0
