#!/bin/sh
# Environment hard mode and growth leave native AUTO off.
# Policy: docs/encoder/output/output-colorspace.md
# Policy: docs/encoder/palette-construction/cover-policy/README.md
set -eux
echo "1..1"
set -v

SIXEL_PALETTE_COVER_MODE=hard SIXEL_PALETTE_COVER_GROW=1 SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/auto_cover_float/0017_environment_hard_grow || {
    echo "not ok 1 - environment hard grow"
    exit 0
}
echo "ok 1 - environment hard grow"
exit 0
