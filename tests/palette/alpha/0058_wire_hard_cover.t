#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify packed alpha: wire hard cover.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/packed_alpha/0024_wire_hard_cover || {
    printf '\n%s\n' "not ok 1 - packed alpha wire hard cover"
    exit 0
}

printf '\n%s\n' "ok 1 - packed alpha wire hard cover"
exit 0
