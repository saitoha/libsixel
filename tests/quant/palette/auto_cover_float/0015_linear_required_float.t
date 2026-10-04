#!/bin/sh
# Working-space conversion admits required float with AUTO.
# Policy: docs/encoder/output/output-colorspace.md
# Policy: docs/encoder/palette-construction/cover-policy/README.md
set -eux
echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/auto_cover_float/0015_linear_required_float || {
    echo "not ok 1 - linear required float"
    exit 0
}
echo "ok 1 - linear required float"
exit 0
