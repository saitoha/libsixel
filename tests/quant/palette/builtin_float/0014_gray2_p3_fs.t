#!/bin/sh
# Verify built-in native palette: gray2 p3 fs.
# Policy: docs/encoder/palette-construction/fixed-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/builtin_float/0014_gray2_p3_fs || {
    echo "not ok 1 - builtin gray2 p3 fs"
    exit 0
}

echo "ok 1 - builtin gray2 p3 fs"
exit 0
