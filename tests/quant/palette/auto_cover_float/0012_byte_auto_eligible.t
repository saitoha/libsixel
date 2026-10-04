#!/bin/sh
# AUTO leaves an otherwise byte-eligible job on its byte path.
# Policy: docs/encoder/output/output-colorspace.md
# Policy: docs/encoder/palette-construction/cover-policy/README.md
set -eux
echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/auto_cover_float/0012_byte_auto_eligible || {
    echo "not ok 1 - byte auto eligible"
    exit 0
}
echo "ok 1 - byte auto eligible"
exit 0
