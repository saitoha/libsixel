#!/bin/sh
# Verify built-in native palette: output reuse.
# Policy: docs/encoder/palette-construction/fixed-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/builtin_float/0020_output_reuse || {
    echo "not ok 1 - builtin output reuse"
    exit 0
}

echo "ok 1 - builtin output reuse"
exit 0
