#!/bin/sh
# Verify native alpha: vptree reserved.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/alpha_float/0030_vptree_reserved || {
    echo "not ok 1 - native alpha vptree reserved"
    exit 0
}

echo "ok 1 - native alpha vptree reserved"
exit 0
