#!/bin/sh
# Verify fixed palette mask: fs feedback.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/fixed_mask/0012_fs_feedback || {
    echo "not ok 1 - fixed palette mask fs feedback"
    exit 0
}

echo "ok 1 - fixed palette mask fs feedback"
exit 0
