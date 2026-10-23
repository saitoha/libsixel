#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify packed alpha: layout rgba.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_POLICY=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/packed_alpha/0001_layout_rgba || {
    echo "not ok 1 - packed alpha layout rgba"
    exit 0
}

echo "ok 1 - packed alpha layout rgba"
exit 0
