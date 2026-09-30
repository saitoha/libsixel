#!/bin/sh
# Verify built-in native palette: table reuse.
# Policy: docs/encoder/palette-construction/fixed-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/builtin_float/0021_table_reuse || {
    echo "not ok 1 - builtin table reuse"
    exit 0
}

echo "ok 1 - builtin table reuse"
exit 0
