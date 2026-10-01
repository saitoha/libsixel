#!/bin/sh
# Verify byte mask boundary.
# Policy: docs/encoder/palette-construction/cover-policy/README.md
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/cover_mask/0001_cover_mask wire 11 || {
    echo "not ok 1 - byte mask boundary"
    exit 0
}

echo "ok 1 - byte mask boundary"
exit 0
