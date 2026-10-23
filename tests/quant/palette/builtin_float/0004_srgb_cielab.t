#!/bin/sh
# Verify built-in native palette: srgb cielab.
# Policy: docs/encoder/palette-construction/fixed-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_POLICY=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/builtin_float/0004_srgb_cielab || {
    echo "not ok 1 - builtin srgb cielab"
    exit 0
}

echo "ok 1 - builtin srgb cielab"
exit 0
