#!/bin/sh
# Animation re-evaluates byte and float admission on each frame.
# Policy: docs/encoder/output/output-colorspace.md
# Policy: docs/encoder/palette-construction/cover-policy/README.md
set -eux
echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/auto_cover_float/0023_animation_precision || {
    echo "not ok 1 - animation precision"
    exit 0
}
echo "ok 1 - animation precision"
exit 0
