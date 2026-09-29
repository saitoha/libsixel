#!/bin/sh
# Verify native Heckbert luminance axis.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0013_heckbert_luminance_axis >/dev/null || {
    echo "not ok 1 - native Heckbert luminance axis"
    exit 0
}

echo "ok 1 - native Heckbert luminance axis"
exit 0
