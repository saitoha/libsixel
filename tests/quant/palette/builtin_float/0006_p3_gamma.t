#!/bin/sh
# Verify built-in native palette: p3 gamma.
# Policy: docs/encoder/palette-construction/fixed-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_POLICY=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/builtin_float/0006_p3_gamma || {
    echo "not ok 1 - builtin p3 gamma"
    exit 0
}

echo "ok 1 - builtin p3 gamma"
exit 0
