#!/bin/sh
# Verify built-in native palette: byte eligible.
# Policy: docs/encoder/palette-construction/fixed-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_POLICY=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/builtin_float/0016_byte_eligible || {
    echo "not ok 1 - builtin byte eligible"
    exit 0
}

echo "ok 1 - builtin byte eligible"
exit 0
