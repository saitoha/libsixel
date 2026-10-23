#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify packed alpha: layout argb.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_POLICY=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/packed_alpha/0002_layout_argb || {
    echo "not ok 1 - packed alpha layout argb"
    exit 0
}

echo "ok 1 - packed alpha layout argb"
exit 0
