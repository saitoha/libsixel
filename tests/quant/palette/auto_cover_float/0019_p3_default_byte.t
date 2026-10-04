#!/bin/sh
# P3 defaults admit byte input despite a byte preference.
# Policy: docs/encoder/output/output-colorspace.md
# Policy: docs/encoder/palette-construction/cover-policy/README.md
set -eux
echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/auto_cover_float/0019_p3_default_byte || {
    echo "not ok 1 - p3 default byte"
    exit 0
}
echo "ok 1 - p3 default byte"
exit 0
