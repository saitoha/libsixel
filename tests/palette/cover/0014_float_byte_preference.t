#!/bin/sh
# precision-policy=8bit applies a float source through the byte path.
# Policy: docs/encoder/output/output-colorspace.md
# Policy: docs/encoder/palette-construction/cover-policy/README.md
set -eux
echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/auto_cover_float/0014_float_byte_preference || {
    echo "not ok 1 - float byte preference"
    exit 0
}
echo "ok 1 - float byte preference"
exit 0
