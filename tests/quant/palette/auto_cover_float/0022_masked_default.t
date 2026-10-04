#!/bin/sh
# Omitted cover excludes hidden RGB and reserves its mask slot.
# Policy: docs/encoder/output/output-colorspace.md
# Policy: docs/encoder/palette-construction/cover-policy/README.md
set -eux
echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/auto_cover_float/0022_masked_default || {
    echo "not ok 1 - masked default"
    exit 0
}
echo "ok 1 - masked default"
exit 0
