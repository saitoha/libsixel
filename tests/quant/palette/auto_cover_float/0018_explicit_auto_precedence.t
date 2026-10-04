#!/bin/sh
# Explicit AUTO overrides an active environment cover budget.
# Policy: docs/encoder/output/output-colorspace.md
# Policy: docs/encoder/palette-construction/cover-policy/README.md
set -eux
echo "1..1"
set -v

SIXEL_PALETTE_COVER=corners SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/auto_cover_float/0018_explicit_auto_precedence || {
    echo "not ok 1 - explicit auto precedence"
    exit 0
}
echo "ok 1 - explicit auto precedence"
exit 0
