#!/bin/sh
# Verify smptec fixed map native output.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/output_float/0026_smptec_fixed_map \
    "${TOP_SRCDIR}/tests/data/inputs/mapfile/p3-fixed.pal" || {
    echo "not ok 1 - smptec fixed map native output"
    exit 0
}

echo "ok 1 - smptec fixed map native output"
exit 0
