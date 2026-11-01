#!/bin/sh
# P3 snap can be enabled and disabled on the same encoder.
# Policy: docs/encoder/output/output-colorspace.md
# Policy: docs/encoder/palette-construction/cover-policy/README.md
set -eux
echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/auto_cover_float/0020_p3_snap_transition || {
    echo "not ok 1 - p3 rejection recovery"
    exit 0
}
echo "ok 1 - p3 rejection recovery"
exit 0
