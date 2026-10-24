#!/bin/sh
# Verify built-in native palette: vt340 duplicates.
# Policy: docs/encoder/palette-construction/fixed-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/builtin_float/0015_vt340_duplicates || {
    echo "not ok 1 - builtin vt340 duplicates"
    exit 0
}

echo "ok 1 - builtin vt340 duplicates"
exit 0
