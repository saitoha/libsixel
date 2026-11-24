#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify fixed animation: mask map.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/fixed_animation/0023_mask_map \
    "${TOP_SRCDIR}/tests/data/inputs/mapfile/p3-fixed.gpl" || {
    printf '\n%s\n' "not ok 1 - fixed animation mask map"
    exit 0
}

printf '\n%s\n' "ok 1 - fixed animation mask map"
exit 0
