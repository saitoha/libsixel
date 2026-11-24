#!/bin/sh
# Verify named destination adobe zero-rate snap identity.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/named_output/0011_adobe_snap_zero || {
    printf '\n%s\n' "not ok 1 - named destination adobe zero-rate snap identity"
    exit 0
}

printf '\n%s\n' "ok 1 - named destination adobe zero-rate snap identity"
exit 0
