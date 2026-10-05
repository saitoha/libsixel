#!/bin/sh
# Verify built-in native palette: multiframe native.
# Policy: docs/encoder/palette-construction/fixed-palettes.md
set -eux

echo "1..1"
set -v

# The C test checks encoded data in memory. Keep animation cursor controls off
# TAP stdout while preserving assertion diagnostics on stderr.
SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/builtin_float/0023_multiframe_native >/dev/null || {
    echo "not ok 1 - builtin multiframe native"
    exit 0
}

echo "ok 1 - builtin multiframe native"
exit 0
