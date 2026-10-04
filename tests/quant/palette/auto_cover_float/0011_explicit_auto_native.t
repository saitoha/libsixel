#!/bin/sh
# Plain AUTO preserves native extended palette values.
# Policy: docs/encoder/output/output-colorspace.md
# Policy: docs/encoder/palette-construction/cover-policy/README.md
set -eux
echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/auto_cover_float/0011_explicit_auto_native || {
    echo "not ok 1 - explicit auto native"
    exit 0
}
echo "ok 1 - explicit auto native"
exit 0
