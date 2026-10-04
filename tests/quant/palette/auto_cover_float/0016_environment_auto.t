#!/bin/sh
# Environment AUTO admits the native palette path.
# Policy: docs/encoder/output/output-colorspace.md
# Policy: docs/encoder/palette-construction/cover-policy/README.md
set -eux
echo "1..1"
set -v

SIXEL_PALETTE_COVER=auto SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/auto_cover_float/0016_environment_auto || {
    echo "not ok 1 - environment auto"
    exit 0
}
echo "ok 1 - environment auto"
exit 0
