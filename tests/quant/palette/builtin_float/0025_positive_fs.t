#!/bin/sh
# Verify built-in native palette: positive fs.
# Policy: docs/encoder/palette-construction/fixed-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_POLICY=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/builtin_float/0025_positive_fs || {
    echo "not ok 1 - builtin positive fs"
    exit 0
}

echo "ok 1 - builtin positive fs"
exit 0
