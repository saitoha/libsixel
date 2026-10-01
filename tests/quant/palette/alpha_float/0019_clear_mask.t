#!/bin/sh
# Verify native alpha: clear mask.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/alpha_float/0019_clear_mask || {
    echo "not ok 1 - native alpha clear mask"
    exit 0
}

echo "ok 1 - native alpha clear mask"
exit 0
